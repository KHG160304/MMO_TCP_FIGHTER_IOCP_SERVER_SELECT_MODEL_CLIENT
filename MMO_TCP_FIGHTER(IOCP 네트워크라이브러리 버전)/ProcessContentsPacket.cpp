//#include "Profiler.h"
#include <windows.h>
#include <process.h>
#include <map>

#include "Network.h"
#include "ProcessContentsPacket.h"
#include "GameContentValueSetting.h"
#include "Log.h"
#include "Monitoring.h"
#include "Sector.h"
#include "RingBuffer.h"

#undef PROFILE

static DWORD gCharacterID = 1;
static std::map<SESSIONID, CharacterInfo*> characterList;
static SRWLOCK srwCharacterListLock = SRWLOCK_INIT;
static const wchar_t* dirTable[8] = { L"LL", L"LU", L"UU", L"RU", L"RR", L"RD", L"DD", L"LD" };

/**********************************************
컨텐츠 스레드에서 사용할 변수들
**********************************************/
static int IsUpdateThreadRunning = true;
HANDLE hThreadUpdate;

CharacterInfo* CreateCharacterInfo(SESSIONID sessionID);

size_t GetCharacterCnt(void)
{
	AcquireSRWLockShared(&srwCharacterListLock);
	size_t size = characterList.size();
	ReleaseSRWLockShared(&srwCharacterListLock);
	return size;
}

inline CharacterInfo* FindCharacter(SESSIONID sessionID)
{
	std::map<SESSIONID, CharacterInfo*>::iterator iter = characterList.find(sessionID);
	if (iter == characterList.end())
	{
		return nullptr;
	}
	return iter->second;
}

inline size_t EraseCharacter(SESSIONID sessionID)
{
	return characterList.erase(sessionID);
}

void OnRecvBefore(SESSIONID sessionID)
{
	
}

void OnRecv(SESSIONID sessionID, SerializationBuffer& packet)
{
	AcquireSRWLockShared(&srwCharacterListLock);
	CharacterInfo* ptrCharacter = FindCharacter(sessionID);
	AcquireSRWLockExclusive(&ptrCharacter->srwCharacterLock);
	ReleaseSRWLockShared(&srwCharacterListLock);
	ptrCharacter->lastRecvTime = timeGetTime();
	ReleaseSRWLockExclusive(&ptrCharacter->srwCharacterLock);

	DispatchPacketToContents(sessionID, packet);
}

void OnAccept(SESSIONID sessionID)
{
	CharacterInfo* characInfo = CreateCharacterInfo(sessionID);

	SerializationBuffer sendPacket;
	//MakePacketCreateMyCharacter(&sendPacket, characInfo->characterID, dfPACKET_MOVE_DIR_LL, characInfo->xPos, characInfo->yPos, defCHARACTER_DEFAULT_HP);
	MakePacketCreateMyCharacter(sendPacket, characInfo);
	SendPacket(sessionID, sendPacket);

	ConvertPacketCreateMyCharaterToCreateOtherCharacter(sendPacket);
	AcquireSRWLockExclusive(&srwCharacterListLock);
	SendPacketByAcceptEvent(characInfo, sendPacket);
	characterList.insert({ sessionID, characInfo });
	Sector_AddCharacter(characInfo);
	ReleaseSRWLockExclusive(&srwCharacterListLock);
}

void OnDisconnect(SESSIONID sessionID)
{
	CHARACTERID characterID;
	SerializationBuffer sendPacket;
	CharacterInfo* disconnectCharac;
	AcquireSRWLockExclusive(&srwCharacterListLock);
	disconnectCharac = FindCharacter(sessionID);
	if (disconnectCharac == nullptr)
	{
		ReleaseSRWLockExclusive(&srwCharacterListLock);
		return;
	}
	characterID = disconnectCharac->characterID;
	// 섹터에서 제거
	Sector_RemoveCharacter(disconnectCharac);
	characterList.erase(sessionID);
	AcquireSRWLockExclusive(&disconnectCharac->srwCharacterLock);
	ReleaseSRWLockExclusive(&srwCharacterListLock);
	ReleaseSRWLockExclusive(&disconnectCharac->srwCharacterLock);
	MakePacketDeleteCharacter(sendPacket, characterID);
	SendSectorAround(disconnectCharac, sendPacket, true);
	delete disconnectCharac;
}

