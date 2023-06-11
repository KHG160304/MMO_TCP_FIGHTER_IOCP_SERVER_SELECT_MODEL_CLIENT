#include "Log.h"
#include "Sector.h"
#include "Network.h"
#include "ProcessContentsPacket.h"

static const int SECTOR_HEIGHT = dfSECTOR_HEIGHT;
static const int SECTOR_WIDTH = dfSECTOR_WIDTH;

std::map<CHARACTERID, CharacterInfo*> sectorList[SECTOR_HEIGHT][SECTOR_WIDTH];
SRWLOCK sectorLockList[SECTOR_HEIGHT][SECTOR_WIDTH] = { SRWLOCK_INIT, };

SectorPos ConvertWorldPosToSectorPos(short worldXPos, short worldYPos)
{
	return {
		(short)(worldXPos / dfSIX_FRAME_X_DISTANCE),
		(short)(worldYPos / dfSIX_FRAME_Y_DISTANCE)
	};
}

void Sector_AddCharacter(CharacterInfo* charac)
{
	AcquireSRWLockExclusive(&sectorLockList[charac->curPos.yPos][charac->curPos.xPos]);
	sectorList[charac->curPos.yPos][charac->curPos.xPos].insert({ charac->characterID, charac });
	ReleaseSRWLockExclusive(&sectorLockList[charac->curPos.yPos][charac->curPos.xPos]);
}

void Sector_RemoveCharacter(CharacterInfo* charac)
{
	AcquireSRWLockExclusive(&sectorLockList[charac->curPos.yPos][charac->curPos.xPos]);
	sectorList[charac->curPos.yPos][charac->curPos.xPos].erase(charac->characterID);
	ReleaseSRWLockExclusive(&sectorLockList[charac->curPos.yPos][charac->curPos.xPos]);
}

bool Sector_UpdateCharacter(CharacterInfo* charac)
{
	SectorPos curPos = ConvertWorldPosToSectorPos(charac->xPos, charac->yPos);
	if (curPos.yPos >= SECTOR_HEIGHT || curPos.xPos >= SECTOR_WIDTH)
	{
		DisconnectSession(charac->sessionID);
		return false;
	}

	if (*((DWORD*)&curPos) == *((DWORD*)&(charac->curPos)))
	{
		return false;
	}

	charac->oldPos = charac->curPos;
	Sector_RemoveCharacter(charac);

	charac->curPos = curPos;
	Sector_AddCharacter(charac);

	return true;
}

size_t GetSectorCharacterCnt(void)
{
	size_t cnt = 0;
	for (int y = 0; y < dfSECTOR_HEIGHT; ++y)
	{
		for (int x = 0; x < dfSECTOR_WIDTH; ++x)
		{
			AcquireSRWLockShared(&sectorLockList[y][x]);
			cnt += sectorList[y][x].size();
			ReleaseSRWLockShared(&sectorLockList[y][x]);
		}
	}
	return cnt;
}

void GetSectorAround(SectorPos findSector, SectorAround* pSectorAround, bool includeFindSector)
{

	short endYPos = findSector.yPos + 1;
	short endXPos = findSector.xPos + 1;
	short y, x;

	if (includeFindSector == false)
	{
		int idx = 0;
		for (y = endYPos - 2; y <= endYPos; ++y)
		{
			if (y < 0 || y >= dfSECTOR_HEIGHT)
			{
				continue;
			}
			for (x = endXPos - 2; x <= endXPos; ++x)
			{
				if (x < 0 || x >= dfSECTOR_WIDTH)
				{
					continue;
				}

				if (findSector.xPos == x && findSector.yPos == y)
				{
					continue;
				}

				pSectorAround->around[idx++] = { x, y };
			}
		}
		pSectorAround->cnt = idx;

		return;
	}

	int idx = 0;
	for (y = endYPos - 2; y <= endYPos; ++y)
	{
		if (y < 0 || y >= dfSECTOR_HEIGHT)
		{
			continue;
		}
		for (x = endXPos - 2; x <= endXPos; ++x)
		{
			if (x < 0 || x >= dfSECTOR_WIDTH)
			{
				continue;
			}

			pSectorAround->around[idx++] = { x, y };
		}
	}

	pSectorAround->cnt = idx;
}

