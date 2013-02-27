/*
 *
 *  Utils.h
 *
 *  Copyright (C) Frank Friemel - Feb 2013
 *
 *  This file is part of Shairport4w.
 *
 *  Shairport4w is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  Shairport4w is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#pragma once

template <class T>
void TrimRight(T& s, const char* tr = "\t \n\r")
{
	int n = s.find_last_not_of(tr);
			
	if (n != string::npos)
	   s = s.substr(0, n+1);
	else
	   s.erase();
}

template <class T>
void TrimLeft(T& s, const char* tr = "\t \n\r")
{
	int n = s.find_first_not_of(tr);
			
	if (n != string::npos)
	   s = s.substr(n);
	else
	   s.erase();
}

template <class T>
void Trim(T& s, const char* tr = "\t \n\r")
{
	TrimRight(s, tr);
	TrimLeft(s, tr);
}

__inline _CONST_RETURN char * __CRTDECL memichr(_In_count_(_N) const char *_S, _In_ char _C, _In_ size_t _N)
        {for (; 0 < _N; ++_S, --_N)
                if (tolower(*_S) == tolower(_C))
                        return (_CONST_RETURN char *)(_S);
        return (0); }

// case ignore traits
struct ci_char_traits : public char_traits<char>
{
	static bool eq( char c1, char c2 )
	{ return tolower(c1) == tolower(c2); }

	static bool ne( char c1, char c2 )
	{ return tolower(c1) != tolower(c2); }

	static bool lt( char c1, char c2 )
	{ return tolower(c1) <  tolower(c2); }

	static int compare( const char* s1, const char* s2, size_t n ) 
	{ return memicmp(s1, s2, n); }

	static const char * find(const char *_First, size_t _Count, const char& _Ch)
	{ return ((const char *) memichr(_First, _Ch, _Count)); }
};

// case ignore compare string-class
typedef basic_string<char, ci_char_traits, allocator<char> > ci_string;

extern	CMutex		mtxAppSessionInstance;
extern	CMutex		mtxAppGlobalInstance;
extern	BYTE		hw_addr[6];
extern	string		strConfigName;
extern	bool		bPrivateConfig;
extern	bool		bIsWinXP;

extern	const char*	libao_devicename;
extern	const char*	libao_deviceid;

EXTERN_C char*		libao_file;
EXTERN_C HANDLE		libao_hfile;
EXTERN_C void		deinit_ao();

class CRaopContext;

long StartDecoder(CRaopContext* pContext);
void FlushDecoder();
void VolumeDecoder(double lfVolume);
void StopDecoder();
int	 GetStartFill();
void SetStartFill(int nValue);

#define BUFFER_FRAMES	512
#define START_FILL		282
#define	MIN_FILL		30
#define	MAX_FILL		500

bool GetValueFromRegistry(HKEY hKey, LPCSTR pValueName, string& strValue, LPCSTR strKeyPath = NULL);
bool PutValueToRegistry(HKEY hKey, LPCSTR pValueName, LPCSTR pValue, LPCSTR strKeyPath = NULL);
bool RemoveValueFromRegistry(HKEY hKey, LPCSTR pValueName, LPCSTR strKeyPath = NULL);

bool start_serving();
void stop_serving();
bool is_streaming();

void EncodeBase64(const BYTE* pBlob, long sizeOfBlob, string& strValue, bool bInsertLineBreaks = true);
long DecodeBase64(string strValue, CTempBuffer<BYTE>& Blob);

bool Log(LPCSTR pszFormat, ...);

Bitmap* BitmapFromResource(HINSTANCE hInstance, LPCTSTR strID, LPCTSTR strType = _T("PNG"));

#ifndef BCM_SETSHIELD
#define BCM_SETSHIELD            (0x1600 + 0x000C)
#define Button_SetElevationRequiredState(hwnd, fRequired) \
    (LRESULT)SNDMSG((hwnd), BCM_SETSHIELD, 0, (LPARAM)fRequired)
#endif

#ifndef IDI_SHIELD
#define IDI_SHIELD MAKEINTRESOURCE(32518)
#endif

class CServiceResolveCallback
{
public:
	virtual void OnServiceResolved(const char* strHost, USHORT nPort) = 0;
};

class CRegisterServiceEvent
{
public:
	bool Resolve(CServiceResolveCallback* pCallback);

	bool		m_bRegister;
	ci_string	m_strService;
	ci_string	m_strRegType;
	ci_string	m_strReplyDomain;
	ULONG	    m_nInterfaceIndex;
};
