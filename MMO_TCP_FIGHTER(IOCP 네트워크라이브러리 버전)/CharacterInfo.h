#pragma once
#ifndef __CHARACTERINFO_H__
#define __CHARACTERINFO_H__

#include <windows.h>
#include "Sector.h"

//#include <winsock2.h>

/*union WorldPos
{
	struct { WORD xPos, yPos; };
	DWORD dwPos;
};*/
typedef unsigned long long	SESSIONID;
typedef unsigned char		BYTE;

struct Session;
struct CharacterInfo
{
	SESSIONID sessionID;
	//Session* ptrSession;
	//SOCKET	socket;
	DWORD	characterID;
	DWORD	dwActionTick;
	WORD	xPos;
	WORD	yPos;
	//WorldPos pos;
	WORD	actionXpos;
	WORD	actionYpos;
	BYTE	stop2Dir;
	BYTE	move8Dir;
	BYTE	action;
	char	hp;

	SectorPos curPos;
	SectorPos oldPos;

	DWORD lastRecvTime;
	SRWLOCK srwCharacterLock;
};

#endif // !__CHARACTERINFO_H__
