#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_CHANNELS 256
#define MAX_PACKET_SIZE 256
#define HEADER_SIZE 4

typedef struct
{
	int16_t channel;
	pid_t pid;
	int stdin_fd;
	int stdout_fd;
	char *command;
} channel_info_t;

static channel_info_t channels[MAX_CHANNELS];
static int num_channels = 0;
static volatile int should_exit = 0;

void signal_handler(int sig)
{
	should_exit = 1;
}

void cleanup()
{
	fputs("Exiting\n", stderr);
	for (int i = 0; i < num_channels; i++)
	{
		if (channels[i].pid > 0)
		{
			kill(channels[i].pid, SIGTERM);
			waitpid(channels[i].pid, NULL, 0);
		}
		if (channels[i].stdin_fd >= 0)
			close(channels[i].stdin_fd);
		if (channels[i].stdout_fd >= 0)
			close(channels[i].stdout_fd);
		free(channels[i].command);
	}
}

int parse_config(const char *config_path)
{
	FILE *file = fopen(config_path, "r");
	if (!file)
	{
		perror("Failed to open config file");
		return -1;
	}

	fputs("Config:\n", stderr);
	char line[1024];
	while (fgets(line, sizeof(line), file) && num_channels < MAX_CHANNELS)
	{
		// Remove newline
		line[strcspn(line, "\n")] = 0;

		char *comma = strchr(line, ',');
		if (!comma)
		{
			fprintf(stderr, "Invalid config line: %s\n", line);
			continue;
		}

		*comma = 0;
		int16_t channel = atoi(line);
		char *command = comma + 1;

		channels[num_channels].channel = channel;
		channels[num_channels].command = strdup(command);
		channels[num_channels].pid = -1;
		channels[num_channels].stdin_fd = -1;
		channels[num_channels].stdout_fd = -1;

		fprintf(stderr, "\tChannel %d: %s\n", channel, command);
		num_channels++;
	}

	fclose(file);
	return 0;
}

int spawn_process(int idx)
{
	int stdin_pipe[2], stdout_pipe[2];

	if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1)
	{
		perror("pipe");
		return -1;
	}

	pid_t pid = fork();
	if (pid == -1)
	{
		perror("fork");
		close(stdin_pipe[0]);
		close(stdin_pipe[1]);
		close(stdout_pipe[0]);
		close(stdout_pipe[1]);
		return -1;
	}

	if (pid == 0)
	{
		// Child process
		close(stdin_pipe[1]);  // Close write end
		close(stdout_pipe[0]); // Close read end

		dup2(stdin_pipe[0], STDIN_FILENO);
		dup2(stdout_pipe[1], STDOUT_FILENO);

		close(stdin_pipe[0]);
		close(stdout_pipe[1]);

		execl("/bin/sh", "sh", "-c", channels[idx].command, NULL);
		perror("execl");
		exit(1);
	}

	// Parent process
	close(stdin_pipe[0]);  // Close read end
	close(stdout_pipe[1]); // Close write end

	channels[idx].pid = pid;
	channels[idx].stdin_fd = stdin_pipe[1];
	channels[idx].stdout_fd = stdout_pipe[0];

	return 0;
}

int find_channel(int16_t channel)
{
	for (int i = 0; i < num_channels; i++)
	{
		if (channels[i].channel == channel)
		{
			return i;
		}
	}
	return -1;
}

