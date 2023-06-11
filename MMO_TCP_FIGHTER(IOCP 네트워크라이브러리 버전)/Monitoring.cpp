#include "Monitoring.h"
#include "Log.h"
#include <windows.h>
#pragma comment(lib, "winmm.lib")

DWORD fps = 0;
unsigned long long loopCnt = 0;
DWORD startMeasurementTime = 0;
size_t(*GetCharacterCnt)(void);
size_t(*GetSessionCnt)(void);
size_t(*GetSectorCharacterCnt)(void);

DWORD StartMonitor()
{
	startMeasurementTime = timeGetTime();
	return startMeasurementTime;
}

void Monitoring()
{
	DWORD endMeasurementTime = timeGetTime();
	DWORD interval = endMeasurementTime - startMeasurementTime;
	static int printCnt = 0;
	if (interval >= 1000)
	{
		if (printCnt == 11)
		{
			printCnt = 0;
			_Log(dfLOG_LEVEL_SYSTEM, "FPS: %d, Loop: %lld, Session Count: %lld, Character Count: %lld, Sector Character Count: %lld"
				, fps, loopCnt, GetSessionCnt(), GetCharacterCnt(), GetSectorCharacterCnt());
		}

		startMeasurementTime = endMeasurementTime - (interval - 1000);
		fps = 0;
		loopCnt = 0;
		printCnt += 1;
	}
}

void CountFrame()
{
	fps += 1;
}

void CountLoop()
{
	loopCnt += 1;
}

void SetCharacterCntHandle(size_t(*size)(void))
{
	GetCharacterCnt = size;
}

void SetSessionCntHandle(size_t(*size)(void))
{
	GetSessionCnt = size;
}

void SetSectorCharacterCntHandle(size_t(*size)(void))
{
	GetSectorCharacterCnt = size;
}