void GetUpdateSectorAround(CharacterInfo* ptrCharac, SectorAround* ptrRemoveSectors, SectorAround* ptrAddSectors)
{
	int idxOld;
	int idxCur;
	bool isFind;
	int removeSectorsCnt = 0;
	int addSectorsCnt = 0;

	SectorAround oldSectors;
	SectorAround curSectors;
	GetSectorAround(ptrCharac->oldPos, &oldSectors);
	GetSectorAround(ptrCharac->curPos, &curSectors);

	for (idxOld = 0; idxOld < oldSectors.cnt; ++idxOld)
	{
		isFind = false;
		for (idxCur = 0; idxCur < curSectors.cnt; ++idxCur)
		{
			if (*((DWORD*)(oldSectors.around + idxOld)) == *((DWORD*)(curSectors.around + idxCur)))
			{
				isFind = true;
				break;
			}
		}

		if (isFind == false)
		{
			ptrRemoveSectors->around[removeSectorsCnt++] = oldSectors.around[idxOld];
		}
	}
	ptrRemoveSectors->cnt = removeSectorsCnt;

	for (idxCur = 0; idxCur < curSectors.cnt; ++idxCur)
	{
		isFind = false;
		for (idxOld = 0; idxOld < oldSectors.cnt; ++idxOld)
		{
			if (*((DWORD*)(curSectors.around + idxCur)) == *((DWORD*)(oldSectors.around + idxOld)))
			{
				isFind = true;
				break;
			}
		}

		if (isFind == false)
		{
			ptrAddSectors->around[addSectorsCnt++] = curSectors.around[idxCur];
		}
	}
	ptrAddSectors->cnt = addSectorsCnt;
}