bool InitTCPFighterContentThread()
{
	hThreadUpdate = (HANDLE)_beginthreadex(nullptr, 0, UpdateThread, nullptr, false, nullptr);
	if (hThreadUpdate == nullptr)
	{
		_Log(dfLOG_LEVEL_SYSTEM, "Update Thread Create Error code %d", GetLastError());
		return false;
	}

	SetOnRecvBeforeEvent(OnRecvBefore);
	SetOnRecvEvent(OnRecv);
	SetOnAcceptEvent(OnAccept);
	SetOnDisconnectEvent(OnDisconnect);	
	return true;
}

void ExitTCPFighterContentThread()
{
	IsUpdateThreadRunning = false;
	if (WaitForSingleObject(hThreadUpdate, INFINITE) == WAIT_FAILED)
	{
		_Log(dfLOG_LEVEL_SYSTEM, "hThreadUpdate - WaitForSingleObject error code: %d", GetLastError());
	}

	CloseHandle(hThreadUpdate);

	printf("메인 스레드 종료 완료\n");
}

CharacterInfo* CreateCharacterInfo(SESSIONID sessionID)
{
	CharacterInfo* characInfo = new CharacterInfo();
	characInfo->sessionID = sessionID;
	characInfo->characterID = gCharacterID;
	characInfo->hp = defCHARACTER_DEFAULT_HP;
	characInfo->action = INVALID_ACTION;
	characInfo->move8Dir = dfPACKET_MOVE_DIR_LL;
	characInfo->stop2Dir = dfPACKET_MOVE_DIR_LL;
	characInfo->xPos = rand() % dfRANGE_MOVE_RIGHT;
	characInfo->yPos = rand() % dfRANGE_MOVE_BOTTOM;

	characInfo->dwActionTick = 0;

	characInfo->xPos -= (characInfo->xPos % dfSIX_FRAME_X_DISTANCE);
	characInfo->yPos -= (characInfo->yPos % dfSIX_FRAME_Y_DISTANCE);

	SectorPos sectorPos = ConvertWorldPosToSectorPos(characInfo->xPos, characInfo->yPos);
	characInfo->curPos = sectorPos;
	characInfo->oldPos = sectorPos;
	characInfo->lastRecvTime = timeGetTime();
	characInfo->srwCharacterLock = SRWLOCK_INIT;

	gCharacterID = (gCharacterID + 1) % INVALID_CHARACTER_ID;
	return characInfo;
}

bool DispatchPacketToContents(SESSIONID sessionID, SerializationBuffer& tmpRecvPacketBody)
{
	BYTE	byType;
	tmpRecvPacketBody >> byType;
	switch (byType)
	{
	case dfPACKET_CS_MOVE_START:
		return ProcessPacketMoveStart(sessionID, tmpRecvPacketBody);
	case dfPACKET_CS_MOVE_STOP:
		return ProcessPacketMoveStop(sessionID, tmpRecvPacketBody);
	case dfPACKET_CS_ATTACK1:
		return ProcessPacketAttack1(sessionID, tmpRecvPacketBody);
	case dfPACKET_CS_ATTACK2:
		return ProcessPacketAttack2(sessionID, tmpRecvPacketBody);
	case dfPACKET_CS_ATTACK3:
		return ProcessPacketAttack3(sessionID, tmpRecvPacketBody);
	case dfPACKET_CS_ECHO:
		return ProcessPacketEcho(sessionID, tmpRecvPacketBody);
	}
	return false;
}

void MakePacketEcho(SerializationBuffer& packetBuf, DWORD time)
{
	CommonPacketHeader packetHeader;
	packetHeader.byCode = dfPACKET_CODE;
	packetHeader.bySize = sizeof(time);
	packetHeader.byType = dfPACKET_SC_ECHO;

	packetBuf.Enqueue((char*)&packetHeader, sizeof(CommonPacketHeader));
	packetBuf << time;
	//(*packetBuf) << (BYTE)dfPACKET_SC_ECHO << time;
}

void MakePacketSyncXYPos(SerializationBuffer& packetBuf, DWORD id, WORD xPos, WORD yPos)
{
	CommonPacketHeader packetHeader;
	packetHeader.byCode = dfPACKET_CODE;
	packetHeader.bySize = sizeof(id) + sizeof(xPos) + sizeof(yPos);
	packetHeader.byType = dfPACKET_SC_SYNC;

	packetBuf.Enqueue((char*)&packetHeader, sizeof(CommonPacketHeader));
	packetBuf << id << xPos << yPos;
	//(*packetBuf) << (BYTE)dfPACKET_SC_SYNC << id << xPos << yPos;
}

