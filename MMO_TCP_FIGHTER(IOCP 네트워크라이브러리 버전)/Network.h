#pragma once
#ifndef __NETWORK_H__
#define	__NETWORK_H__
#include "SerializationBuffer.h"
#define WINAPI	__stdcall

typedef unsigned short		WORD;
typedef unsigned long		DWORD;
typedef	void*				LPVOID;
typedef unsigned long long	SESSIONID;

void SetOnRecvBeforeEvent(void (*_OnRecvBefore)(SESSIONID sessionID));
void SetOnRecvEvent(void (*_OnRecv)(SESSIONID sessionID, SerializationBuffer& packet));
void SetOnAcceptEvent(void (*_OnAccept)(SESSIONID sessionID));
void SetOnDisconnectEvent(void (*_OnDisconnect)(SESSIONID sessionID));
void SendPacket(SESSIONID sessionID, const SerializationBuffer& sendPacket);

void DisconnectSession(SESSIONID sessionID);

size_t GetAcceptTotalCnt();
size_t GetCurrentSessionCnt();

void RequestExitNetworkLibThread(void);
bool InitNetworkLib(WORD port, DWORD createIOCPWorkerThreadCnt = 0, DWORD runningIOCPWorkerThreadCnt = 0);

#endif // !__NETWORK_H__