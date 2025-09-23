#include "diskio.h"
#include <assert.h>
#include <attrib.h>
#include <drivers/sd2.h>
#include <fs/ff.h>

static DSTATUS stat = STA_NOINIT;

weak DSTATUS disk_status(BYTE pdrv)
{
	if (pdrv != 0)
		return STA_NOINIT;
	return stat;
}

weak DSTATUS disk_initialize(BYTE pdrv)
{
	if (pdrv != 0)
		return STA_NOINIT;
	switch (sd_init())
	{
	case SD_OK:
		stat &= ~STA_NOINIT;
		break;
	case SD_TIMEOUT:
		stat = STA_NOINIT | STA_NODISK;
		break;
	case SD_ERROR:
		stat = STA_NOINIT;
		break;
	}
	return stat;
}

weak DRESULT disk_read(
	BYTE pdrv,    /* [IN] Physical drive number */
	BYTE *buff,   /* [OUT] Pointer to the read data buffer */
	LBA_t sector, /* [IN] Start sector number */
	UINT count    /* [IN] Number of sectros to read */
)
{
	if (pdrv != 0)
		return RES_NOTRDY;
	if (stat & STA_NOINIT)
		return RES_NOTRDY;
	if (sd_readblock((unsigned int)sector, buff, count) == 0)
		return RES_ERROR;
	return RES_OK;
}

#if FF_FS_READONLY == 0

weak DRESULT disk_write(
	BYTE pdrv,        /* [IN] Physical drive number */
	const BYTE *buff, /* [IN] Pointer to the data to be written */
	LBA_t sector,     /* [IN] Sector number to write from */
	UINT count        /* [IN] Number of sectors to write */
)
{
	if (pdrv != 0)
		return RES_NOTRDY;
	if (stat & STA_NOINIT)
		return RES_NOTRDY;
	if (stat & STA_PROTECT)
		return RES_WRPRT;
	if (sd_writeblock(buff, sector, count) == 0)
		return RES_ERROR;
	return RES_OK;
}

#endif

weak DRESULT disk_ioctl(
	BYTE pdrv, /* [IN] Drive number */
	BYTE cmd,  /* [IN] Control command code */
	void *buff /* [I/O] Parameter and data buffer */
)
{
	if (pdrv != 0)
		return RES_NOTRDY;
	if (stat & STA_NOINIT)
		return RES_NOTRDY;

	switch (cmd)
	{
	case CTRL_SYNC: // Writes are synced inmediately
		return RES_OK;
	default:
		return RES_PARERR;
	}
}