int handle_stdin_packet()
{
	static uint8_t header[HEADER_SIZE];
	static int bytes_read, count = 0;

	// Read packet header
	while (HEADER_SIZE - count > 0)
	{
		bytes_read = read(STDIN_FILENO, &header[count], HEADER_SIZE - count);
		if (bytes_read <= 0)
		{
			perror("read stdin header");
			return -1; // EOF or error
		}
		count += bytes_read;
	}
	count = 0;

	int16_t channel = *(int16_t *)header;
	uint16_t length = *(uint16_t *)(header + 2);

	// Find channel
	int idx = find_channel(channel);
	if (idx == -1)
	{
		// Unknown channel, skip packet
		fprintf(stderr, "Unknown channel %d, skipping packet\n", channel);
		static char buffer[MAX_PACKET_SIZE];
		uint16_t remaining = length;
		while (remaining > 0)
		{
			int to_read = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
			bytes_read = read(STDIN_FILENO, buffer, to_read);
			if (bytes_read <= 0)
			{
				perror("read stdin skip");
				return -1;
			}
			remaining -= bytes_read;
		}
		return 0;
	}

	fprintf(stderr, "Forwarding packet to channel %d, length %d\n", channel, length);
	// Forward packet data to subprocess
	uint16_t remaining = length;
	char buffer[MAX_PACKET_SIZE];
	while (remaining > 0)
	{
		int to_read = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
		int bytes_read = read(STDIN_FILENO, buffer, to_read);
		if (bytes_read <= 0)
		{
			perror("read stdin");
			return -1;
		}

		if (write(channels[idx].stdin_fd, buffer, bytes_read) != bytes_read)
		{
			perror("write to subprocess");
			return -1;
		}
		remaining -= bytes_read;
	}

	return 0;
}

int handle_subprocess_output(int idx)
{
	char buffer[MAX_PACKET_SIZE];
	int bytes_read = read(channels[idx].stdout_fd, buffer, sizeof(buffer));

	if (bytes_read <= 0)
	{
		// Subprocess closed stdout or error
		perror("read from subprocess");
		return -1;
	}

	// Create packet header
	uint8_t header[HEADER_SIZE];
	*(int16_t *)header = channels[idx].channel;
	*(uint16_t *)(header + 2) = bytes_read;

	fprintf(stderr, "Forwarding packet from channel %d, length %d\n", channels[idx].channel, bytes_read);
	// Write header and data to stdout
	if (write(STDOUT_FILENO, header, HEADER_SIZE) != HEADER_SIZE ||
		write(STDOUT_FILENO, buffer, bytes_read) != bytes_read)
	{
		perror("write to stdout");
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
		return 1;
	}
	fputs("Starting\n", stderr);

	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

	if (parse_config(argv[1]) < 0)
	{
		return 1;
	}

	// Spawn all processes
	for (int i = 0; i < num_channels; i++)
	{
		if (spawn_process(i) < 0)
		{
			fprintf(stderr, "Failed to spawn process for channel %d\n", channels[i].channel);
			cleanup();
			return 1;
		}
	}

	fd_set readfds;
	int max_fd = STDIN_FILENO;

	// Find maximum fd for select
	for (int i = 0; i < num_channels; i++)
	{
		if (channels[i].stdout_fd > max_fd)
		{
			max_fd = channels[i].stdout_fd;
		}
	}

	while (!should_exit)
	{
		FD_ZERO(&readfds);
		FD_SET(STDIN_FILENO, &readfds);

		for (int i = 0; i < num_channels; i++)
		{
			FD_SET(channels[i].stdout_fd, &readfds);
		}

		int result = select(max_fd + 1, &readfds, NULL, NULL, NULL);
		if (result < 0)
		{
			if (errno == EINTR)
				continue;
			perror("select");
			break;
		}

		// Check for stdin data
		if (FD_ISSET(STDIN_FILENO, &readfds))
		{
			if (handle_stdin_packet() < 0)
			{
				break; // EOF or error on stdin
			}
		}

		// Check subprocess outputs
		for (int i = 0; i < num_channels; i++)
		{
			if (FD_ISSET(channels[i].stdout_fd, &readfds))
			{
				if (handle_subprocess_output(i) < 0)
				{
					fprintf(stderr, "Subprocess for channel %d terminated\n", channels[i].channel);
					should_exit = 1;
					break;
				}
			}
		}

		// Check if any subprocess has died
		for (int i = 0; i < num_channels; i++)
		{
			int status;
			if (waitpid(channels[i].pid, &status, WNOHANG) > 0)
			{
				fprintf(stderr, "Subprocess for channel %d exited\n", channels[i].channel);
				should_exit = 1;
				break;
			}
		}
	}

	cleanup();
	return 0;
}