void CharacterSectorUpdatePacket(CharacterInfo* ptrMyCharac)
{
	SerializationBuffer packetBuf;

	SectorAround removeSectors;
	SectorAround addSectors;

	int idxRemoveSectors;
	int idxAddSectors;

	SectorPos removeSectorPos;
	SectorPos addSectorPos;

	CharacterInfo* addSecterCharac;

	GetUpdateSectorAround(ptrMyCharac, &removeSectors, &addSectors);
	CHARACTERID myCharacterID = ptrMyCharac->characterID;
	/*
		removeSectors에 존재하는 캐릭터들에게,
		ptrCharac의 정보를 화면에서 지우라고 함
	*/
	MakePacketDeleteCharacter(packetBuf, myCharacterID);
	for (idxRemoveSectors = 0; idxRemoveSectors < removeSectors.cnt; ++idxRemoveSectors)
	{
		SendUnicastSector(removeSectors.around[idxRemoveSectors], packetBuf, myCharacterID);
	}

	/*
		ptrCharac에개 존재하는 캐릭터들에게,
		removeSectors의 정보를 화면에서 지우라고 함
	*/
	for (idxRemoveSectors = 0; idxRemoveSectors < removeSectors.cnt; ++idxRemoveSectors)
	{
		removeSectorPos = removeSectors.around[idxRemoveSectors];
		std::map<CHARACTERID, CharacterInfo*>& removeCharacterMap = sectorList[removeSectorPos.yPos][removeSectorPos.xPos];
		AcquireSRWLockShared(&sectorLockList[removeSectorPos.yPos][removeSectorPos.xPos]);
		std::map<CHARACTERID, CharacterInfo*>::iterator iter = removeCharacterMap.begin();
		for (; iter != removeCharacterMap.end(); ++iter)
		{
			if (iter->first == myCharacterID)
			{
				continue;
			}
			packetBuf.ClearBuffer();
			MakePacketDeleteCharacter(packetBuf, iter->first);
			SendPacket(ptrMyCharac->sessionID, packetBuf);
		}
		ReleaseSRWLockShared(&sectorLockList[removeSectorPos.yPos][removeSectorPos.xPos]);
	}

	/*
		이동 중 새로 보이는 시야에 존재하는 캐릭터들에게
		이동 하는 캐릭터의 정보를 전송;
	*/
	packetBuf.ClearBuffer();

	//MakePacketCreateOtherCharacter(&packetBuf, ptrCharac->characterID
	//	, ptrCharac->stop2Dir, ptrCharac->xPos, ptrCharac->yPos, ptrCharac->hp);
	MakePacketCreateOtherCharacter(packetBuf, ptrMyCharac);
	//MakePacketMoveStart(&packetBuf, ptrCharac->characterID, ptrCharac->move8Dir, ptrCharac->xPos, ptrCharac->yPos);
	MakePacketMoveStart(packetBuf, ptrMyCharac);
	for (idxAddSectors = 0; idxAddSectors < addSectors.cnt; ++idxAddSectors)
	{
		SendUnicastSector(addSectors.around[idxAddSectors], packetBuf, myCharacterID);
	}

	/*
		새로 보이는 시야에 있는 캐릭터 들의 정보를 내게 가져온다.
		이동 하는 캐릭터의 정보를 전송;
	*/
	for (idxAddSectors = 0; idxAddSectors < addSectors.cnt; ++idxAddSectors)
	{
		addSectorPos = addSectors.around[idxAddSectors];
		std::map<DWORD, CharacterInfo*>& addCharacterMap = sectorList[addSectorPos.yPos][addSectorPos.xPos];
		AcquireSRWLockShared(&sectorLockList[addSectorPos.yPos][addSectorPos.xPos]);
		std::map<DWORD, CharacterInfo*>::iterator iter = addCharacterMap.begin();
		for (; iter != addCharacterMap.end(); ++iter)
		{
			if (iter->first == myCharacterID)
			{
				continue;
			}
			packetBuf.ClearBuffer();


			addSecterCharac = iter->second;
			MakePacketCreateOtherCharacter(packetBuf, addSecterCharac);
			//MakePacketCreateOtherCharacter(&packetBuf, addSecterCharac->characterID
			//	, addSecterCharac->stop2Dir, addSecterCharac->xPos, addSecterCharac->yPos, addSecterCharac->hp);

			switch (addSecterCharac->action)
			{
			case dfPACKET_MOVE_DIR_LL:
			case dfPACKET_MOVE_DIR_LU:
			case dfPACKET_MOVE_DIR_UU:
			case dfPACKET_MOVE_DIR_RU:
			case dfPACKET_MOVE_DIR_RR:
			case dfPACKET_MOVE_DIR_RD:
			case dfPACKET_MOVE_DIR_DD:
			case dfPACKET_MOVE_DIR_LD:
				//MakePacketMoveStart(&packetBuf, addSecterCharac->characterID, addSecterCharac->move8Dir, addSecterCharac->xPos, addSecterCharac->yPos);
				MakePacketMoveStart(packetBuf, addSecterCharac);
			}

			//SendUnicast(ptrMyCharac->ptrSession, packetBuf.GetFrontBufferPtr(), packetBuf.GetUseSize());
			SendPacket(ptrMyCharac->sessionID, packetBuf);
		}
		ReleaseSRWLockShared(&sectorLockList[addSectorPos.yPos][addSectorPos.xPos]);
	}
}

/*void SendUnicastSector(SectorPos target, const char* buf, int size, DWORD excludeCharacterID)
{
	if (excludeCharacterID != INVALID_CHARACTER_ID)
	{
		std::map<CHARACTERID, CharacterInfo*>::iterator iter = sectorList[target.yPos][target.xPos].begin();
		for (; iter != sectorList[target.yPos][target.xPos].end(); ++iter)
		{
			if (excludeCharacterID != iter->first)
			{
				SendUnicast(iter->second->ptrSession, buf, size);
			}
		}
		return;
	}

	std::map<CHARACTERID, CharacterInfo*>::iterator iter = sectorList[target.yPos][target.xPos].begin();
	for (; iter != sectorList[target.yPos][target.xPos].end(); ++iter)
	{
		SendUnicast(iter->second->ptrSession, buf, size);
	}
}*/