void MakePacketCreateMyCharacter(SerializationBuffer& packetBuf, DWORD id, BYTE stop2Dir, WORD xPos, WORD yPos, BYTE hp)
{
	CommonPacketHeader packetHeader;
	packetHeader.byCode = dfPACKET_CODE;
	packetHeader.bySize = sizeof(id) + sizeof(stop2Dir) + sizeof(xPos) + sizeof(yPos) + sizeof(hp);
	packetHeader.byType = dfPACKET_SC_CREATE_MY_CHARACTER;

	packetBuf.Enqueue((char*)&packetHeader, sizeof(CommonPacketHeader));
	packetBuf << id << stop2Dir << xPos << yPos << hp;
	//(*packetBuf) << (BYTE)dfPACKET_SC_CREATE_MY_CHARACTER << id << stop2Dir << xPos << yPos << hp;
}

void MakePacketCreateMyCharacter(SerializationBuffer& packetBuf, CharacterInfo* charac)
{
	CommonPacketHeader packetHeader;
	packetHeader.byCode = dfPACKET_CODE;
	packetHeader.bySize = sizeof(DWORD) + sizeof(BYTE) + sizeof(WORD) + sizeof(WORD) + sizeof(BYTE);
	packetHeader.byType = dfPACKET_SC_CREATE_MY_CHARACTER;

	packetBuf.Enqueue((char*)&packetHeader, sizeof(CommonPacketHeader));
	packetBuf << charac->characterID << charac->stop2Dir << charac->xPos << charac->yPos << charac->hp;
	//(*packetBuf) << (BYTE)dfPACKET_SC_CREATE_MY_CHARACTER << charac->characterID << charac->stop2Dir << charac->xPos << charac->yPos << charac->hp;
}

void MakePacketCreateOtherCharacter(SerializationBuffer& packetBuf, DWORD id, BYTE stop2Dir, WORD xPos, WORD yPos, BYTE hp)
{
	CommonPacketHeader packetHeader;
	packetHeader.byCode = dfPACKET_CODE;
	packetHeader.bySize = sizeof(id) + sizeof(stop2Dir) + sizeof(xPos) + sizeof(yPos) + sizeof(hp);
	packetHeader.byType = dfPACKET_SC_CREATE_OTHER_CHARACTER;

	packetBuf.Enqueue((char*)&packetHeader, sizeof(CommonPacketHeader));
	packetBuf << id << stop2Dir << xPos << yPos << hp;
	//(*packetBuf) << (BYTE)dfPACKET_SC_CREATE_OTHER_CHARACTER << id << stop2Dir << xPos << yPos << hp;
}

void MakePacketCreateOtherCharacter(SerializationBuffer& packetBuf, CharacterInfo* charac)
{
	CommonPacketHeader packetHeader;
	packetHeader.byCode = dfPACKET_CODE;
	packetHeader.bySize = sizeof(DWORD) + sizeof(BYTE) + sizeof(WORD) + sizeof(WORD) + sizeof(BYTE);
	packetHeader.byType = dfPACKET_SC_CREATE_OTHER_CHARACTER;

	packetBuf.Enqueue((char*)&packetHeader, sizeof(CommonPacketHeader));
	packetBuf << charac->characterID << charac->stop2Dir << charac->xPos << charac->yPos << charac->hp;
	//(*packetBuf) << (BYTE)dfPACKET_SC_CREATE_OTHER_CHARACTER << charac->characterID << charac->stop2Dir << charac->xPos << charac->yPos << charac->hp;
}

void ConvertPacketCreateMyCharaterToCreateOtherCharacter(SerializationBuffer& packetBuf)
{
	((CommonPacketHeader*)packetBuf.GetFrontBufferPtr())->byType = dfPACKET_SC_CREATE_OTHER_CHARACTER;
	//*((BYTE*)packetBuf->GetFrontBufferPtr()) = (BYTE)dfPACKET_SC_CREATE_OTHER_CHARACTER;
}

