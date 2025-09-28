#include "sys/mux.h"
#include <attrib.h>
#include <dirent.h>
#include <drivers/uart.h>
#include <errno.h>
#include <fcntl.h>
#include <fs/ff.h>
#include <sglib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Configuration defines
#define MAX_OPEN_FDS 128
#define MUX_BUFFER_SIZE 1024

// File descriptor types
typedef enum
{
	FD_TYPE_INVALID,
	FD_TYPE_STDIN,
	FD_TYPE_STDOUT,
	FD_TYPE_STDERR,
	FD_TYPE_FATFS,
	FD_TYPE_MUX
} fd_type_t;

// File descriptor entry
typedef struct fd_entry
{
	int fd;
	fd_type_t type;
	union
	{
		struct
		{
			FIL file;
		} fatfs;
		struct
		{
			int16_t channel;
		} mux;
	} data;
	int flags;
} fd_entry_t;

// Global FD table
static fd_entry_t fd_table[MAX_OPEN_FDS];
static bool fd_initialized = false;

extern const void __end;
static void *curr_break = (void *)&__end;
noreturn extern void proc_hang();

// FatFs error to errno mapping
static int fatfs_to_errno(FRESULT res)
{
	switch (res)
	{
	case FR_OK:
		return 0;
	case FR_DISK_ERR:
		return EIO;
	case FR_INT_ERR:
		return EIO;
	case FR_NOT_READY:
		return ENODEV;
	case FR_NO_FILE:
		return ENOENT;
	case FR_NO_PATH:
		return ENOENT;
	case FR_INVALID_NAME:
		return EINVAL;
	case FR_DENIED:
		return EACCES;
	case FR_EXIST:
		return EEXIST;
	case FR_INVALID_OBJECT:
		return EBADF;
	case FR_WRITE_PROTECTED:
		return EROFS;
	case FR_INVALID_DRIVE:
		return ENODEV;
	case FR_NOT_ENABLED:
		return ENODEV;
	case FR_NO_FILESYSTEM:
		return ENODEV;
	case FR_TIMEOUT:
		return ETIMEDOUT;
	case FR_LOCKED:
		return EBUSY;
	case FR_NOT_ENOUGH_CORE:
		return ENOMEM;
	case FR_TOO_MANY_OPEN_FILES:
		return EMFILE;
	case FR_INVALID_PARAMETER:
		return EINVAL;
	default:
		return EIO;
	}
}

// Convert POSIX open flags to FatFs mode
static BYTE posix_to_fatfs_mode(int flags)
{
	BYTE mode = 0;

	if ((flags & O_ACCMODE) == O_RDONLY)
	{
		mode = FA_READ;
	}
	else if ((flags & O_ACCMODE) == O_WRONLY)
	{
		mode = FA_WRITE;
	}
	else if ((flags & O_ACCMODE) == O_RDWR)
	{
		mode = FA_READ | FA_WRITE;
	}

	if (flags & O_CREAT)
	{
		if (flags & O_EXCL)
		{
			mode |= FA_CREATE_NEW;
		}
		else if (flags & O_TRUNC)
		{
			mode |= FA_CREATE_ALWAYS;
		}
		else
		{
			mode |= FA_OPEN_ALWAYS;
		}
	}

	if (flags & O_APPEND)
	{
		mode |= FA_OPEN_APPEND;
	}

	return mode;
}

// Parse path prefix
typedef enum
{
	PATH_TYPE_INVALID,
	PATH_TYPE_FATFS,
	PATH_TYPE_MUX
} path_type_t;

static path_type_t parse_path_prefix(const char *path, const char **remaining_path, int16_t *channel)
{
	if (!path || !remaining_path)
		return PATH_TYPE_INVALID;

	// Check for "sd:" prefix (case insensitive)
	if (strncasecmp(path, "sd:", 3) == 0)
	{
		*remaining_path = path + 3;
		return PATH_TYPE_FATFS;
	}

	// Check for "channel:" prefix (case insensitive)
	if (strncasecmp(path, "channel:", 8) == 0)
	{
		char *endptr;
		long ch = strtol(path + 8, &endptr, 10);
		if (endptr == path + 8 || *endptr != '\0' || ch > INT16_MAX)
		{
			return PATH_TYPE_INVALID;
		}
		*channel = (int16_t)ch;
		*remaining_path = path + 8;
		return PATH_TYPE_MUX;
	}

	return PATH_TYPE_INVALID;
}

