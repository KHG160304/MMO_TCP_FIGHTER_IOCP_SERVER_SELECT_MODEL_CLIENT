#define _WINSOCKAPI_
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include "Profiler.h"

#define INVALID_CALL_COUNT	2

class Profile
{
public:
	Profile(const char* tag) : _tag(tag)
	{

	}
	~Profile()
	{

	}
private:
	const char* _tag;
};

struct ProfileSample
{
	long			lFlag;
	wchar_t			szName[64];

	LARGE_INTEGER	iStartTime;

	__int64			iTotalTime;
	__int64			iMinTime[2];
	__int64			iMinTimeCallCount[2];
	__int64			iMaxTime[2];
	__int64			iMaxTimeCallCount[2];
	__int64			iCallCount;
};

static int sampleCnt = 0;
static ProfileSample gProfileSample[64] = { 0 };
static LARGE_INTEGER start;
static LARGE_INTEGER end;

void BeginProfile(const WCHAR* tag)
{
	//-------------------------------------------------------------------
	//프로파일링하려는 구간의 시작을 알리는 함수이다.
	//인자로 받은 태그명을 가지고 gProfileSample 배열을 순회하면서
	//인자로 받은 태그가 등록되어있는지 확인한다.
	//태그명이 존재하지 않으면, gProfileSample의 빈자리에 
	//해당 태그를 등록한다.
	//그리고 QueryPerformanceCounter를 호출하여
	//현재(측정시작 시점)의 타임스탬프 값을 구한다.
	//-------------------------------------------------------------------
	int i;
	for (i = 0; i < sampleCnt; ++i)
	{
		if (gProfileSample[i].lFlag == 1
			&& wcscmp(tag, gProfileSample[i].szName) == 0)
		{
			break;
		}
	}
	if (sampleCnt < 64 && i == sampleCnt)
	{
		gProfileSample[sampleCnt].lFlag = 1;
		wcscpy_s(gProfileSample[sampleCnt].szName, tag);
		sampleCnt += 1;
	}
	QueryPerformanceCounter(&start);
}

void EndProfile(const WCHAR* tag)
{
	//-------------------------------------------------------------------
	//프로파일링하려는 구간의 종료를 알리는 함수이다.
	//인자로 받은 태그명을 가지고 gProfileSample 배열을 순회한다.
	//태그명이 등록된 위치를 찾은 다음에
	//프로파일링하려는 구간의 실행 횟수를 카운트하고
	//측정한 시간값을 총 시간값에 누적시킨다.
	//가장 느린측정값, 가장 빠른 측정값을 저장한다.
	//-------------------------------------------------------------------
	QueryPerformanceCounter(&end);
	int i;
	LONGLONG intervalTime;
	for (i = 0; i < sampleCnt; ++i)
	{
		if (gProfileSample[i].lFlag == 1
			&& wcscmp(tag, gProfileSample[i].szName) == 0)
		{
			intervalTime = (end.QuadPart - start.QuadPart);
			if (gProfileSample[i].iCallCount == 0)
			{
				gProfileSample[i].iMaxTime[0] = intervalTime;
				gProfileSample[i].iMinTime[1] = intervalTime;
			}
			else if (gProfileSample[i].iCallCount == 1)
			{
				if (gProfileSample[i].iMaxTime[0] > intervalTime)
				{
					gProfileSample[i].iMaxTime[1] = intervalTime;
					gProfileSample[i].iMinTime[1] = gProfileSample[i].iMinTime[0];
					gProfileSample[i].iMinTime[0] = intervalTime;
				}
				else if (gProfileSample[i].iMaxTime[0] < intervalTime)
				{
					gProfileSample[i].iMaxTime[1] = gProfileSample[i].iMaxTime[0];
					gProfileSample[i].iMaxTime[0] = intervalTime;
					gProfileSample[i].iMinTime[0] = intervalTime;
				}
				else
				{
					gProfileSample[i].iMaxTime[1] = intervalTime;
					gProfileSample[i].iMinTime[0] = intervalTime;
				}
			}
			else
			{
				if (gProfileSample[i].iMaxTime[0] < intervalTime)
				{
					gProfileSample[i].iMaxTime[0] = intervalTime;
				}
				else if (gProfileSample[i].iMaxTime[1] < intervalTime)
				{
					gProfileSample[i].iMaxTime[1] = intervalTime;
				}
				else if (gProfileSample[i].iMinTime[0] > intervalTime)
				{
					gProfileSample[i].iMinTime[0] = intervalTime;
				}
				else if (gProfileSample[i].iMinTime[1] > intervalTime)
				{
					gProfileSample[i].iMinTime[1] = intervalTime;
				}
			}

			gProfileSample[i].iCallCount += 1;
			gProfileSample[i].iTotalTime += intervalTime;
		}
	}
}