void SendUnicastSector(SectorPos target, const SerializationBuffer& sendPacket, DWORD excludeCharacterID)
{
	if (excludeCharacterID != INVALID_CHARACTER_ID)
	{
		AcquireSRWLockShared(&sectorLockList[target.yPos][target.xPos]);
		std::map<CHARACTERID, CharacterInfo*>::iterator iter = sectorList[target.yPos][target.xPos].begin();
		for (; iter != sectorList[target.yPos][target.xPos].end(); ++iter)
		{
			if (excludeCharacterID != iter->first)
			{
				//SendUnicast(iter->second->ptrSession, buf, size);
				SendPacket(iter->second->sessionID, sendPacket);
			}
		}
		ReleaseSRWLockShared(&sectorLockList[target.yPos][target.xPos]);
		return;
	}

	AcquireSRWLockShared(&sectorLockList[target.yPos][target.xPos]);
	std::map<CHARACTERID, CharacterInfo*>::iterator iter = sectorList[target.yPos][target.xPos].begin();
	for (; iter != sectorList[target.yPos][target.xPos].end(); ++iter)
	{
		//SendUnicast(iter->second->ptrSession, buf, size);
		SendPacket(iter->second->sessionID, sendPacket);
	}
	ReleaseSRWLockShared(&sectorLockList[target.yPos][target.xPos]);
}

/*void SendSectorAround(CharacterInfo* ptrCharac, const char* buf, int size, bool includeMe)
{
	SectorAround sendTargetSectors;
	GetSectorAround(ptrCharac->curPos, &sendTargetSectors, false);

	int idxSectors;
	for (idxSectors = 0; idxSectors < sendTargetSectors.cnt; ++idxSectors)
	{
		SendUnicastSector(sendTargetSectors.around[idxSectors], buf, size);
	}

	if (includeMe == false)
	{
		SendUnicastSector(ptrCharac->curPos, buf, size, ptrCharac->characterID);
	}
	else
	{
		SendUnicastSector(ptrCharac->curPos, buf, size);
	}
}*/

/*void SendSectorAround(CharacterInfo* ptrCharac, const char* buf, int size, bool includeMe)
{
	SectorAround sendTargetSectors;
	GetSectorAround(ptrCharac->curPos, &sendTargetSectors, false);

	int idxSectors;
	for (idxSectors = 0; idxSectors < sendTargetSectors.cnt; ++idxSectors)
	{
		SendUnicastSector(sendTargetSectors.around[idxSectors], buf, size);
	}

	if (includeMe == false)
	{
		SendUnicastSector(ptrCharac->curPos, buf, size, ptrCharac->characterID);
	}
	else
	{
		SendUnicastSector(ptrCharac->curPos, buf, size);
	}
}*/

void SendSectorAround(CharacterInfo* ptrCharac, const SerializationBuffer& sendPacket, bool includeMe)
{
	SectorAround sendTargetSectors;
	GetSectorAround(ptrCharac->curPos, &sendTargetSectors, false);

	int idxSectors;
	for (idxSectors = 0; idxSectors < sendTargetSectors.cnt; ++idxSectors)
	{
		SendUnicastSector(sendTargetSectors.around[idxSectors], sendPacket);
	}

	if (includeMe == false)
	{
		SendUnicastSector(ptrCharac->curPos, sendPacket, ptrCharac->characterID);
	}
	else
	{
		SendUnicastSector(ptrCharac->curPos, sendPacket);
	}
}

/*void SendPacketByAcceptEvent(CharacterInfo* ptrCharac, const char* buf, int size)
{
	SerializationBuffer sendPacket;
	SectorAround sendTargetSectors;
	CharacterInfo* ptrOtherCharac;
	GetSectorAround(ptrCharac->curPos, &sendTargetSectors);

	// 내정보를 주변 섹터에 뿌린다.
	int idxSectors;
	for (idxSectors = 0; idxSectors < sendTargetSectors.cnt; ++idxSectors)
	{
		SendUnicastSector(sendTargetSectors.around[idxSectors], buf, size);
	}
	// 주변 섹터정보를 모두 나에게 전송한다.
	for (idxSectors = 0; idxSectors < sendTargetSectors.cnt; ++idxSectors)
	{
		std::map<DWORD, CharacterInfo*>& sector = sectorList[sendTargetSectors.around[idxSectors].yPos][sendTargetSectors.around[idxSectors].xPos];
		std::map<DWORD, CharacterInfo*>::iterator iter = sector.begin();
		for (; iter != sector.end(); ++iter)
		{
			ptrOtherCharac = iter->second;
			MakePacketCreateOtherCharacter(&sendPacket, ptrOtherCharac);
			if (ptrOtherCharac->action != INVALID_ACTION)
			{
				MakePacketMoveStart(&sendPacket, ptrOtherCharac);
			}
			//SendUnicast(ptrCharac->ptrSession, sendPacket.GetFrontBufferPtr(), sendPacket.GetUseSize());
			SendPacket(ptrCharac->sessionID, sendPacket);
			sendPacket.ClearBuffer();
		}
	}
}*/