// Find free FD slot
static int find_free_fd(void)
{
	for (int i = 3; i < MAX_OPEN_FDS; i++)
	{
		if (fd_table[i].type == FD_TYPE_INVALID)
		{
			return i;
		}
	}
	return -1;
}

// Initialize FD system
constructor static void init_fd_system(void)
{
	if (fd_initialized)
		return;

	// Initialize FD table
	memset(fd_table, 0, sizeof(fd_table));

	// Reserve stdin, stdout, stderr
	fd_table[0].fd = 0;
	fd_table[0].type = FD_TYPE_STDIN;
	fd_table[0].flags = O_RDONLY;

	fd_table[1].fd = 1;
	fd_table[1].type = FD_TYPE_STDOUT;
	fd_table[1].flags = O_WRONLY;

	fd_table[2].fd = 2;
	fd_table[2].type = FD_TYPE_STDERR;
	fd_table[2].flags = O_WRONLY;

	// Initialize mux channels for stdio
	mux_channel_add(0, MUX_BUFFER_SIZE, false); // stdin/stdout
	mux_channel_add(1, MUX_BUFFER_SIZE, false); // stderr

	fd_initialized = true;
}

// Validate FD
static fd_entry_t *get_fd_entry(int fd)
{
	if (fd < 0 || fd >= MAX_OPEN_FDS || fd_table[fd].type == FD_TYPE_INVALID)
	{
		errno = EBADF;
		return NULL;
	}
	return &fd_table[fd];
}

void *_sbrk(intptr_t increment)
{
	if (increment == 0)
		return curr_break;
	if (curr_break + increment < &__end)
	{
		errno = ENOMEM;
		return (void *)-1;
	}

	void *last_break = curr_break;
	curr_break += increment;
	return last_break;
}

int _write(int fd, const void *buf, size_t count)
{
	if (!fd_initialized)
		init_fd_system();

	fd_entry_t *entry = get_fd_entry(fd);
	if (!entry)
		return -1;

	switch (entry->type)
	{
	case FD_TYPE_STDOUT:
	case FD_TYPE_STDERR:
	{
		int channel = (entry->type == FD_TYPE_STDOUT) ? 0 : 1;
		ssize_t written;
		for (;;)
		{
			written = mux_send(channel, buf, count);
			if (count != 0 && written == 0)
				asm("wfi");
			else
				break;
		}
		return written;
	}

	case FD_TYPE_FATFS:
	{
		UINT bytes_written;
		FRESULT res = f_write(&entry->data.fatfs.file, buf, count, &bytes_written);
		if (res != FR_OK)
		{
			errno = fatfs_to_errno(res);
			return -1;
		}
		return bytes_written;
	}

	case FD_TYPE_MUX:
	{
		size_t written = mux_send(entry->data.mux.channel, buf, count);
		return written;
	}

	default:
		errno = EBADF;
		return -1;
	}
}

int _read(int fd, void *buf, size_t count)
{
	if (!fd_initialized)
		init_fd_system();

	fd_entry_t *entry = get_fd_entry(fd);
	if (!entry)
		return -1;

	switch (entry->type)
	{
	case FD_TYPE_STDIN:
	{
		int channel = 0;
		size_t bytes_read;
		for (;;)
		{
			bytes_read = mux_recv(channel, buf, count);
			if (count != 0 && bytes_read == 0)
				asm("wfi");
			else
				break;
		}
		return bytes_read;
	}

	case FD_TYPE_FATFS:
	{
		UINT bytes_read;
		FRESULT res = f_read(&entry->data.fatfs.file, buf, count, &bytes_read);
		if (res != FR_OK)
		{
			errno = fatfs_to_errno(res);
			return -1;
		}
		return bytes_read;
	}

	case FD_TYPE_MUX:
	{
		size_t bytes_read = mux_recv(entry->data.mux.channel, buf, count);
		return bytes_read;
	}

	default:
		errno = EBADF;
		return -1;
	}
}