void MakePacketDeleteCharacter(SerializationBuffer& packetBuf, DWORD id)
{
	CommonPacketHeader packetHeader;
	packetHeader.byCode = dfPACKET_CODE;
	packetHeader.bySize = sizeof(id);
	packetHeader.byType = dfPACKET_SC_DELETE_CHARACTER;

	packetBuf.Enqueue((char*)&packetHeader, sizeof(CommonPacketHeader));
	packetBuf << id;
	//(*packetBuf) << (BYTE)dfPACKET_SC_DELETE_CHARACTER << id;
}

void MakePacketMoveStart(SerializationBuffer& packetBuf, DWORD id, BYTE move8Dir, WORD xPos, WORD yPos)
{
	CommonPacketHeader packetHeader;
	packetHeader.byCode = dfPACKET_CODE;
	packetHeader.bySize = sizeof(id) + sizeof(move8Dir) + sizeof(xPos) + sizeof(yPos);
	packetHeader.byType = dfPACKET_SC_MOVE_START;

	packetBuf.Enqueue((char*)&packetHeader, sizeof(CommonPacketHeader));
	packetBuf << id << move8Dir << xPos << yPos;
	//(*packetBuf) << (BYTE)dfPACKET_SC_MOVE_START << id << move8Dir << xPos << yPos;
}

void MakePacketMoveStart(SerializationBuffer& packetBuf, CharacterInfo* charac)
{
	CommonPacketHeader packetHeader;
	packetHeader.byCode = dfPACKET_CODE;
	packetHeader.bySize = sizeof(DWORD) + sizeof(BYTE) + sizeof(WORD) + sizeof(WORD);
	packetHeader.byType = dfPACKET_SC_MOVE_START;

	packetBuf.Enqueue((char*)&packetHeader, sizeof(CommonPacketHeader));
	packetBuf << charac->characterID << charac->move8Dir << charac->xPos << charac->yPos;
	//(*packetBuf) << (BYTE)dfPACKET_SC_MOVE_START << charac->characterID << charac->move8Dir << charac->xPos << charac->yPos;
}

void MakePacketMoveStop(SerializationBuffer& packetBuf, DWORD id, BYTE dir, WORD xPos, WORD yPos)
{
	CommonPacketHeader packetHeader;
	packetHeader.byCode = dfPACKET_CODE;
	packetHeader.bySize = sizeof(id) + sizeof(dir) + sizeof(xPos) + sizeof(yPos);
	packetHeader.byType = dfPACKET_SC_MOVE_STOP;

	packetBuf.Enqueue((char*)&packetHeader, sizeof(CommonPacketHeader));
	packetBuf << id << dir << xPos << yPos;
	//(*packetBuf) << (BYTE)dfPACKET_SC_MOVE_STOP << id << dir << xPos << yPos;
}

void MakePacketAttack1(SerializationBuffer& packetBuf, DWORD id, BYTE dir, WORD xPos, WORD yPos)
{
	CommonPacketHeader packetHeader;
	packetHeader.byCode = dfPACKET_CODE;
	packetHeader.bySize = sizeof(id) + sizeof(dir) + sizeof(xPos) + sizeof(yPos);
	packetHeader.byType = dfPACKET_SC_ATTACK1;

	packetBuf.Enqueue((char*)&packetHeader, sizeof(CommonPacketHeader));
	packetBuf << id << dir << xPos << yPos;
	//(*packetBuf) << (BYTE)dfPACKET_SC_ATTACK1 << id << dir << xPos << yPos;
}

void MakePacketAttack2(SerializationBuffer& packetBuf, DWORD id, BYTE dir, WORD xPos, WORD yPos)
{
	CommonPacketHeader packetHeader;
	packetHeader.byCode = dfPACKET_CODE;
	packetHeader.bySize = sizeof(id) + sizeof(dir) + sizeof(xPos) + sizeof(yPos);
	packetHeader.byType = dfPACKET_SC_ATTACK2;

	packetBuf.Enqueue((char*)&packetHeader, sizeof(CommonPacketHeader));
	packetBuf << id << dir << xPos << yPos;
	//(*packetBuf) << (BYTE)dfPACKET_SC_ATTACK2 << id << dir << xPos << yPos;
}

void MakePacketAttack3(SerializationBuffer& packetBuf, DWORD id, BYTE dir, WORD xPos, WORD yPos)
{
	CommonPacketHeader packetHeader;
	packetHeader.byCode = dfPACKET_CODE;
	packetHeader.bySize = sizeof(id) + sizeof(dir) + sizeof(xPos) + sizeof(yPos);
	packetHeader.byType = dfPACKET_SC_ATTACK3;

	packetBuf.Enqueue((char*)&packetHeader, sizeof(CommonPacketHeader));
	packetBuf << id << dir << xPos << yPos;
	//(*packetBuf) << (BYTE)dfPACKET_SC_ATTACK3 << id << dir << xPos << yPos;
}