bool SaveProfileSampleToText(const WCHAR* szFileName)
{
	//-----------------------------------------------
	//gProfileSample 배열에 저장된 측정 값들을
	//텍스트 파일로 저장한다.
	//태그명, 평균 측정시간, 최소 측정시간, 최대 측정시간, 측정횟수
	//로 표를 만들어서 저장한다.
	//-----------------------------------------------
	tm TM;
	time_t currentTime;
	time(&currentTime);
	localtime_s(&TM, &currentTime);

	FILE* file = nullptr;
	WCHAR  szTime[15];
	swprintf_s(szTime, 15, L"%04d%02d%02d%02d%02d%02d"
		, TM.tm_year + 1900
		, TM.tm_mon + 1
		, TM.tm_mday
		, TM.tm_hour
		, TM.tm_min
		, TM.tm_sec);
	size_t saveFileNameLen = wcslen(szFileName) + wcslen(szTime) + wcslen(L".txt") + 1;
	WCHAR* _szFileName = (WCHAR*)malloc(sizeof(WCHAR) * saveFileNameLen);
	if (_szFileName == nullptr)
	{
		return false;
	}
	memset(_szFileName, 0, sizeof(WCHAR) * saveFileNameLen);
	wcscat_s(_szFileName, saveFileNameLen, szFileName);
	wcscat_s(_szFileName, saveFileNameLen, szTime);
	wcscat_s(_szFileName, saveFileNameLen, L".txt");
	errno_t err = _wfopen_s(&file, _szFileName, L"w");
	if (err != 0 || file == nullptr)
	{
		free(_szFileName);
		return false;
	}

	static LARGE_INTEGER lpFrequency = { 0 };
	if (lpFrequency.QuadPart == 0)
	{
		QueryPerformanceFrequency(&lpFrequency);
	}

	fprintf_s(file, "%-50s |%15s |%15s |%15s |%13s |\n", "Name", "Average", "Min", "Max", "Call");
	fprintf_s(file, "-----------------------------------------------------------------------------------------------------------------------------------------\n");

	for (int i = 0; i < sampleCnt; ++i)
	{
		fprintf_s(file, "%-50ws |%13.4Lfμs |%13.4Lfμs |%13.4Lfμs |%13lld |\n"
			, gProfileSample[i].szName
			, ((long double)((gProfileSample[i].iTotalTime - gProfileSample[i].iMaxTime[0] - gProfileSample[i].iMaxTime[1] - gProfileSample[i].iMinTime[0] - gProfileSample[i].iMinTime[1]) * 1000000)
				/ (long double)lpFrequency.QuadPart) / ((long double)gProfileSample[i].iCallCount - INVALID_CALL_COUNT)
			, (long double)gProfileSample[i].iMinTime[0] * 1000000 / (long double)lpFrequency.QuadPart
			, (long double)gProfileSample[i].iMaxTime[0] * 1000000 / (long double)lpFrequency.QuadPart
			, gProfileSample[i].iCallCount - INVALID_CALL_COUNT);
	}
	free(_szFileName);
	fclose(file);
	return true;
}