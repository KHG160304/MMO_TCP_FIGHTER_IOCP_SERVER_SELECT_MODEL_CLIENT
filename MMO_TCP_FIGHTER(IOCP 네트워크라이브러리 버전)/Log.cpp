#include "Log.h"

int __gLogLvl = dfLOG_LEVEL_SYSTEM;
__declspec(thread) wchar_t __gLogBuffer[2048 + 1];
const wchar_t* __strLogLvl[3] = { L"[DEBUG]", L"[ERROR]", L"[SYSTEM]" };

void Log(const wchar_t* strLog, int logLvl)
{
	wprintf_s(L"%s\n", strLog);
}