void MakePacketDamage(SerializationBuffer& packetBuf, DWORD attackerID, DWORD damagedID, BYTE damageHP)
{
	CommonPacketHeader packetHeader;
	packetHeader.byCode = dfPACKET_CODE;
	packetHeader.bySize = sizeof(attackerID) + sizeof(damagedID) + sizeof(damageHP);
	packetHeader.byType = dfPACKET_SC_DAMAGE;

	packetBuf.Enqueue((char*)&packetHeader, sizeof(CommonPacketHeader));
	packetBuf << attackerID << damagedID << damageHP;
	//(*packetBuf) << (BYTE)dfPACKET_SC_DAMAGE << attackerID << damagedID << damageHP;
}

bool ProcessPacketMoveStart(SESSIONID sessionID, SerializationBuffer& tmpRecvPacketBody)
{
	SerializationBuffer packetBuf;
	BYTE move8Dir;
	WORD clientXpos;
	WORD clientYpos;
	tmpRecvPacketBody >> move8Dir >> clientXpos >> clientYpos;
	AcquireSRWLockShared(&srwCharacterListLock);
	CharacterInfo* ptrCharacter = FindCharacter(sessionID);
	AcquireSRWLockExclusive(&ptrCharacter->srwCharacterLock);
	ReleaseSRWLockShared(&srwCharacterListLock);
	_Log(dfLOG_LEVEL_DEBUG, "CHARACTER_ID[%d] PACKET MOVE_START [DIR: %s/X: %d/Y: %d]"
		, ptrCharacter->characterID, dirTable[move8Dir], clientXpos, clientYpos);

	if (abs(ptrCharacter->xPos - clientXpos) > dfERROR_RANGE
		|| abs(ptrCharacter->yPos - clientYpos) > dfERROR_RANGE)
	{
		clientXpos = ptrCharacter->xPos;
		clientYpos = ptrCharacter->yPos;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             

		MakePacketSyncXYPos(packetBuf, ptrCharacter->characterID, clientXpos, clientYpos);
		SendSectorAround(ptrCharacter, packetBuf, true);
		packetBuf.ClearBuffer();
	}
	else
	{
		ptrCharacter->xPos = clientXpos;
		ptrCharacter->yPos = clientYpos;
	}

	ptrCharacter->action = move8Dir;
	ptrCharacter->move8Dir = move8Dir;

	switch (move8Dir)
	{
	case dfPACKET_MOVE_DIR_LL:
	case dfPACKET_MOVE_DIR_LU:
		ptrCharacter->stop2Dir = dfPACKET_MOVE_DIR_LL;
		break;
	case dfPACKET_MOVE_DIR_RU:
	case dfPACKET_MOVE_DIR_RR:
	case dfPACKET_MOVE_DIR_RD:
		ptrCharacter->stop2Dir = dfPACKET_MOVE_DIR_RR;
		break;
	case dfPACKET_MOVE_DIR_LD:
		ptrCharacter->stop2Dir = dfPACKET_MOVE_DIR_LL;
		break;
	}
	ReleaseSRWLockExclusive(&ptrCharacter->srwCharacterLock);

	MakePacketMoveStart(packetBuf, ptrCharacter->characterID, move8Dir, clientXpos, clientYpos);
	SendSectorAround(ptrCharacter, packetBuf);

	return true;
}