int _open(const char *path, int flags, mode_t mode)
{
	if (!fd_initialized)
		init_fd_system();

	if (!path)
	{
		errno = EINVAL;
		return -1;
	}

	const char *remaining_path;
	int16_t channel;
	path_type_t type = parse_path_prefix(path, &remaining_path, &channel);

	if (type == PATH_TYPE_INVALID)
	{
		errno = ENOENT;
		return -1;
	}

	int fd = find_free_fd();
	if (fd == -1)
	{
		errno = EMFILE;
		return -1;
	}

	fd_entry_t *entry = &fd_table[fd];
	entry->fd = fd;
	entry->flags = flags;

	switch (type)
	{
	case PATH_TYPE_FATFS:
	{
		BYTE fatfs_mode = posix_to_fatfs_mode(flags);
		FRESULT res = f_open(&entry->data.fatfs.file, remaining_path, fatfs_mode);
		if (res != FR_OK)
		{
			errno = fatfs_to_errno(res);
			return -1;
		}
		entry->type = FD_TYPE_FATFS;
		break;
	}

	case PATH_TYPE_MUX:
	{
		// For mux channels, we just store the channel number
		// The channel should already exist or be created by the application
		if (!mux_channel_add(channel, MUX_BUFFER_SIZE, false))
		{
			errno = ENOENT;
			return -1;
		}
		entry->data.mux.channel = channel;
		entry->type = FD_TYPE_MUX;
		break;
	}

	default:
		errno = EINVAL;
		return -1;
	}

	return fd;
}

int _close(int fd)
{
	if (!fd_initialized)
		init_fd_system();

	if (fd >= 0 && fd <= 2)
	{
		return 0; // Don't close stdin/stdout/stderr
	}

	fd_entry_t *entry = get_fd_entry(fd);
	if (!entry)
		return -1;

	int result = 0;

	switch (entry->type)
	{
	case FD_TYPE_FATFS:
	{
		FRESULT res = f_close(&entry->data.fatfs.file);
		if (res != FR_OK)
		{
			errno = fatfs_to_errno(res);
			result = -1;
		}
		break;
	}

	case FD_TYPE_MUX:
	{
		// Don't remove the channel, just stop using it
		break;
	}

	default:
		break;
	}

	// Clear the entry
	memset(entry, 0, sizeof(*entry));
	entry->type = FD_TYPE_INVALID;

	return result;
}

off_t _lseek(int fd, off_t offset, int whence)
{
	if (!fd_initialized)
		init_fd_system();

	fd_entry_t *entry = get_fd_entry(fd);
	if (!entry)
		return -1;

	switch (entry->type)
	{
	case FD_TYPE_FATFS:
	{
		FSIZE_t new_pos;

		switch (whence)
		{
		case SEEK_SET:
			new_pos = offset;
			break;
		case SEEK_CUR:
			new_pos = f_tell(&entry->data.fatfs.file) + offset;
			break;
		case SEEK_END:
			new_pos = f_size(&entry->data.fatfs.file) + offset;
			break;
		default:
			errno = EINVAL;
			return -1;
		}

		FRESULT res = f_lseek(&entry->data.fatfs.file, new_pos);
		if (res != FR_OK)
		{
			errno = fatfs_to_errno(res);
			return -1;
		}

		return f_tell(&entry->data.fatfs.file);
	}

	case FD_TYPE_STDIN:
	case FD_TYPE_STDOUT:
	case FD_TYPE_STDERR:
	case FD_TYPE_MUX:
		errno = ESPIPE; // Seek not supported on pipes/character devices
		return -1;

	default:
		errno = EBADF;
		return -1;
	}
}

