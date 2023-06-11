#pragma once
#ifndef __PROCESS_CONTENTS_PACKET_H__
#define	__PROCESS_CONTENTS_PACKET_H__
#include "SerializationBuffer.h"
#include "CharacterInfo.h"

struct CommonPacketHeader
{
	BYTE	byCode;
	BYTE	bySize;
	BYTE	byType;
};

void InitContents(void);

bool InitTCPFighterContentThread();

void ExitTCPFighterContentThread();

bool DispatchPacketToContents(SESSIONID sessionKey, SerializationBuffer& tmpRecvPacketBody);

size_t GetCharacterCnt(void);

void MakePacketEcho(SerializationBuffer& packetBuf, DWORD time);

void MakePacketSyncXYPos(SerializationBuffer& packetBuf, DWORD id, WORD xPos, WORD yPos);

void MakePacketCreateMyCharacter(SerializationBuffer& packetBuf, DWORD id, BYTE stop2Dir, WORD xPos, WORD yPos, BYTE hp);

void MakePacketCreateMyCharacter(SerializationBuffer& packetBuf, CharacterInfo* charac);

void MakePacketCreateOtherCharacter(SerializationBuffer& packetBuf, DWORD id, BYTE stop2Dir, WORD xPos, WORD yPos, BYTE hp);

void MakePacketCreateOtherCharacter(SerializationBuffer& packetBuf, CharacterInfo* charac);

void MakePacketDeleteCharacter(SerializationBuffer& packetBuf, DWORD id);

void MakePacketMoveStart(SerializationBuffer& packetBuf, DWORD id, BYTE move8Dir, WORD xPos, WORD yPos);

void MakePacketMoveStart(SerializationBuffer& packetBuf, CharacterInfo* charac);

void MakePacketMoveStop(SerializationBuffer& packetBuf, DWORD id, BYTE dir, WORD xPos, WORD yPos);

void MakePacketAttack1(SerializationBuffer& packetBuf, DWORD id, BYTE dir, WORD xPos, WORD yPos);

void MakePacketAttack2(SerializationBuffer& packetBuf, DWORD id, BYTE dir, WORD xPos, WORD yPos);

void MakePacketAttack3(SerializationBuffer& packetBuf, DWORD id, BYTE dir, WORD xPos, WORD yPos);

void MakePacketDamage(SerializationBuffer& packetBuf, DWORD attackerID, DWORD damagedID, BYTE damageHP);

void ConvertPacketCreateMyCharaterToCreateOtherCharacter(SerializationBuffer& packetBuf);

bool ProcessPacketMoveStart(SESSIONID sessionID, SerializationBuffer& tmpRecvPacketBody);

bool ProcessPacketMoveStop(SESSIONID sessionID, SerializationBuffer& tmpRecvPacketBody);

bool ProcessPacketAttack1(SESSIONID sessionID, SerializationBuffer& tmpRecvPacketBody);

bool ProcessPacketAttack2(SESSIONID sessionID, SerializationBuffer& tmpRecvPacketBody);

bool ProcessPacketAttack3(SESSIONID sessionID, SerializationBuffer& tmpRecvPacketBody);

bool ProcessPacketEcho(SESSIONID sessionID, SerializationBuffer& tmpRecvPacketBody);

//void Update();
unsigned WINAPI UpdateThread(LPVOID args);
#endif // !PROCESS_CONTENTS_PACKET