bool ProcessPacketMoveStop(SESSIONID sessionID, SerializationBuffer& tmpRecvPacketBody)
{
	SerializationBuffer packetBuf;
	BYTE stop2Dir;
	WORD clientXpos;
	WORD clientYpos;

	tmpRecvPacketBody >> stop2Dir >> clientXpos >> clientYpos;
	AcquireSRWLockShared(&srwCharacterListLock);
	CharacterInfo* ptrCharacter = FindCharacter(sessionID);
	AcquireSRWLockExclusive(&ptrCharacter->srwCharacterLock);
	ReleaseSRWLockShared(&srwCharacterListLock);
	_Log(dfLOG_LEVEL_DEBUG, "CHARACTER_ID[%d] PACKET MOVE_STOP [DIR: %s/X: %d/Y: %d]"
		, ptrCharacter->characterID, dirTable[stop2Dir], clientXpos, clientYpos);

	if (abs(ptrCharacter->xPos - clientXpos) > dfERROR_RANGE
		|| abs(ptrCharacter->yPos - clientYpos) > dfERROR_RANGE)
	{
		clientXpos = ptrCharacter->xPos;
		clientYpos = ptrCharacter->yPos;

		MakePacketSyncXYPos(packetBuf, ptrCharacter->characterID, clientXpos, clientYpos);
		SendSectorAround(ptrCharacter, packetBuf, true);
		packetBuf.ClearBuffer();
	}
	else
	{
		ptrCharacter->xPos = clientXpos;
		ptrCharacter->yPos = clientYpos;
	}

	ptrCharacter->action = INVALID_ACTION;
	ptrCharacter->stop2Dir = stop2Dir;
	ReleaseSRWLockExclusive(&ptrCharacter->srwCharacterLock);

	MakePacketMoveStop(packetBuf, ptrCharacter->characterID, stop2Dir, clientXpos, clientYpos);
	SendSectorAround(ptrCharacter, packetBuf);
	return true;
}

bool ProcessPacketAttack1(SESSIONID sessionID, SerializationBuffer& tmpRecvPacketBody)
{
	SerializationBuffer packetBuf;
	BYTE stop2Dir;
	WORD clientXpos;
	WORD clientYpos;

	tmpRecvPacketBody >> stop2Dir >> clientXpos >> clientYpos;
	AcquireSRWLockShared(&srwCharacterListLock);
	CharacterInfo* ptrCharacter = FindCharacter(sessionID);
	AcquireSRWLockExclusive(&ptrCharacter->srwCharacterLock);
	ReleaseSRWLockShared(&srwCharacterListLock);
	if (abs(ptrCharacter->xPos - clientXpos) > dfERROR_RANGE
		|| abs(ptrCharacter->yPos - clientYpos) > dfERROR_RANGE)
	{
		clientXpos = ptrCharacter->xPos;
		clientYpos = ptrCharacter->yPos;

		MakePacketSyncXYPos(packetBuf, ptrCharacter->characterID, clientXpos, clientYpos);
		SendSectorAround(ptrCharacter, packetBuf, true);
		packetBuf.ClearBuffer();
	}
	else
	{
		ptrCharacter->xPos = clientXpos;
		ptrCharacter->yPos = clientYpos;
	}

	ptrCharacter->stop2Dir = stop2Dir;

	MakePacketAttack1(packetBuf, ptrCharacter->characterID, stop2Dir, clientXpos, clientYpos);
	SendSectorAround(ptrCharacter, packetBuf);

	CharacterInfo* damagedCharacter;
	if (SearchCollision(dfATTACK1_RANGE_X, dfATTACK1_RANGE_Y, ptrCharacter, &damagedCharacter))
	{
		packetBuf.ClearBuffer();
		MakePacketDamage(packetBuf, ptrCharacter->characterID, damagedCharacter->characterID, damagedCharacter->hp -= dfATTACK1_DAMAGE);
		SendSectorAround(damagedCharacter, packetBuf, true);
	}
	ReleaseSRWLockExclusive(&ptrCharacter->srwCharacterLock);

	return true;
}

bool ProcessPacketAttack2(SESSIONID sessionID, SerializationBuffer& tmpRecvPacketBody)
{
	SerializationBuffer packetBuf;
	BYTE stop2Dir;
	WORD clientXpos;
	WORD clientYpos;

	tmpRecvPacketBody >> stop2Dir >> clientXpos >> clientYpos;
	AcquireSRWLockShared(&srwCharacterListLock);
	CharacterInfo* ptrCharacter = FindCharacter(sessionID);
	AcquireSRWLockExclusive(&ptrCharacter->srwCharacterLock);
	ReleaseSRWLockShared(&srwCharacterListLock);
	if (abs(ptrCharacter->xPos - clientXpos) > dfERROR_RANGE
		|| abs(ptrCharacter->yPos - clientYpos) > dfERROR_RANGE)
	{
		clientXpos = ptrCharacter->xPos;
		clientYpos = ptrCharacter->yPos;

		MakePacketSyncXYPos(packetBuf, ptrCharacter->characterID, clientXpos, clientYpos);
		SendSectorAround(ptrCharacter, packetBuf, true);
		packetBuf.ClearBuffer();
	}
	else
	{
		ptrCharacter->xPos = clientXpos;
		ptrCharacter->yPos = clientYpos;
	}

	ptrCharacter->stop2Dir = stop2Dir;

	MakePacketAttack2(packetBuf, ptrCharacter->characterID, stop2Dir, clientXpos, clientYpos);
	SendSectorAround(ptrCharacter, packetBuf);

	CharacterInfo* damagedCharacter;
	if (SearchCollision(dfATTACK2_RANGE_X, dfATTACK2_RANGE_Y, ptrCharacter, &damagedCharacter))
	{
		packetBuf.ClearBuffer();
		MakePacketDamage(packetBuf, ptrCharacter->characterID, damagedCharacter->characterID, damagedCharacter->hp -= dfATTACK2_DAMAGE);
		SendSectorAround(damagedCharacter, packetBuf, true);
	}
	ReleaseSRWLockExclusive(&ptrCharacter->srwCharacterLock);
	return true;
}

