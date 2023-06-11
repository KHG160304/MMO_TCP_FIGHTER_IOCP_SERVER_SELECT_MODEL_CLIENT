#pragma once
#ifndef __PROFILER_H__
#define __PROFILER_H__
#define PROFILE
//#undef PROFILE

#ifdef PROFILE
#define PRO_BEGIN(tagName)	BeginProfile(tagName)
#define PRO_END(tagName)	EndProfile(tagName)
#else
#define PRO_BEGIN(tagName)
#define PRO_END(tagName)
#endif // PROFILE

typedef wchar_t	WCHAR;

void BeginProfile(const WCHAR* tag);
void EndProfile(const WCHAR* tag);
bool SaveProfileSampleToText(const WCHAR* szFileName);

#endif // !__PROFILER_H__