// Directory operations
DIR *opendir(const char *name)
{
	if (!name)
	{
		errno = EINVAL;
		return NULL;
	}

	const char *remaining_path;
	int16_t channel;
	path_type_t type = parse_path_prefix(name, &remaining_path, &channel);

	if (type != PATH_TYPE_FATFS)
	{
		errno = ENOTDIR;
		return NULL;
	}

	DIR *dirp = malloc(sizeof(DIR) + sizeof(fd_type_t));
	if (!dirp)
	{
		errno = ENOMEM;
		return NULL;
	}

	// Store the type after the DIR structure
	fd_type_t *type_ptr = (fd_type_t *)((char *)dirp + sizeof(DIR));
	*type_ptr = FD_TYPE_FATFS;

	FRESULT res = f_opendir((DIR *)dirp, remaining_path);
	if (res != FR_OK)
	{
		free(dirp);
		errno = fatfs_to_errno(res);
		return NULL;
	}

	return dirp;
}

struct dirent *readdir(DIR *dirp)
{
	if (!dirp)
	{
		errno = EBADF;
		return NULL;
	}

	static struct dirent dirent_buf;
	FILINFO fno;

	FRESULT res = f_readdir((DIR *)dirp, &fno);
	if (res != FR_OK)
	{
		errno = fatfs_to_errno(res);
		return NULL;
	}

	if (fno.fname[0] == 0)
	{
		return NULL; // End of directory
	}

	strncpy(dirent_buf.d_name, fno.fname, sizeof(dirent_buf.d_name) - 1);
	dirent_buf.d_name[sizeof(dirent_buf.d_name) - 1] = '\0';

	return &dirent_buf;
}

int closedir(DIR *dirp)
{
	if (!dirp)
	{
		errno = EBADF;
		return -1;
	}

	FRESULT res = f_closedir((DIR *)dirp);
	free(dirp);

	if (res != FR_OK)
	{
		errno = fatfs_to_errno(res);
		return -1;
	}

	return 0;
}

int _gettimeofday(struct timeval *restrict tp, void *restrict tzp)
{
	tp->tv_sec = 1758682781;
	tp->tv_usec = 0;
	return 0;
}

void undef_func(const char *func)
{
	fprintf(stderr, "Call to undefined function: %s\n", func);
	proc_hang();
}

int _fstat(int fd, struct stat *buf)
{
	if (!fd_initialized)
		init_fd_system();

	fd_entry_t *entry = get_fd_entry(fd);
	if (!entry)
		return -1;

	memset(buf, 0, sizeof(*buf));

	switch (entry->type)
	{
	case FD_TYPE_STDIN:
	case FD_TYPE_STDOUT:
	case FD_TYPE_STDERR:
	case FD_TYPE_MUX:
		buf->st_mode = S_IFCHR;
		break;

	case FD_TYPE_FATFS:
	{
		FILINFO fno;
		FRESULT res = f_stat("", &fno); // Get file info - need to track filename
		if (res == FR_OK)
		{
			buf->st_mode = (fno.fattrib & AM_DIR) ? S_IFDIR : S_IFREG;
			buf->st_size = fno.fsize;
		}
		else
		{
			buf->st_mode = S_IFREG;
			buf->st_size = f_size(&entry->data.fatfs.file);
		}
		break;
	}

	default:
		errno = EBADF;
		return -1;
	}

	return 0;
}

int _isatty(int fd)
{
	if (!fd_initialized)
		init_fd_system();

	fd_entry_t *entry = get_fd_entry(fd);
	if (!entry)
		return 0;

	switch (entry->type)
	{
	case FD_TYPE_STDIN:
	case FD_TYPE_STDOUT:
	case FD_TYPE_STDERR:
	case FD_TYPE_MUX:
		return 1;
	default:
		return 0;
	}
}

extern void noreturn kernel_jump();
void _exit(int status)
{
	if (status == 0)
		kernel_jump();
	else
		proc_hang();
}

long sysconf(int name)
{
	switch (name)
	{
	case _SC_OPEN_MAX:
		return MAX_OPEN_FDS;
	case _SC_PAGESIZE:
		return 4096;
	default:
		printf("Unknown sysconf variable: %d\n", name);
		return 0;
	}
}