void SendPacketByAcceptEvent(CharacterInfo* ptrCharac, const SerializationBuffer& myCharacInfoPacket)
{
	SerializationBuffer otherCharacInfoPacket;
	SectorAround sendTargetSectors;
	CharacterInfo* ptrOtherCharac;
	GetSectorAround(ptrCharac->curPos, &sendTargetSectors);

	// 내정보를 주변 섹터에 뿌린다.
	int idxSectors;
	for (idxSectors = 0; idxSectors < sendTargetSectors.cnt; ++idxSectors)
	{
		SendUnicastSector(sendTargetSectors.around[idxSectors], myCharacInfoPacket);
	}
	// 주변 섹터정보를 모두 나에게 전송한다.
	for (idxSectors = 0; idxSectors < sendTargetSectors.cnt; ++idxSectors)
	{
		std::map<DWORD, CharacterInfo*>& sector = sectorList[sendTargetSectors.around[idxSectors].yPos][sendTargetSectors.around[idxSectors].xPos];
		AcquireSRWLockShared(&sectorLockList[sendTargetSectors.around[idxSectors].yPos][sendTargetSectors.around[idxSectors].xPos]);
		std::map<DWORD, CharacterInfo*>::iterator iter = sector.begin();
		for (; iter != sector.end(); ++iter)
		{
			ptrOtherCharac = iter->second;
			// 이 함수는 OnAccept에서 호출하고 있다.
			// 여기서는 다른 세션의 캐릭터를 조회하는데, 다른스레드에서 
			// 지금 조회하려는 캐릭터 변경이 발생할 수 있기때문에 락을 건다.
			AcquireSRWLockShared(&ptrOtherCharac->srwCharacterLock);
			MakePacketCreateOtherCharacter(otherCharacInfoPacket, ptrOtherCharac);
			if (ptrOtherCharac->action != INVALID_ACTION)
			{
				MakePacketMoveStart(otherCharacInfoPacket, ptrOtherCharac);
			}
			ReleaseSRWLockShared(&ptrOtherCharac->srwCharacterLock);
			SendPacket(ptrCharac->sessionID, otherCharacInfoPacket);
			otherCharacInfoPacket.ClearBuffer();
		}
		ReleaseSRWLockShared(&sectorLockList[sendTargetSectors.around[idxSectors].yPos][sendTargetSectors.around[idxSectors].xPos]);
	}
}