bool ProcessPacketAttack3(SESSIONID sessionID, SerializationBuffer& tmpRecvPacketBody)
{
	SerializationBuffer packetBuf;
	BYTE stop2Dir;
	WORD clientXpos;
	WORD clientYpos;

	tmpRecvPacketBody >> stop2Dir >> clientXpos >> clientYpos;
	AcquireSRWLockShared(&srwCharacterListLock);
	CharacterInfo* ptrCharacter = FindCharacter(sessionID);
	AcquireSRWLockExclusive(&ptrCharacter->srwCharacterLock);
	ReleaseSRWLockShared(&srwCharacterListLock);
	if (abs(ptrCharacter->xPos - clientXpos) > dfERROR_RANGE
		|| abs(ptrCharacter->yPos - clientYpos) > dfERROR_RANGE)
	{
		clientXpos = ptrCharacter->xPos;
		clientYpos = ptrCharacter->yPos;

		MakePacketSyncXYPos(packetBuf, ptrCharacter->characterID, clientXpos, clientYpos);
		SendSectorAround(ptrCharacter, packetBuf, true);
		packetBuf.ClearBuffer();
	}
	else
	{
		ptrCharacter->xPos = clientXpos;
		ptrCharacter->yPos = clientYpos;
	}
	
	ptrCharacter->stop2Dir = stop2Dir;

	MakePacketAttack3(packetBuf, ptrCharacter->characterID, stop2Dir, clientXpos, clientYpos);
	SendSectorAround(ptrCharacter, packetBuf);

	CharacterInfo* damagedCharacter;
	if (SearchCollision(dfATTACK3_RANGE_X, dfATTACK3_RANGE_Y, ptrCharacter, &damagedCharacter))
	{
		packetBuf.ClearBuffer();
		MakePacketDamage(packetBuf, ptrCharacter->characterID, damagedCharacter->characterID, damagedCharacter->hp -= dfATTACK3_DAMAGE);
		SendSectorAround(damagedCharacter, packetBuf, true);
	}
	ReleaseSRWLockExclusive(&ptrCharacter->srwCharacterLock);
	return true;
}

bool ProcessPacketEcho(SESSIONID sessionID, SerializationBuffer& tmpRecvPacketBody)
{
	SerializationBuffer packetBuf;
	DWORD time;
	tmpRecvPacketBody >> time;
	MakePacketEcho(packetBuf, time);
	SendPacket(sessionID, packetBuf);
	return true;
}

