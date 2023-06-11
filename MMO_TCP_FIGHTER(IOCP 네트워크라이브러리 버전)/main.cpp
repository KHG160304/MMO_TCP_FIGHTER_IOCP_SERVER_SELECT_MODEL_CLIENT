//#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "Network.h"
#include "RingBuffer.h"
#include "ProcessContentsPacket.h"
#include "Log.h"
#include "Monitoring.h"

#define SERVERPORT	6000
#define IOCP_WORKER_THREAD_CREATE_CNT	0
#define IOCP_WORKER_THREAD_RUNNING_CNT	0

bool isRunning = true;

int main(void)
{
	int key;
	if (!InitTCPFighterContentThread())
	{
		return 0;
	}

	if (!InitNetworkLib(SERVERPORT, IOCP_WORKER_THREAD_CREATE_CNT, IOCP_WORKER_THREAD_RUNNING_CNT))
	{
		ExitTCPFighterContentThread();
		return 0;
	}

	SetCharacterCntHandle(GetCharacterCnt);
	SetSessionCntHandle(GetCurrentSessionCnt);
	SetSectorCharacterCntHandle(GetSectorCharacterCnt);
	//SetDisconnectFromServerCntHandle();

	while (isRunning)
	{
		if (_kbhit())
		{
			key = _getch();
			if (key == 'Q' || key == 'q')
			{
				isRunning = false;
			}
		}

		Monitoring();
	}

	RequestExitNetworkLibThread();
	ExitTCPFighterContentThread();

	return 0;
}


/*unsigned WINAPI UpdateThread(LPVOID args)
{
	int useSize;
	EchoMessage message;
	DWORD result;
	HANDLE arrEvent[2] = { hEventWakeUpdateThread, hEventExitUpdateThread };
	_Log(dfLOG_LEVEL_SYSTEM, "Start UpdateThread");
	for (;;)
	{
		// Update 스레드를 멀티스레드로 돌려보기 위한 코드조각
		//result = WaitForMultipleObjects(2, arrEvent, FALSE, INFINITE);
		//if (result == WAIT_FAILED)
		//{
		//	_Log(dfLOG_LEVEL_SYSTEM, "Event WAIT_FAILED error code: %d", GetLastError());
		//	_Log(dfLOG_LEVEL_SYSTEM, "Exit UpdateThread");
		//	return -1;
		//}
		//else if (result == (WAIT_OBJECT_0 + 1))
		//{
		//	_Log(dfLOG_LEVEL_SYSTEM, "Exit UpdateThread");
		//	return 0;
		//}

		//while (updateThreadQueue.GetUseSize())
		//{
		//	AcquireSRWLockExclusive(&updateThreadQueueLock);
		//	if (!updateThreadQueue.Dequeue((char*)&message, sizeof(message)))
		//	{
		//		ReleaseSRWLockExclusive(&updateThreadQueueLock);
		//		break;
		//	}
		//	SendPacket(message.sessionID, *(message.ptrPayload));
		//	ReleaseSRWLockExclusive(&updateThreadQueueLock);
		//	delete message.ptrPayload;
		//}

		
		//	아래는 Update 스레드가 싱글스레드에서 작동하는 코드이다.
		
		if (!(useSize = updateThreadQueue.GetUseSize()))
		{
			result = WaitForMultipleObjects(2, arrEvent, FALSE, INFINITE);
			if (result == WAIT_FAILED)
			{
				_Log(dfLOG_LEVEL_SYSTEM, "Event WAIT_FAILED error code: %d", GetLastError());
				_Log(dfLOG_LEVEL_SYSTEM, "Exit UpdateThread");
				return -1;
			}
			else if (result == (WAIT_OBJECT_0 + 1))
			{
				_Log(dfLOG_LEVEL_SYSTEM, "Exit UpdateThread");
				return 0;
			}
			useSize = updateThreadQueue.GetUseSize();
		}

		while (useSize)
		{
			updateThreadQueue.Dequeue((char*)&message, sizeof(message));
			SendPacket(message.sessionID, *(message.ptrPayload));
			delete message.ptrPayload;
			useSize -= sizeof(EchoMessage);
		}
	}
}*/