bool SearchCollision(int attackXRange, int attackYRange, const CharacterInfo* characterOnAttack, CharacterInfo** outCharacterIDOnDamage)
{
	CharacterInfo* charac;
	SectorPos posOnAttacker = characterOnAttack->curPos;
	int attackerXPos = characterOnAttack->xPos;
	int attackerYPos = characterOnAttack->yPos;
	BYTE attackerStop2Dir = characterOnAttack->stop2Dir;
	DWORD attackerCharacID = characterOnAttack->characterID;

	int tmpDistanceX;
	int tmpDistanceY;
	int minDistanceX = attackXRange;
	int minDistanceY = attackYRange;

	int tmpX;
	int cnt;
	CharacterInfo* targetCharacter = nullptr;

	if (attackerStop2Dir == dfPACKET_MOVE_DIR_LL)
	{
		for (tmpX = posOnAttacker.xPos, cnt = 2; targetCharacter == nullptr && tmpX > -1 && cnt != 0; --tmpX, --cnt)
		{
			std::map<DWORD, CharacterInfo*> characterList = sectorList[posOnAttacker.yPos][tmpX];
			AcquireSRWLockShared(&sectorLockList[posOnAttacker.yPos][tmpX]);
			std::map<DWORD, CharacterInfo*>::iterator iter = characterList.begin();
			for (; iter != characterList.end(); ++iter)
			{
				charac = iter->second;
				if (charac->characterID == attackerCharacID)
				{
					continue;
				}
				// 여기서 캐릭터에 락을 걸지 않는 이유는, 
				// 이 함수는 지금 OnRecv를 타고들어와서 받은 패킷을 처리하는
				// 함수에서 호출되는데 거기서 이미 캐릭터에 락을 걸고 있다.
				// 여기서 또 걸면 무조건 데드락이다.
				tmpDistanceX = attackerXPos - charac->xPos;
				tmpDistanceY = abs(charac->yPos - attackerYPos);

				if ((tmpDistanceX >= 0 && tmpDistanceX <= attackXRange) && tmpDistanceY <= attackYRange)
				{
					if ((tmpDistanceX < minDistanceX)
						|| (tmpDistanceX == minDistanceX && tmpDistanceY < minDistanceY))
					{
						minDistanceX = tmpDistanceX;
						minDistanceY = tmpDistanceY;
						targetCharacter = charac;
					}
				}
			}
			ReleaseSRWLockShared(&sectorLockList[posOnAttacker.yPos][tmpX]);

			if (targetCharacter != nullptr)
			{
				*outCharacterIDOnDamage = targetCharacter;
				return true;
			}
		}

		int tmpYUp = ((attackerYPos - attackYRange) / dfSIX_FRAME_Y_DISTANCE);
		int tmpYDown = ((attackerYPos + attackYRange) / dfSIX_FRAME_Y_DISTANCE);
		int searchEndY = -1;

		if (tmpYUp > -1 && tmpYUp == posOnAttacker.yPos - 1)
		{
			searchEndY = tmpYUp;
		}
		else if (tmpYDown < dfSECTOR_HEIGHT&& tmpYDown == posOnAttacker.yPos + 1)
		{
			searchEndY = tmpYDown;
		}

		if (searchEndY != -1)
		{
			for (tmpX = posOnAttacker.xPos, cnt = 2; targetCharacter == nullptr && tmpX > -1 && cnt != 0; --tmpX, --cnt)
			{
				std::map<DWORD, CharacterInfo*> characterList = sectorList[searchEndY][tmpX];
				AcquireSRWLockShared(&sectorLockList[searchEndY][tmpX]);
				std::map<DWORD, CharacterInfo*>::iterator iter = characterList.begin();
				for (; iter != characterList.end(); ++iter)
				{
					charac = iter->second;
					if (charac->characterID == attackerCharacID)
					{
						continue;
					}
					// 여기서 캐릭터에 락을 걸지 않는 이유는, 
					// 이 함수는 지금 특정세션이 OnRecv를 타고들어와서 받은 패킷을 처리하는
					// 함수에서 호출되는데 거기서 이미 캐릭터에 락을 걸고 있다.
					// 여기서 또 걸면 무조건 데드락이다.
					tmpDistanceX = attackerXPos - charac->xPos;
					tmpDistanceY = abs(charac->yPos - attackerYPos);

					if ((tmpDistanceX >= 0 && tmpDistanceX <= attackXRange) && tmpDistanceY <= attackYRange)
					{
						if ((tmpDistanceX < minDistanceX)
							|| (tmpDistanceX == minDistanceX && tmpDistanceY < minDistanceY))
						{
							minDistanceX = tmpDistanceX;
							minDistanceY = tmpDistanceY;
							targetCharacter = charac;
						}
					}
				}
				ReleaseSRWLockShared(&sectorLockList[searchEndY][tmpX]);

				if (targetCharacter != nullptr)
				{
					*outCharacterIDOnDamage = targetCharacter;
					return true;
				}
			}
		}

		return false;
	}
	else if (attackerStop2Dir == dfPACKET_MOVE_DIR_RR)
	{
		for (tmpX = posOnAttacker.xPos, cnt = 2; tmpX < dfSECTOR_WIDTH && cnt != 0; ++tmpX, --cnt)
		{
			std::map<DWORD, CharacterInfo*> characterList = sectorList[posOnAttacker.yPos][tmpX];
			AcquireSRWLockShared(&sectorLockList[posOnAttacker.yPos][tmpX]);
			std::map<DWORD, CharacterInfo*>::iterator iter = characterList.begin();
			for (; iter != characterList.end(); ++iter)
			{
				charac = iter->second;
				if (charac->characterID == attackerCharacID)
				{
					continue;
				}
				// 여기서 캐릭터에 락을 걸지 않는 이유는, 
				// 이 함수는 지금 OnRecv를 타고들어와서 받은 패킷을 처리하는
				// 함수에서 호출되는데 거기서 이미 캐릭터에 락을 걸고 있다.
				// 여기서 또 걸면 무조건 데드락이다.
				tmpDistanceX = charac->xPos - attackerXPos;
				tmpDistanceY = abs(charac->yPos - attackerYPos);

				if ((tmpDistanceX >= 0 && tmpDistanceX <= attackXRange) && tmpDistanceY <= attackYRange)
				{
					if ((tmpDistanceX < minDistanceX)
						|| (tmpDistanceX == minDistanceX && tmpDistanceY < minDistanceY))
					{
						minDistanceX = tmpDistanceX;
						minDistanceY = tmpDistanceY;
						targetCharacter = charac;
					}
				}
			}
			ReleaseSRWLockShared(&sectorLockList[posOnAttacker.yPos][tmpX]);

			if (targetCharacter != nullptr)
			{
				*outCharacterIDOnDamage = targetCharacter;
				return true;
			}
		}

		int tmpYUp = ((attackerYPos - attackYRange) / dfSIX_FRAME_Y_DISTANCE);
		int tmpYDown = ((attackerYPos + attackYRange) / dfSIX_FRAME_Y_DISTANCE);
		int searchEndY = -1;

		if (tmpYUp > -1 && tmpYUp == posOnAttacker.yPos - 1)
		{
			searchEndY = tmpYUp;
		}
		else if (tmpYDown < dfSECTOR_HEIGHT && tmpYDown == posOnAttacker.yPos + 1)
		{
			searchEndY = tmpYDown;
		}

		if (searchEndY != -1)
		{
			for (tmpX = posOnAttacker.xPos, cnt = 2; tmpX < dfSECTOR_WIDTH && cnt != 0; ++tmpX, --cnt)
			{
				std::map<DWORD, CharacterInfo*> characterList = sectorList[searchEndY][tmpX];
				AcquireSRWLockShared(&sectorLockList[searchEndY][tmpX]);
				std::map<DWORD, CharacterInfo*>::iterator iter = characterList.begin();
				for (; iter != characterList.end(); ++iter)
				{
					charac = iter->second;
					if (charac->characterID == attackerCharacID)
					{
						continue;
					}
					// 여기서 캐릭터에 락을 걸지 않는 이유는, 
					// 이 함수는 지금 OnRecv를 타고들어와서 받은 패킷을 처리하는
					// 함수에서 호출되는데 거기서 이미 캐릭터에 락을 걸고 있다.
					// 여기서 또 걸면 무조건 데드락이다.
					tmpDistanceX = charac->xPos - attackerXPos;
					tmpDistanceY = abs(charac->yPos - attackerYPos);

					if ((tmpDistanceX >= 0 && tmpDistanceX <= attackXRange) && tmpDistanceY <= attackYRange)
					{
						if ((tmpDistanceX < minDistanceX)
							|| (tmpDistanceX == minDistanceX && tmpDistanceY < minDistanceY))
						{
							minDistanceX = tmpDistanceX;
							minDistanceY = tmpDistanceY;
							targetCharacter = charac;
						}
					}
				}
				ReleaseSRWLockShared(&sectorLockList[searchEndY][tmpX]);

				if (targetCharacter != nullptr)
				{
					*outCharacterIDOnDamage = targetCharacter;
					return true;
				}
			}
		}

		return false;
	}

	return false;
}