static DWORD startTime = StartMonitor();
unsigned WINAPI UpdateThread(LPVOID args)
{
	_Log(dfLOG_LEVEL_SYSTEM, "UpdateThread 시작");
	bool isUpdate = false;
	DWORD endTime = timeGetTime();
	DWORD intervalTime = endTime - startTime;
	while (IsUpdateThreadRunning)
	{
		CountLoop();
		endTime = timeGetTime();
		intervalTime = endTime - startTime;
		if (intervalTime < INTERVAL_FPS(25))
		{
			continue;
		}
		startTime = endTime - (intervalTime - INTERVAL_FPS(25));
		CountFrame();

		CharacterInfo* ptrCharac;
		AcquireSRWLockShared(&srwCharacterListLock);
		std::map<SESSIONID, CharacterInfo*>::iterator iter = characterList.begin();
		for (; iter != characterList.end();)
		{
			ptrCharac = iter->second;
			++iter;
			AcquireSRWLockExclusive(&ptrCharac->srwCharacterLock);
			if (ptrCharac->hp < 1)
			{
				DisconnectSession(ptrCharac->sessionID);
			}
			else if (endTime > ptrCharac->lastRecvTime && endTime - ptrCharac->lastRecvTime > dfNETWORK_PACKET_RECV_TIMEOUT)
			{
				ptrCharac->dwActionTick = 0x89;
				//_Log(dfLOG_LEVEL_SYSTEM, "endTime: %d, lastRecvTime: %d, TimeOut: %d characID:%d sessionID: %lld", endTime, ptrCharac->lastRecvTime, endTime - ptrCharac->lastRecvTime, ptrCharac->characterID, ptrCharac->sessionID);
				DisconnectSession(ptrCharac->sessionID);
			}
			else if (ptrCharac->action != INVALID_ACTION)
			{
				int xPos = ptrCharac->xPos;
				int yPos = ptrCharac->yPos;

				switch (ptrCharac->action)
				{
				case dfPACKET_MOVE_DIR_LL:
					if (xPos - dfSPEED_PLAYER_X > dfRANGE_MOVE_LEFT)
					{
						ptrCharac->xPos = xPos - dfSPEED_PLAYER_X;
					}
					break;
				case dfPACKET_MOVE_DIR_LU:
					if (xPos - dfSPEED_PLAYER_X > dfRANGE_MOVE_LEFT && yPos - dfSPEED_PLAYER_Y > dfRANGE_MOVE_TOP)
					{
						ptrCharac->xPos = xPos - dfSPEED_PLAYER_X;
						ptrCharac->yPos = yPos - dfSPEED_PLAYER_Y;
					}
					break;
				case dfPACKET_MOVE_DIR_LD:
					if (xPos - dfSPEED_PLAYER_X > dfRANGE_MOVE_LEFT && yPos + dfSPEED_PLAYER_Y < dfRANGE_MOVE_BOTTOM)
					{
						ptrCharac->xPos = xPos - dfSPEED_PLAYER_X;
						ptrCharac->yPos = yPos + dfSPEED_PLAYER_Y;
					}
					break;
				case dfPACKET_MOVE_DIR_UU:
					if (yPos - dfSPEED_PLAYER_Y > dfRANGE_MOVE_TOP)
					{
						ptrCharac->yPos = yPos - dfSPEED_PLAYER_Y;
					}
					break;
				case dfPACKET_MOVE_DIR_RU:
					if (xPos + dfSPEED_PLAYER_X < dfRANGE_MOVE_RIGHT && yPos - dfSPEED_PLAYER_Y > dfRANGE_MOVE_TOP)
					{
						ptrCharac->xPos = xPos + dfSPEED_PLAYER_X;
						ptrCharac->yPos = yPos - dfSPEED_PLAYER_Y;
					}
					break;
				case dfPACKET_MOVE_DIR_RR:
					if (xPos + dfSPEED_PLAYER_X < dfRANGE_MOVE_RIGHT)
					{
						ptrCharac->xPos = xPos + dfSPEED_PLAYER_X;
					}
					break;
				case dfPACKET_MOVE_DIR_RD:
					if (xPos + dfSPEED_PLAYER_X < dfRANGE_MOVE_RIGHT && yPos + dfSPEED_PLAYER_Y < dfRANGE_MOVE_BOTTOM)
					{
						ptrCharac->xPos = xPos + dfSPEED_PLAYER_X;
						ptrCharac->yPos = yPos + dfSPEED_PLAYER_Y;
					}
					break;
				case dfPACKET_MOVE_DIR_DD:
					if (yPos + dfSPEED_PLAYER_Y < dfRANGE_MOVE_BOTTOM)
					{
						ptrCharac->yPos = yPos + dfSPEED_PLAYER_Y;
					}
					break;
				}

				// 섹터에 정보 업데이트
				isUpdate = Sector_UpdateCharacter(ptrCharac);
				ReleaseSRWLockExclusive(&ptrCharac->srwCharacterLock);
				if (isUpdate)
				{
					CharacterSectorUpdatePacket(ptrCharac);
				}
				continue;
			}
			ReleaseSRWLockExclusive(&ptrCharac->srwCharacterLock);
		}
		ReleaseSRWLockShared(&srwCharacterListLock);
	}

	_Log(dfLOG_LEVEL_SYSTEM, "UpdateThread 종료");
	return 0;
}
