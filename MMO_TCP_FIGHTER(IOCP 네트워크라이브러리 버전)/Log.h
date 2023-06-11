#pragma once
#ifndef __LOG_H__
#define __LOG_H__

#include <strsafe.h>
#include <time.h>

#define	dfLOG_LEVEL_DEBUG	0
#define dfLOG_LEVEL_ERROR	1
#define dfLOG_LEVEL_SYSTEM	2

#define __FILENAME__	strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__

#define _Log(logLvl, fmt, ...)											\
do {																	\
	tm timeInfo;														\
	time_t timestamp;													\
	if (logLvl >= __gLogLvl)											\
	{																	\
		timestamp = time(nullptr);										\
		localtime_s(&timeInfo, &timestamp);								\
		StringCchPrintf(__gLogBuffer, 2048								\
			, L"%-8s [%02d/%02d/%04d %02d:%02d:%02d] [%hs:%hs:%d] " fmt	\
			, __strLogLvl[logLvl]										\
			, timeInfo.tm_mon + 1										\
			, timeInfo.tm_mday											\
			, timeInfo.tm_year + 1900									\
			, timeInfo.tm_hour											\
			, timeInfo.tm_min											\
			, timeInfo.tm_sec											\
			, __FILENAME__												\
			, __func__													\
			, __LINE__													\
			, ##__VA_ARGS__);											\
		Log(__gLogBuffer, logLvl);										\
	}\
} while (0)

extern int __gLogLvl;
extern __declspec(thread) wchar_t __gLogBuffer[2048 + 1];
extern const wchar_t* __strLogLvl[3];

void Log(const wchar_t* strLog, int logLvl);
#endif
