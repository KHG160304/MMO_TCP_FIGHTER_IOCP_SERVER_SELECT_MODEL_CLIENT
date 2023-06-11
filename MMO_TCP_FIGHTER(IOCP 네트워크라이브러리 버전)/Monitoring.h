#pragma once
#ifndef __MONITORING_H__
#define	__MONITORING_H__

typedef	unsigned long DWORD;

DWORD StartMonitor();

void Monitoring();

void CountFrame();

void CountLoop();

void SetCharacterCntHandle(size_t(*size)(void));

void SetSessionCntHandle(size_t(*size)(void));

void SetSectorCharacterCntHandle(size_t(*size)(void));

#endif // !__MONITORING_H__
