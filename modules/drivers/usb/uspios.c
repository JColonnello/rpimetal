#include "arm/irq.h"
#include <assert.h>
#include <drivers/mbox.h>
#include <drivers/timer.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <uspios.h>

//Implement timer and sleep functions through timer.h
void MsDelay(unsigned nMilliSeconds)
{
	timer_millisleep(nMilliSeconds);
}

void usDelay(unsigned nMicroSeconds)
{
	timer_microsleep(nMicroSeconds);
}

typedef struct
{
	void *pParam;
	void *pContext;
	TKernelTimerHandler *pHandler;
} TimerHandlerData;

static void TimerHandler(unsigned hTimer, void *pData)
{
	TimerHandlerData *data = (TimerHandlerData *)pData;
	if (data && data->pHandler)
	{
		data->pHandler(hTimer, data->pParam, data->pContext);
	}
}

unsigned StartKernelTimer(
	unsigned nHzDelay, // in HZ units (see "system configuration" above)
	TKernelTimerHandler *pHandler,
	void *pParam,
	void *pContext
)
{
	TimerHandlerData *data = malloc(sizeof(TimerHandlerData));
	if (!data)
	{
		return 0; // Failed to allocate memory for timer data
	}
	data->pParam = pParam;
	data->pContext = pContext;
	data->pHandler = pHandler;

	// HZ unit is 1000 (a millisecond each), so we convert nHzDelay to microseconds
	return timer_register(nHzDelay * 1000, true, TimerHandler, data);
}

//Implement using timer_set_data to get and free the data
void CancelKernelTimer(unsigned hTimer)
{
	TimerHandlerData *data = NULL;

	if (timer_set_data(hTimer, (void **)&data, NULL))
		free(data);
}

// Implement through irq_register
void ConnectInterrupt(unsigned nIRQ, TInterruptHandler *pHandler, void *pParam)
{
	irq_register(pHandler, pParam, GPU_INTERRUPT1, nIRQ);
}

int SetPowerStateOn(unsigned nDeviceId)
{
	mbox[0] = 8 * 4;
	mbox[1] = MBOX_REQUEST;

	mbox[2] = 0x00028001; // Set power state
	mbox[3] = 0;
	mbox[4] = 8;            // Buffer size
	mbox[5] = nDeviceId;    // Device ID
	mbox[6] = 1 | (1 << 1); // Power state (on) and wait for completion

	mbox[7] = MBOX_TAG_LAST; // End of tags

	mbox_call(MBOX_CH_PROP);
	mbox_wait();

	if ((mbox[6] & 0b11) == 1)
		return 1; // Success, power state set to on
	else
		return 0; // Failure, power state not set to on
}

int GetMACAddress(unsigned char Buffer[6]) // "get board MAC address"

{
	mbox[0] = 6 * 4;
	mbox[1] = MBOX_REQUEST;

	mbox[2] = 0x00010003; // Get MAC address
	mbox[3] = 0;
	mbox[4] = 0; // Buffer size

	mbox[5] = MBOX_TAG_LAST; // End of tags

	mbox_call(MBOX_CH_PROP);
	mbox_wait();

	if (mbox[4] != 6)
		return 0;

	unsigned char *mac = (unsigned char *)&mbox[5];
	memcpy(Buffer, mac, 6);
	return 1;
}

void LogWrite(const char *pSource, unsigned Severity, const char *pMessage, ...)
{
	va_list args;
	va_start(args, pMessage);
	const char *severity_str;
	switch (Severity)
	{
	case LOG_ERROR:
		severity_str = "ERROR";
		break;
	case LOG_WARNING:
		severity_str = "WARNING";
		break;
	case LOG_NOTICE:
		severity_str = "NOTICE";
		break;
	case LOG_DEBUG:
		severity_str = "DEBUG";
		break;
	default:
		severity_str = "UNKNOWN";
		break;
	}
	printf("[%s] %s: ", severity_str, pSource);
	vprintf(pMessage, args);
	va_end(args);
}

void DebugHexdump(const void *pStart, unsigned nBytes, const char *pSource)
{
	uint8_t *pOffset = (uint8_t *)pStart;

	if (pSource == 0)
	{
		pSource = "debug";
	}

	LogWrite(pSource, LOG_DEBUG, "Dumping 0x%X bytes starting at 0x%lX", nBytes, (uintptr_t)pOffset);

	while (nBytes > 0)
	{
		LogWrite(
			pSource,
			LOG_DEBUG,
			"%04X: %02X %02X %02X %02X %02X %02X %02X %02X-%02X %02X %02X %02X %02X %02X %02X %02X",
			(unsigned)((uintptr_t)pOffset & 0xFFFF),
			(unsigned)pOffset[0],
			(unsigned)pOffset[1],
			(unsigned)pOffset[2],
			(unsigned)pOffset[3],
			(unsigned)pOffset[4],
			(unsigned)pOffset[5],
			(unsigned)pOffset[6],
			(unsigned)pOffset[7],
			(unsigned)pOffset[8],
			(unsigned)pOffset[9],
			(unsigned)pOffset[10],
			(unsigned)pOffset[11],
			(unsigned)pOffset[12],
			(unsigned)pOffset[13],
			(unsigned)pOffset[14],
			(unsigned)pOffset[15]
		);

		pOffset += 16;

		if (nBytes >= 16)
		{
			nBytes -= 16;
		}
		else
		{
			nBytes = 0;
		}
	}
}
