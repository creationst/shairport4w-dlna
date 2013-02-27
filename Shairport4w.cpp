/*
 *
 *  Shairport4w.cpp
 *
 *  Copyright (C) Frank Friemel - April 2011
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

#include "stdafx.h"

#include "resource.h"

#include "aboutdlg.h"
#include "MainDlg.h"
#include "Bonjour/dns_sd.h"

#include "DmapParser.h"

typedef EXECUTION_STATE (WINAPI *_SetThreadExecutionState)(EXECUTION_STATE esFlags);
_SetThreadExecutionState	pSetThreadExecutionState = NULL;

CAppModule		_Module;
CMutex			mtxAppSessionInstance;
CMutex			mtxAppGlobalInstance;
CMainDlg		dlgMain;
BYTE			hw_addr[6];
string			strConfigName("Shairport4w");
bool			bPrivateConfig	= false;
bool			bIsWinXP		= false;
CMyMutex		mtxRSA;
RSA*			pRSA			= NULL;
HMODULE			hDnsSD			= NULL;
HANDLE			hRedirProcess	= NULL;
CMyMutex		mtxLOG;
string			strPrivateKey	=	"MIIEpQIBAAKCAQEA59dE8qLieItsH1WgjrcFRKj6eUWqi+bGLOX1HL3U3GhC/j0Qg90u3sG/1CUt"
									"wC5vOYvfDmFI6oSFXi5ELabWJmT2dKHzBJKa3k9ok+8t9ucRqMd6DZHJ2YCCLlDRKSKv6kDqnw4U"
									"wPdpOMXziC/AMj3Z/lUVX1G7WSHCAWKf1zNS1eLvqr+boEjXuBOitnZ/bDzPHrTOZz0Dew0uowxf"
									"/+sG+NCK3eQJVxqcaJ/vEHKIVd2M+5qL71yJQ+87X6oV3eaYvt3zWZYD6z5vYTcrtij2VZ9Zmni/"
									"UAaHqn9JdsBWLUEpVviYnhimNVvYFZeCXg/IdTQ+x4IRdiXNv5hEewIDAQABAoIBAQDl8Axy9XfW"
									"BLmkzkEiqoSwF0PsmVrPzH9KsnwLGH+QZlvjWd8SWYGN7u1507HvhF5N3drJoVU3O14nDY4TFQAa"
									"LlJ9VM35AApXaLyY1ERrN7u9ALKd2LUwYhM7Km539O4yUFYikE2nIPscEsA5ltpxOgUGCY7b7ez5"
									"NtD6nL1ZKauw7aNXmVAvmJTcuPxWmoktF3gDJKK2wxZuNGcJE0uFQEG4Z3BrWP7yoNuSK3dii2jm"
									"lpPHr0O/KnPQtzI3eguhe0TwUem/eYSdyzMyVx/YpwkzwtYL3sR5k0o9rKQLtvLzfAqdBxBurciz"
									"aaA/L0HIgAmOit1GJA2saMxTVPNhAoGBAPfgv1oeZxgxmotiCcMXFEQEWflzhWYTsXrhUIuz5jFu"
									"a39GLS99ZEErhLdrwj8rDDViRVJ5skOp9zFvlYAHs0xh92ji1E7V/ysnKBfsMrPkk5KSKPrnjndM"
									"oPdevWnVkgJ5jxFuNgxkOLMuG9i53B4yMvDTCRiIPMQ++N2iLDaRAoGBAO9v//mU8eVkQaoANf0Z"
									"oMjW8CN4xwWA2cSEIHkd9AfFkftuv8oyLDCG3ZAf0vrhrrtkrfa7ef+AUb69DNggq4mHQAYBp7L+"
									"k5DKzJrKuO0r+R0YbY9pZD1+/g9dVt91d6LQNepUE/yY2PP5CNoFmjedpLHMOPFdVgqDzDFxU8hL"
									"AoGBANDrr7xAJbqBjHVwIzQ4To9pb4BNeqDndk5Qe7fT3+/H1njGaC0/rXE0Qb7q5ySgnsCb3DvA"
									"cJyRM9SJ7OKlGt0FMSdJD5KG0XPIpAVNwgpXXH5MDJg09KHeh0kXo+QA6viFBi21y340NonnEfdf"
									"54PX4ZGS/Xac1UK+pLkBB+zRAoGAf0AY3H3qKS2lMEI4bzEFoHeK3G895pDaK3TFBVmD7fV0Zhov"
									"17fegFPMwOII8MisYm9ZfT2Z0s5Ro3s5rkt+nvLAdfC/PYPKzTLalpGSwomSNYJcB9HNMlmhkGzc"
									"1JnLYT4iyUyx6pcZBmCd8bD0iwY/FzcgNDaUmbX9+XDvRA0CgYEAkE7pIPlE71qvfJQgoA9em0gI"
									"LAuE4Pu13aKiJnfft7hIjbK+5kyb3TysZvoyDnb3HOKvInK7vXbKuU4ISgxB2bB3HcYzQMGsz1qJ"
									"2gG0N5hvJpzwwhbhXqFKA4zaaSrw622wDniAK5MlIE0tIAKKP4yxNGjoD2QYjhBGuhvkWKY=";


static HANDLE Exec(LPCTSTR strCmd, DWORD dwWait = INFINITE, bool bStdInput = false)
{
	STARTUPINFO			si;
	PROCESS_INFORMATION	pi;

	memset(&pi, 0, sizeof(pi));
	memset(&si, 0, sizeof(si));

	si.cb			= sizeof(si);
	si.dwFlags		= STARTF_USESHOWWINDOW;
	si.wShowWindow	= SW_HIDE;

	if (bStdInput)
	{
		HANDLE hread	= INVALID_HANDLE_VALUE;
		HANDLE hwrite	= INVALID_HANDLE_VALUE;

		SECURITY_ATTRIBUTES   sa;

		sa.nLength				= sizeof(sa);
		sa.lpSecurityDescriptor = NULL;
		sa.bInheritHandle		= TRUE;

		if (CreatePipe(&hread, &hwrite, &sa, 0))
		{
			SetHandleInformation(hwrite, HANDLE_FLAG_INHERIT, 0);

			libao_hfile = hwrite;

			si.hStdInput  = hread;
			si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
			si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
			si.dwFlags	|= STARTF_USESTDHANDLES;
		}
	}
	TCHAR	_strCmd[4096];

	_tcscpy_s(_strCmd, 4096, strCmd);

	if (CreateProcess(NULL, _strCmd, NULL, NULL, bStdInput ? TRUE : FALSE, 0, NULL, NULL, &si, &pi))
	{
		CloseHandle(pi.hThread);

		if (WaitForSingleObject(pi.hProcess, dwWait) != WAIT_OBJECT_0)
			return pi.hProcess;
		CloseHandle(pi.hProcess);
	}
	return NULL;
}

bool Log(LPCSTR pszFormat, ...)
{
#ifndef _DEBUG
	if (dlgMain.m_bLogToFile)
#endif
	{
		char	buf[32768];
		bool	bResult = true;

		va_list ptr;
		va_start(ptr, pszFormat);

		mtxLOG.Lock();

		try
		{
			vsprintf_s(buf, sizeof(buf), pszFormat, ptr);
			OutputDebugStringA(buf);
			va_end(ptr);

			if (dlgMain.m_bLogToFile)
			{
				char FilePath[MAX_PATH];
		
				ATLVERIFY(GetModuleFileNameA(NULL, FilePath, sizeof(FilePath)) > 0);
				FilePath[strlen(FilePath)-3] = 'l';
				FilePath[strlen(FilePath)-2] = 'o';
				FilePath[strlen(FilePath)-1] = 'g';

				CString strFilePath(FilePath);

				FILE* fh = _tfopen(strFilePath, _T("a"));

				// Get size of file
				fseek(fh, 0, SEEK_END);
				long fSize = ftell(fh);
				rewind(fh);
				
				if (fSize > 1024*1024)
				{
					fclose(fh);
					_tremove(strFilePath + ".bak");
					_trename(strFilePath, strFilePath + ".bak");
					fh = _tfopen(strFilePath, _T("a"));
				}

				if (fh != NULL)
				{
					time_t szClock;

					time(&szClock);
					struct tm*	newTime = localtime(&szClock);

					strftime(FilePath, MAX_PATH, "%x %X ", newTime);
					fwrite(FilePath, sizeof(char), strlen(FilePath), fh);
					fwrite(buf, sizeof(char), strlen(buf), fh);
					fclose(fh);
				}
				else
				{
					bResult = false;
				}
			}
		}
		catch (...)
		{
			// catch that silently
		}
		mtxLOG.Unlock();

		return bResult;
	}
	return true;
}

bool GetValueFromRegistry(HKEY hKey, LPCSTR pValueName, string& strValue, LPCSTR strKeyPath /*= NULL*/)
{
	HKEY	h		= NULL;
	bool	bResult = false;
	string	strKeyName("Software\\");

	if (bPrivateConfig)
		strKeyName += "Shairport4w\\";

	strKeyName += strConfigName;

	if (strKeyPath != NULL)
		strKeyName = strKeyPath;

	if (RegOpenKeyExA(hKey, strKeyName.c_str(), 0, KEY_READ, &h) == ERROR_SUCCESS)
	{
		DWORD dwSize = 0;

		if (RegQueryValueExA(h, pValueName, NULL, NULL, NULL, &dwSize) == ERROR_SUCCESS)
		{
			dwSize += 2;

			LPBYTE pBuf = new BYTE[dwSize];

			memset(pBuf, 0, dwSize);

			if (RegQueryValueExA(h, pValueName, NULL, NULL, pBuf, &dwSize) == ERROR_SUCCESS)
			{
				strValue	= (LPCSTR)pBuf;
				bResult		= true;
			}
			delete [] pBuf;
		}
		RegCloseKey(h);
	}
	return bResult;
}

bool PutValueToRegistry(HKEY hKey, LPCSTR pValueName, LPCSTR pValue, LPCSTR strKeyPath /*= NULL*/)
{
	HKEY	h;
	DWORD	disp	= 0;
	bool	bResult	= false;
	string	strKeyName("Software\\");

	if (bPrivateConfig)
		strKeyName += "Shairport4w\\";

	strKeyName += strConfigName;
	
	if (strKeyPath != NULL)
		strKeyName = strKeyPath;

	if (RegCreateKeyExA(hKey, strKeyName.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &h, &disp) == ERROR_SUCCESS)
	{
		bResult = RegSetValueExA(h, pValueName, 0, REG_SZ, (CONST BYTE *)pValue, ((unsigned long)strlen(pValue)+1) * sizeof(char)) == ERROR_SUCCESS;
		RegCloseKey(h);
	}
	return bResult;
}

bool RemoveValueFromRegistry(HKEY hKey, LPCSTR pValueName, LPCSTR strKeyPath /*= NULL*/)
{
	HKEY	h;
	DWORD	disp	= 0;
	bool	bResult	= false;
	string	strKeyName("Software\\");

	if (bPrivateConfig)
		strKeyName += "Shairport4w\\";

	strKeyName += strConfigName;
	
	if (strKeyPath != NULL)
		strKeyName = strKeyPath;

	if (RegCreateKeyExA(hKey, strKeyName.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &h, &disp) == ERROR_SUCCESS)
	{
		bResult = RegDeleteValueA(h, pValueName) == ERROR_SUCCESS;
		RegCloseKey(h);
	}
	return bResult;
}

void EncodeBase64(const BYTE* pBlob, long sizeOfBlob, string& strValue, bool bInsertLineBreaks /*= true*/)
{
	BIO* bio_base64		= BIO_new(BIO_f_base64());
	BIO* bio_mem		= BIO_new(BIO_s_mem());

	if (!bInsertLineBreaks)
		BIO_set_flags(bio_base64, BIO_FLAGS_BASE64_NO_NL);
	else
		BIO_clear_flags(bio_base64, BIO_FLAGS_BASE64_NO_NL);

	BIO_push(bio_base64, bio_mem);

	BIO_write(bio_base64, pBlob, sizeOfBlob);
	BIO_flush (bio_base64);

	BUF_MEM* pBufferInfo = NULL;
	BIO_get_mem_ptr(bio_mem, &pBufferInfo);

	strValue = string(pBufferInfo->data, pBufferInfo->length);

	BIO_free_all(bio_base64);
}

long DecodeBase64(string strValue, CTempBuffer<BYTE>& Blob)
{
	long nResult = strValue.length();
	
	// Pad with '=' as per RFC 1521
	while (nResult % 4 != 0)
	{
		strValue += "=";
		++nResult;
	}
	Blob.Reallocate(nResult);

	BIO* bio_base64		= BIO_new(BIO_f_base64());
	BIO* bio_mem		= BIO_new_mem_buf((void*)strValue.c_str(), nResult);

	if (strValue.find('\n') == string::npos)
		BIO_set_flags(bio_base64, BIO_FLAGS_BASE64_NO_NL);
	else
		BIO_clear_flags(bio_base64, BIO_FLAGS_BASE64_NO_NL);

	BIO_push(bio_base64, bio_mem);

	nResult = BIO_read(bio_base64, Blob, nResult);

	BIO_free_all(bio_base64);

	return nResult;
}

static string MD5_Hex(const BYTE* pBlob, long sizeOfBlob, bool bUppercase = false)
{
	string strResult;

	BYTE md5digest[MD5_DIGEST_LENGTH];

	if (MD5(pBlob, sizeOfBlob, md5digest) != NULL)
	{
		for (long i = 0; i < MD5_DIGEST_LENGTH; ++i)
		{
			char buf[16];

			sprintf_s(buf, 16, bUppercase ? "%02X" : "%02x", md5digest[i]);
			strResult += buf;
		}
	}
	return strResult;
}

static bool ParseRegEx(CAtlRegExp<CAtlRECharTraitsA>& exp, LPCSTR strToParse, LPCSTR strRegExp, list<string>& listTok, BOOL bCaseSensitive = TRUE, BOOL bMatchAllGroups = TRUE)
{
	if (strToParse == NULL || *strToParse == 0)
		return true;

	if (listTok.empty())
	{
		if (exp.Parse(strRegExp, bCaseSensitive) != REPARSE_ERROR_OK)
			return false;
	}
	CAtlREMatchContext<CAtlRECharTraitsA> mc; 

	if (exp.Match(strToParse, &mc))
	{
		const CAtlREMatchContext<CAtlRECharTraitsA>::RECHAR* szEnd = NULL; 

		for (UINT nGroupIndex = 0; nGroupIndex < mc.m_uNumGroups; ++nGroupIndex) 
		{ 
			const CAtlREMatchContext<CAtlRECharTraitsA>::RECHAR* szStart = NULL; 
				
			mc.GetMatch(nGroupIndex, &szStart, &szEnd);
				
			ptrdiff_t nLength = szEnd - szStart; 
		
			if (nLength > 0)
			{
				listTok.push_back(string(szStart, nLength));
				
				if (!bMatchAllGroups)
					break;
			}
		}
		return ParseRegEx(exp, szEnd, strRegExp, listTok, bCaseSensitive, bMatchAllGroups);
	}
	return true;
}

static bool ParseRegEx(LPCSTR strToParse, LPCSTR strRegExp, list<string>& listTok, BOOL bCaseSensitive=TRUE)
{
	CAtlRegExp<CAtlRECharTraitsA> exp;
	
	return ParseRegEx(exp, strToParse, strRegExp, listTok, bCaseSensitive);
}

static bool ParseRegEx(LPCSTR strToParse, LPCSTR strRegExp, string& strTok, BOOL bCaseSensitive=TRUE)
{
	CAtlRegExp<CAtlRECharTraitsA> exp;

	list<string> listTok;
	
	ParseRegEx(exp, strToParse, strRegExp, listTok, bCaseSensitive);

	if (!listTok.empty())
	{
		strTok = *listTok.begin();
		return true;
	}
	return false;
}

static bool ParseRegEx(LPCSTR strToParse, LPCSTR strRegExp, map<ci_string, string>& mapKeyValue, BOOL bCaseSensitive=TRUE)
{
	CAtlRegExp<CAtlRECharTraitsA> exp;

	list<string> listTok;

	if (ParseRegEx(exp, strToParse, strRegExp, listTok, bCaseSensitive))
	{
		for (list<string>::iterator i = listTok.begin(); i != listTok.end(); ++i)
		{
			string& strValue = mapKeyValue[i->c_str()];

			if (++i == listTok.end())
				break;
			strValue = *i;
		}
		return true;
	}
	return false;
}

static DNSServiceErrorType StaticDNSServiceRegister
    (
    DNSServiceRef                       *sdRef,
    DNSServiceFlags                     flags,
    uint32_t                            interfaceIndex,
    const char                          *name,         /* may be NULL */
    const char                          *regtype,
    const char                          *domain,       /* may be NULL */
    const char                          *host,         /* may be NULL */
    uint16_t                            port,
    uint16_t                            txtLen,
    const void                          *txtRecord,    /* may be NULL */
    DNSServiceRegisterReply             callBack,      /* may be NULL */
    void                                *context       /* may be NULL */
    )
{
	typedef DNSServiceErrorType (DNSSD_API *_typeDNSServiceRegister)
																(
																DNSServiceRef                       *sdRef,
																DNSServiceFlags                     flags,
																uint32_t                            interfaceIndex,
																const char                          *name,         /* may be NULL */
																const char                          *regtype,
																const char                          *domain,       /* may be NULL */
																const char                          *host,         /* may be NULL */
																uint16_t                            port,
																uint16_t                            txtLen,
																const void                          *txtRecord,    /* may be NULL */
																DNSServiceRegisterReply             callBack,      /* may be NULL */
																void                                *context       /* may be NULL */
																);

	_typeDNSServiceRegister pDNSServiceRegister = (_typeDNSServiceRegister) GetProcAddress(hDnsSD, "DNSServiceRegister");

	if (pDNSServiceRegister != NULL)
	{
		return pDNSServiceRegister( sdRef,
								    flags,
								    interfaceIndex,
								    name,       
								    regtype,
								    domain,     
								    host,       
								    port,
								    txtLen,
								    txtRecord,  
								    callBack,    
								    context);
	}
	return kDNSServiceErr_Unsupported;
}

static void StaticDNSServiceRefDeallocate(DNSServiceRef sdRef)
{
	typedef void (DNSSD_API *_typeDNSServiceRefDeallocate)(DNSServiceRef sdRef);

	_typeDNSServiceRefDeallocate pDNSServiceRefDeallocate = (_typeDNSServiceRefDeallocate) GetProcAddress(hDnsSD, "DNSServiceRefDeallocate");

	if (pDNSServiceRefDeallocate != NULL)
		pDNSServiceRefDeallocate(sdRef);
	else
	{
		ATLASSERT(FALSE);
	}
}

static DNSServiceErrorType RegisterService(DNSServiceRef* sdRef, const char* nam, const char* typ, const char* dom, uint16_t port, char** argv)
{
	char		txt[2048];
	uint16_t	n	= 0;

	memset(txt, 0, sizeof(txt));
	
	if (nam[0] == '.' && nam[1] == 0) 
		nam = "";
	if (dom[0] == '.' && dom[1] == 0) 
		dom = "";

	while (true)
	{
		const char*	p = *argv++;

		if (p == NULL)
			break;

		if (strlen(p) > 0)
		{
			txt[n++] = (char)strlen(p);

			strcpy(txt + n, p);
			n += strlen(p);
		}
	}
	return StaticDNSServiceRegister(sdRef, 0, kDNSServiceInterfaceIndexAny, nam, typ, dom, NULL, port, n, txt, NULL, NULL);
}

static int StaticDNSServiceRefSockFD(DNSServiceRef sdRef)
{
	typedef int (DNSSD_API *_typeDNSServiceRefSockFD)(DNSServiceRef sdRef);

	_typeDNSServiceRefSockFD pDNSServiceRefSockFD = (_typeDNSServiceRefSockFD) GetProcAddress(hDnsSD, "DNSServiceRefSockFD");

	if (pDNSServiceRefSockFD != NULL)
		return pDNSServiceRefSockFD(sdRef);
	else
	{
		ATLASSERT(FALSE);
	}
	return INVALID_SOCKET;
}

static DNSServiceErrorType StaticDNSServiceProcessResult(DNSServiceRef sdRef)
{
	typedef DNSServiceErrorType (DNSSD_API *_typeDNSServiceProcessResult)(DNSServiceRef sdRef);

	_typeDNSServiceProcessResult pDNSServiceProcessResult = (_typeDNSServiceProcessResult) GetProcAddress(hDnsSD, "DNSServiceProcessResult");

	if (pDNSServiceProcessResult != NULL)
		return pDNSServiceProcessResult(sdRef);
	else
	{
		ATLASSERT(FALSE);
	}
	return kDNSServiceErr_Unsupported;
}

void DNSSD_API MyDNSServiceResolveReply
    (
		DNSServiceRef                       sdRef,
		DNSServiceFlags                     flags,
		uint32_t                            interfaceIndex,
		DNSServiceErrorType                 errorCode,
		const char                          *fullname,
		const char                          *hosttarget,
		uint16_t                            port,
		uint16_t                            txtLen,
		const unsigned char                 *txtRecord,
		void                                *context
    )
{
	ATLASSERT(context != NULL);

	CServiceResolveCallback* pCallback = (CServiceResolveCallback*) context;

	pCallback->OnServiceResolved(hosttarget, port);
}

static DNSServiceErrorType StaticDNSServiceResolve
    (
    DNSServiceRef                       *sdRef,
    DNSServiceFlags                     flags,
    uint32_t                            interfaceIndex,
    const char                          *name,
    const char                          *regtype,
    const char                          *domain,
    DNSServiceResolveReply              callBack,
    void                                *context  /* may be NULL */
  
    )
{
	typedef DNSServiceErrorType (DNSSD_API *_typeDNSServiceResolve)
												(
												DNSServiceRef                       *sdRef,
												DNSServiceFlags                     flags,
												uint32_t                            interfaceIndex,
												const char                          *name,
												const char                          *regtype,
												const char                          *domain,
												DNSServiceResolveReply              callBack,
												void                                *context  
												);

	_typeDNSServiceResolve pDNSServiceResolve = (_typeDNSServiceResolve) GetProcAddress(hDnsSD, "DNSServiceResolve");

	if (pDNSServiceResolve != NULL)
		return pDNSServiceResolve(sdRef,
								 flags,
								 interfaceIndex,
								 name,
								 regtype,
								 domain,
								 callBack,
								 context);
	else
	{
		ATLASSERT(FALSE);
	}
	return kDNSServiceErr_Unsupported;
}

bool CRegisterServiceEvent::Resolve(CServiceResolveCallback* pCallback)
{
	DNSServiceRef sd;

	if (kDNSServiceErr_NoError == StaticDNSServiceResolve(&sd, 0, m_nInterfaceIndex, m_strService.c_str()
															, m_strRegType.c_str(), m_strReplyDomain.c_str()
															, MyDNSServiceResolveReply, pCallback))
	{
		StaticDNSServiceProcessResult(sd);
		StaticDNSServiceRefDeallocate(sd);
		return true;
	}
	return false;
}

static DNSServiceErrorType StaticDNSServiceBrowse(DNSServiceRef *sdRef,
    DNSServiceFlags                     flags,
    uint32_t                            interfaceIndex,
    const char                          *regtype,
    const char                          *domain,    /* may be NULL */
    DNSServiceBrowseReply               callBack,
    void                                *context    /* may be NULL */)
{
	typedef DNSServiceErrorType (DNSSD_API *_typeDNSServiceBrowse)(DNSServiceRef *sdRef,
    DNSServiceFlags                     flags,
    uint32_t                            interfaceIndex,
    const char                          *regtype,
    const char                          *domain,    /* may be NULL */
    DNSServiceBrowseReply               callBack,
    void                                *context    /* may be NULL */);

	_typeDNSServiceBrowse pDNSServiceBrowse = (_typeDNSServiceBrowse) GetProcAddress(hDnsSD, "DNSServiceBrowse");

	if (pDNSServiceBrowse != NULL)
		return pDNSServiceBrowse(sdRef, flags, interfaceIndex, regtype, domain, callBack, context);
	else
	{
		ATLASSERT(FALSE);
	}
	return kDNSServiceErr_Unsupported;
}

static string IPAddrToString(struct sockaddr_in* in)
{
	char	buf[256];
	string	strResult = "";

	switch(in->sin_family)
	{
		case AF_INET:
		{
			sprintf_s(buf, 256, "%ld.%ld.%ld.%ld", (long)in->sin_addr.S_un.S_un_b.s_b1
											, (long)in->sin_addr.S_un.S_un_b.s_b2
											, (long)in->sin_addr.S_un.S_un_b.s_b3
											, (long)in->sin_addr.S_un.S_un_b.s_b4);
			strResult = buf;
		}
		break;

		case AF_INET6:
		{
			DWORD dwLen = sizeof(buf) / sizeof(buf[0]);

			if (WSAAddressToStringA((LPSOCKADDR)in, sizeof(struct sockaddr_in6), NULL, buf, &dwLen) == 0)
			{
				buf[dwLen] = 0;
				strResult = buf;
			}
		}
		break;
	}
	return strResult;
}

static void GetPeerIP(SOCKET sd, bool bV4, string& strIP)
{
	if (bV4)
	{
		struct sockaddr_in	laddr_in;
		int					laddr_size = sizeof(laddr_in);

		if (0 == getpeername(sd, (sockaddr *)&laddr_in, &laddr_size))
			strIP = IPAddrToString(&laddr_in);
	}
	else
	{
		struct sockaddr_in6	laddr_in;
		int					laddr_size = sizeof(laddr_in);

		if (0 == getpeername(sd, (sockaddr *)&laddr_in, &laddr_size))
			strIP = IPAddrToString((struct sockaddr_in*)&laddr_in);
	}
}

static void GetLocalIP(SOCKET sd, bool bV4, string& strIP)
{
	if (bV4)
	{
		struct sockaddr_in	laddr_in;
		int					laddr_size = sizeof(laddr_in);

		if (0 == getsockname(sd, (sockaddr *)&laddr_in, &laddr_size))
			strIP = IPAddrToString(&laddr_in);
	}
	else
	{
		struct sockaddr_in6	laddr_in;
		int					laddr_size = sizeof(laddr_in);

		if (0 == getsockname(sd, (sockaddr *)&laddr_in, &laddr_size))
			strIP = IPAddrToString((struct sockaddr_in*)&laddr_in);
	}
}

static int GetLocalIP(SOCKET sd, bool bV4, CTempBuffer<BYTE>& pIP)
{
	int nResult = 0;

	if (bV4)
	{
		pIP.Allocate(4);

		struct sockaddr_in	laddr_in;
		int					laddr_size = sizeof(laddr_in);

		if (0 == getsockname(sd, (sockaddr *)&laddr_in, &laddr_size))
		{
			nResult = 4;
			memcpy(pIP, &laddr_in.sin_addr.s_addr, nResult);
		}
	}
	else
	{
		pIP.Allocate(16);

		struct sockaddr_in6	laddr_in;
		int					laddr_size = sizeof(laddr_in);

		if (0 == getsockname(sd, (sockaddr *)&laddr_in, &laddr_size))
		{
			nResult = 16;
			memcpy(pIP, &laddr_in.sin6_addr, nResult);
		}
	}
	return nResult;
}

static bool digest_ok(CRaopContext& conn)
{
	if (!conn.m_strNonce.empty())
	{
		string& strAuth = conn.m_CurrentHttpRequest.m_mapHeader["Authorization"];

		if (strAuth.find("Digest ") == 0)
		{
			USES_CONVERSION;

			map<ci_string, string> mapAuth;
			ParseRegEx(strAuth.c_str()+7, "{\\a+}=\"{[^\"]+}\"", mapAuth, FALSE);

			string str1 = mapAuth["username"] + string(":") + mapAuth["realm"] + string(":") + string(T2CA(dlgMain.m_strPassword));
			string str2 = conn.m_CurrentHttpRequest.m_strMethod + string(":") + mapAuth["uri"];

			string strDigest1 = MD5_Hex((const BYTE*)str1.c_str(), str1.length(), true);
			string strDigest2 = MD5_Hex((const BYTE*)str2.c_str(), str2.length(), true);

			string strDigest = strDigest1 + string(":") + mapAuth["nonce"] + string(":") + strDigest2;

			strDigest = MD5_Hex((const BYTE*)strDigest.c_str(), strDigest.length(), true);

			Log("Password Digest: \"%s\" == \"%s\"\n", strDigest.c_str(), mapAuth["response"].c_str());

			return strDigest == mapAuth["response"];
		}
	}
	return false;
}

class CDnsSD_Thread : protected CMyThread
{
public:
	CDnsSD_Thread()
	{
		m_sdref			= NULL;
	}
	~CDnsSD_Thread()
	{
		if (m_sdref != NULL)
			StaticDNSServiceRefDeallocate(m_sdref);
	}
public:
	void Stop()
	{
		CMyThread::Stop();

		if (m_sdref != NULL)
		{
			StaticDNSServiceRefDeallocate(m_sdref);
			m_sdref = NULL;
		}
	}
protected:
	virtual BOOL OnStart()
	{
		ATLASSERT(m_sdref != NULL);

		int sd = StaticDNSServiceRefSockFD(m_sdref);

		if (sd == INVALID_SOCKET)
		{
			ATLASSERT(FALSE);
			return FALSE;
		}
		WSAEventSelect(sd, NULL, 0);

		DWORD dw = TRUE;

		if (ioctlsocket(sd, FIONBIO, &dw) == SOCKET_ERROR)
		{
			ATLASSERT(FALSE);
			return FALSE;
		}
		CloseHandle(m_hEvent);
		m_hEvent = WSACreateEvent();

		if (m_hEvent == WSA_INVALID_EVENT)
		{
			::closesocket(sd);
			ATLASSERT(FALSE);
			return FALSE;
		}
		if (WSAEventSelect(sd, m_hEvent, FD_READ|FD_CLOSE) != 0)
		{
			::closesocket(sd);
			ATLASSERT(FALSE);
			return FALSE;
		}
		m_socket.Attach(sd);
		return TRUE;
	}
	virtual void OnStop()
	{
		m_socket.Close();
	}
	virtual void OnEvent()
	{
		WSANETWORKEVENTS	NetworkEvents;
		bool				bClose = false;

		if (WSAEnumNetworkEvents(m_socket.m_sd, m_hEvent, &NetworkEvents) == 0)
		{
  			if (NetworkEvents.lNetworkEvents & FD_READ)
			{
				ResetEvent(m_hEvent);

				DNSServiceErrorType err = StaticDNSServiceProcessResult(m_sdref);

				if (err != kDNSServiceErr_NoError)
				{
					Log("DNSServiceProcessResult failed with code %ld!\n", (long)err);
				}
			}
			if (NetworkEvents.lNetworkEvents & FD_CLOSE)
			{
				ResetEvent(m_hEvent);
				bClose = true;
			}
		}
		if (bClose)
		{
			Log("DNS-SD closed the connection!\n");
			SetEvent(m_hStopEvent);
		}
	}
protected:
	DNSServiceRef	m_sdref;
	CSocketBase		m_socket;
};

class CDnsSD_Register : public CDnsSD_Thread
{
public:
	bool Start(USHORT nPort)
	{
		USES_CONVERSION;

		char buf[512];

		sprintf_s(buf, 512, "%02X%02X%02X%02X%02X%02X@%s"	, hw_addr[0]
															, hw_addr[1]
															, hw_addr[2]
															, hw_addr[3]
															, hw_addr[4]
															, hw_addr[5]
															, T2CA(dlgMain.m_strApName));

															char* argv[] = { "tp=UDP", "sm=false", "sv=false", "ek=1", "et=0,1", dlgMain.m_bNoMetaInfo ? "" : "md=0,1,2", "cn=0,1", "ch=2", "ss=16", "sr=44100", dlgMain.m_strPassword.IsEmpty() ? "pw=false" : "pw=true", "vn=3", "txtvers=1", NULL };

		if (kDNSServiceErr_NoError == RegisterService(&m_sdref, buf, "_raop._tcp", ".", nPort, argv))
		{
			return CMyThread::Start() ? true : false;
		}
		return false;
	}
};

class CDnsSD_BrowseForService : public CDnsSD_Thread
{
public:
	static void DNSSD_API MyDNSServiceBrowseReply
								(
									DNSServiceRef                       sdRef,
									DNSServiceFlags                     flags,
									uint32_t                            interfaceIndex,
									DNSServiceErrorType                 errorCode,
									const char                          *serviceName,
									const char                          *regtype,
									const char                          *replyDomain,
									void                                *context
								)
	{
		ATLASSERT(context != NULL);

		((CDnsSD_BrowseForService*) context)->OnDNSServiceBrowseReply(sdRef, flags, interfaceIndex, errorCode, serviceName, regtype, replyDomain);
	}
	virtual void OnDNSServiceBrowseReply(
									DNSServiceRef                       sdRef,
									DNSServiceFlags                     flags,
									uint32_t                            interfaceIndex,
									DNSServiceErrorType                 errorCode,
									const char                          *serviceName,
									const char                          *regtype,
									const char                          *replyDomain
									)
	{
		if (serviceName != NULL && dlgMain.IsWindow())
		{
			CRegisterServiceEvent* pEvent = new CRegisterServiceEvent;

			pEvent->m_nInterfaceIndex	= interfaceIndex;
			pEvent->m_strService		= serviceName;
			pEvent->m_strRegType		= regtype != NULL		? regtype		: "";
			pEvent->m_strReplyDomain	= replyDomain != NULL	? replyDomain	: "";
			pEvent->m_bRegister			= (flags & kDNSServiceFlagsAdd) ? true : false;

			dlgMain.PostMessage(m_nMsg, 0, (LPARAM)pEvent);
		}
	}
	bool Start(const char* strRegType, UINT nMsg)
	{
		m_nMsg = nMsg;

		USES_CONVERSION;

		if (kDNSServiceErr_NoError == StaticDNSServiceBrowse(&m_sdref, 0, 0, strRegType, NULL, MyDNSServiceBrowseReply, this))
		{
			return CMyThread::Start() ? true : false;
		}
		return false;
	}
protected:
	UINT	m_nMsg;
};

static	map<SOCKET, CRaopContext*>	c_mapConnection;
static 	CMyMutex					c_mtxConnection;

class CRaopServer : public CNetworkServer, protected CDmapParser
{
protected:
	// Dmap Parser Callbacks
	virtual void on_string(void* ctx, const char *code, const char *name, const char *buf, size_t len)  
	{
		CRaopContext* pCtx = (CRaopContext*)ctx;
		ATLASSERT(pCtx != NULL);
		
		if (stricmp(name, "daap.songalbum") == 0)
		{
			WTL::CString strValue;

			LPTSTR p = strValue.GetBuffer(len*2);

			if (p != NULL)
			{
				int n = ::MultiByteToWideChar(CP_UTF8, 0, buf, len, p, len*2);
				strValue.ReleaseBuffer((n >= 0) ? n : -1);
			}
			pCtx->Lock();
			pCtx->m_str_daap_songalbum = strValue;
			pCtx->Unlock();
		}
		else if (stricmp(name, "daap.songartist") == 0)
		{
			WTL::CString strValue;

			LPTSTR p = strValue.GetBuffer(len*2);

			if (p != NULL)
			{
				int n = ::MultiByteToWideChar(CP_UTF8, 0, buf, len, p, len*2);
				strValue.ReleaseBuffer((n >= 0) ? n : -1);
			}
			pCtx->Lock();
			pCtx->m_str_daap_songartist = strValue;
			pCtx->Unlock();
		}
		else if (stricmp(name, "dmap.itemname") == 0)
		{
			WTL::CString strValue;

			LPTSTR p = strValue.GetBuffer(len*2);

			if (p != NULL)
			{
				int n = ::MultiByteToWideChar(CP_UTF8, 0, buf, len, p, len*2);
				strValue.ReleaseBuffer((n >= 0) ? n : -1);
			}
			pCtx->Lock();
			pCtx->m_str_daap_trackname = strValue;
			pCtx->Unlock();
		}
	}
	// Network Callbacks
	virtual bool OnAccept(SOCKET sd)
	{
		string strIP;

		GetPeerIP(sd, m_bV4, strIP);
		Log("Accepted new client %s\n", strIP.c_str());

		c_mtxConnection.Lock();

		if (c_mapConnection.find(sd) != c_mapConnection.end())
			delete c_mapConnection[sd];

		c_mapConnection[sd] = new CRaopContext;

		c_mapConnection[sd]->m_bV4 = m_bV4;
		c_mtxConnection.Unlock();

		return true;
	}
	virtual void OnRequest(SOCKET sd)
	{
		CTempBuffer<char> buf(0x100000);

		memset(buf, 0, 0x100000);

		CSocketBase sock;

		sock.Attach(sd);

		int nReceived = sock.recv(buf, 0x100000);
		ATLASSERT(nReceived > 0);

		c_mtxConnection.Lock();

		CRaopContext&	conn	= *c_mapConnection[sd];

		c_mtxConnection.Unlock();

		CHttp&			request	= conn.m_CurrentHttpRequest;

		request.m_strBuf += string(buf, nReceived);

		if (request.Parse())
		{
			if (request.m_mapHeader.find("content-length") != request.m_mapHeader.end())
			{
				if (atol(request.m_mapHeader["content-length"].c_str()) > (long)request.m_strBody.length())
				{
					// need more
					sock.Detach();
					return;
				}
			}
			Log("Http Request: %s\n", request.m_strBuf.c_str());

			CHttp response;

			response.Create(request.m_strProtocol.c_str());

			response.m_mapHeader["CSeq"]				= request.m_mapHeader["CSeq"];
			response.m_mapHeader["Audio-Jack-Status"]	= "connected; type=analog";

			if (request.m_mapHeader.find("Apple-Challenge") != request.m_mapHeader.end())
			{
				string strChallenge = request.m_mapHeader["Apple-Challenge"];
				
				CTempBuffer<BYTE>	pChallenge;
				long				sizeChallenge	= 0;

				if ((sizeChallenge = DecodeBase64(strChallenge, pChallenge)))
				{
					CTempBuffer<BYTE>	pIP;
					int					sizeIP = GetLocalIP(sd, m_bV4, pIP);

					BYTE pAppleResponse[64];
					long sizeAppleResponse = 0;

					memset(pAppleResponse, 0, sizeof(pAppleResponse));

					memcpy(pAppleResponse+sizeAppleResponse, pChallenge, sizeChallenge);
					sizeAppleResponse += sizeChallenge;
					ATLASSERT(sizeAppleResponse <= sizeof(pAppleResponse));
					memcpy(pAppleResponse+sizeAppleResponse, pIP, sizeIP);
					sizeAppleResponse += sizeIP;
					ATLASSERT(sizeAppleResponse <= sizeof(pAppleResponse));
					memcpy(pAppleResponse+sizeAppleResponse, hw_addr, sizeof(hw_addr));
					sizeAppleResponse += sizeof(hw_addr);
					ATLASSERT(sizeAppleResponse <= sizeof(pAppleResponse));

					sizeAppleResponse = max(32, sizeAppleResponse);

					LPBYTE pSignature		= new BYTE[2048];
					size_t sizeSignature	= 0;

					mtxRSA.Lock();
					sizeSignature = RSA_private_encrypt(sizeAppleResponse, pAppleResponse, pSignature, pRSA, RSA_PKCS1_PADDING);
					mtxRSA.Unlock();

					ATLASSERT(sizeSignature > 0);

					string strAppleResponse;
					EncodeBase64(pSignature, sizeSignature, strAppleResponse, false);

					TrimRight(strAppleResponse, "=\r\n");

					response.m_mapHeader["Apple-Response"] = strAppleResponse;

					delete [] pSignature;
				}
			}
			if (dlgMain.m_strPassword.GetLength() > 0)
			{
				if (!digest_ok(conn))
				{
					USES_CONVERSION;

					BYTE nonce[20];

					for (long i = 0; i < 20; ++i)
					{
						nonce[i] = (256 * rand()) / RAND_MAX;
					}
					conn.m_strNonce = MD5_Hex(nonce, 20);

					response.m_mapHeader["WWW-Authenticate"] =		string("Digest realm=\"") 
																+	string(T2CA(dlgMain.m_strApName))
																+	string("\", nonce=\"")
																+	conn.m_strNonce
																+	string("\"");
					response.SetStatus(401, "Unauthorized");
					request.m_strMethod = "DENIED";
				}
		    }
			if (stricmp(request.m_strMethod.c_str(), "OPTIONS") == 0)
			{
				response.m_mapHeader["Public"] = "ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER";
			}
			else if (stricmp(request.m_strMethod.c_str(), "ANNOUNCE") == 0)
			{
				map<ci_string, string>	mapKeyValue;
				ParseRegEx(request.m_strBody.c_str(), "a={[^:]+}:{[^\r^\n]+}", mapKeyValue, FALSE);

				conn.Lock();

				conn.Announce();

				conn.m_strFmtp			= mapKeyValue["fmtp"];
				conn.m_sizeAesiv		= DecodeBase64(mapKeyValue["aesiv"], conn.m_Aesiv);
				conn.m_sizeRsaaeskey	= DecodeBase64(mapKeyValue["rsaaeskey"], conn.m_Rsaaeskey);

				ParseRegEx(conn.m_strFmtp.c_str(), "{\\z}", conn.m_listFmtp, FALSE);
				
				if (conn.m_listFmtp.size() >= 12)
				{
					list<string>::iterator iFmtp = conn.m_listFmtp.begin();
					advance(iFmtp, 11);

					conn.m_lfFreq = atof(iFmtp->c_str());
				}
				conn.Unlock();

				mtxRSA.Lock();
				conn.m_sizeRsaaeskey = RSA_private_decrypt(conn.m_sizeRsaaeskey, conn.m_Rsaaeskey, conn.m_Rsaaeskey, pRSA, RSA_PKCS1_OAEP_PADDING);
				mtxRSA.Unlock();
			}
			else if (stricmp(request.m_strMethod.c_str(), "SETUP") == 0)
			{
				bool bOkToSetup = true;

				c_mtxConnection.Lock();

				// currently we allow only *one* streaming client (which makes sence anyway)
				for (map<SOCKET, CRaopContext*>::iterator i = c_mapConnection.begin(); i != c_mapConnection.end(); ++i)
				{
					if (sd != i->first && i->second->m_bIsStreaming)
					{
						bOkToSetup = false;
						break;
					}
				}
				if (bOkToSetup)
				{
					conn.m_bIsStreaming = true;

					if (request.m_mapHeader.find("DACP-ID") != request.m_mapHeader.end())
					{
						dlgMain.m_strDACP_ID		= ci_string("iTunes_Ctrl_") + ci_string(request.m_mapHeader["DACP-ID"].c_str());
						dlgMain.m_strActiveRemote	= request.m_mapHeader["Active-Remote"].c_str();
					}
					dlgMain.PostMessage(WM_RAOP_CTX, 0, (LPARAM)&conn);

					if (pSetThreadExecutionState != NULL)
						pSetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);

					c_mtxConnection.Unlock();

					string strTransport = request.m_mapHeader["transport"];

					ParseRegEx(strTransport.c_str(), "control_port={\\z}", conn.m_strCport, FALSE);
					ParseRegEx(strTransport.c_str(), "timing_port={\\z}", conn.m_strTport, FALSE);
					ParseRegEx(strTransport.c_str(), "server_port={\\z}", conn.m_strSport, FALSE);
				
					response.m_mapHeader["Session"] = "DEADBEEF";

					char strDecoderPort[64];

					sprintf_s(strDecoderPort, 64, "%ld", StartDecoder(&conn));

					response.m_mapHeader["Transport"] = strTransport + string(";server_port=") + string(strDecoderPort);
				}
				else
				{
					c_mtxConnection.Unlock();

					response.SetStatus(503, "Service Unavailable");
				}
			}
			else if (stricmp(request.m_strMethod.c_str(), "FLUSH") == 0)
			{
				if (dlgMain.m_hWnd != NULL && dlgMain.m_pRaopContext == &conn)
					dlgMain.PostMessage(WM_PAUSE, 1);
				FlushDecoder();
			}
			else if (stricmp(request.m_strMethod.c_str(), "TEARDOWN") == 0)
			{
				if (dlgMain.m_hWnd != NULL && dlgMain.m_pRaopContext == &conn)
					dlgMain.PostMessage(WM_STOP);

				StopDecoder();
				conn.m_bIsStreaming = false;
				response.m_mapHeader["Connection"] = "close";

				if (pSetThreadExecutionState != NULL)
					pSetThreadExecutionState(ES_CONTINUOUS);
			}
			else if (stricmp(request.m_strMethod.c_str(), "RECORD") == 0)
			{
				dlgMain.PostMessage(WM_RESUME);
			}
			else if (stricmp(request.m_strMethod.c_str(), "SET_PARAMETER") == 0)
			{
				map<ci_string, string>::iterator ct = request.m_mapHeader.find("content-type");

				if (ct != request.m_mapHeader.end())
				{
					if (stricmp(ct->second.c_str(), "application/x-dmap-tagged") == 0)
					{
						if (!request.m_strBody.empty())
						{
							conn.Lock();
							conn.ResetSongInfos();
							conn.Unlock();

							dmap_parse(&conn, request.m_strBody.c_str(), request.m_strBody.length());

							if (dlgMain.m_hWnd != NULL && dlgMain.m_pRaopContext == &conn)
								dlgMain.PostMessage(WM_SONG_INFO);
						}
					}
					else if (ct->second.length() > 6 && strnicmp(ct->second.c_str(), "image/", 6) == 0)
					{
						conn.Lock();

						if (conn.m_bmInfo != NULL)
						{
							delete conn.m_bmInfo;
							conn.m_bmInfo = NULL;
						}

						if (stricmp(ct->second.c_str()+6, "none") == 0)
						{
							// do nothing
						}
						else if (stricmp(ct->second.c_str()+6, "jpeg") == 0 || stricmp(ct->second.c_str()+6, "png") == 0)
						{
							HGLOBAL hBlob = GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT, request.m_strBody.length());  

							if (hBlob != NULL)
							{
								IStream* pStream = NULL;

								if (SUCCEEDED(CreateStreamOnHGlobal(hBlob, TRUE, &pStream)))
								{
									ULONG nWritten = 0;
									pStream->Write(request.m_strBody.c_str(), request.m_strBody.length(), &nWritten);
									ATLASSERT(request.m_strBody.length() == nWritten);

									conn.m_bmInfo = Bitmap::FromStream(pStream);

									pStream->Release();
								}
								else
								{
									GlobalFree(hBlob);
								}
							}
						}
						else
						{
							Log("Unknown Imagetype: %s\n", ct->second.c_str());
						}
						conn.Unlock();

						if (dlgMain.m_pRaopContext == &conn)
							dlgMain.PostMessage(WM_INFO_BMP);
					}
					else if (stricmp(ct->second.c_str(), "text/parameters") == 0)
					{
						if (!request.m_strBody.empty())
						{
							map<ci_string, string>	mapKeyValue;
							ParseRegEx(request.m_strBody.c_str(), "{[^:]+}:{[^\r^\n]+}", mapKeyValue, FALSE);

							if (!mapKeyValue.empty())
							{
								map<ci_string, string>::iterator i = mapKeyValue.find("volume");

								if (i != mapKeyValue.end())
								{
									VolumeDecoder(atof(i->second.c_str()));
								}
								else
								{
									i = mapKeyValue.find("progress");

									if (i != mapKeyValue.end())
									{
										list<string> listProgressValues;

										ParseRegEx(i->second.c_str(), "{\\z}/{\\z}/{\\z}", listProgressValues, FALSE);

										if (listProgressValues.size() == 3)
										{
											list<string>::iterator tsi = listProgressValues.begin();

											double start	= atof(tsi->c_str());
											double curr		= atof((++tsi)->c_str());
											double end		= atof((++tsi)->c_str());

											if (end >= start)
											{
												time_t nTotalSecs = (time_t)((end-start)  / conn.m_lfFreq);

												conn.Lock();

												conn.m_nTimeStamp		= ::time(NULL);
												conn.m_nTimeTotal		= nTotalSecs;

												if (curr > start)
													conn.m_nTimeCurrentPos	= (time_t)((curr-start) / conn.m_lfFreq); 
												else
													conn.m_nTimeCurrentPos	= 0;

												conn.m_nDurHours = (long)(nTotalSecs / 3600);
												nTotalSecs -= ((time_t)conn.m_nDurHours*3600);

												conn.m_nDurMins = (long)(nTotalSecs / 60);
												conn.m_nDurSecs = (long)(nTotalSecs % 60);

												conn.Unlock();

												if (dlgMain.m_hWnd != NULL && dlgMain.m_pRaopContext == &conn)
													dlgMain.PostMessage(WM_RESUME, 1);
											}
										}
										else
										{
											Log("Unexpected Value of Progress Parameter: %s\n", i->second.c_str());
										}
									}
									else
									{
										Log("Unknown textual parameter: %s\n", request.m_strBody.c_str());
									}
								}
							}
							else
							{
								Log("No Key Values in textual parameter: %s\n", request.m_strBody.c_str());
							}
						}
					}
					else
					{
						Log("Unknown Parameter Content Type: %s\n", ct->second.c_str());
					}
				}
			}
			string strResponse = response.GetAsString();

			strResponse += "\r\n";

			Log("Http Response: %s\n", strResponse.c_str());

			if (!sock.Send(strResponse.c_str(), strResponse.length()))
			{
				Log("Communication failed\n");
			}
			request.InitNew();
		}
		sock.Detach();
	}
	virtual void OnDisconnect(SOCKET sd)
	{
		string strIP;

		GetPeerIP(sd, m_bV4, strIP);

		Log("Client %s disconnected\n", strIP.c_str());
		c_mtxConnection.Lock();

		map<SOCKET, CRaopContext*>::iterator conn = c_mapConnection.find(sd);

		if (conn != c_mapConnection.end())
		{
			if (dlgMain.m_hWnd != NULL && dlgMain.m_pRaopContext == conn->second)
			{
				dlgMain.SendMessage(WM_RAOP_CTX, 0, NULL);
			}

			if (conn->second->m_bIsStreaming)
			{
				StopDecoder();
				FlushDecoder();
			}
			delete conn->second;
			c_mapConnection.erase(conn);
		}
		c_mtxConnection.Unlock();
	}
protected:
	bool						m_bV4;
};

class CRaopServerV6 : public CRaopServer
{
public:
	BOOL Run(int nPort)
	{
		m_bV4 = false;

		SOCKADDR_IN6 in;

		memset(&in, 0, sizeof(in));

		in.sin6_family		= AF_INET6;
		in.sin6_port		= nPort;
		in.sin6_addr		= in6addr_any;

		return CNetworkServer::Run(in);
	}
};

class CRaopServerV4 : public CRaopServer
{
public:
	BOOL Run()
	{
		m_bV4 = true;

		int		nPort	= 5000;
		BOOL	bResult = FALSE;
		int		nTry	= 10;

		do 
		{
			bResult	= CNetworkServer::Run(htons(nPort++));
		}
		while(!bResult && nTry--);

		return bResult;
	}
};

static CRaopServerV6			raop_v6_server;
static CRaopServerV4			raop_v4_server;
static CDnsSD_Register			dns_sd_register_server;
static CDnsSD_BrowseForService	dns_sd_browse_dacp;

bool is_streaming()
{
	bool bResult = false;

	c_mtxConnection.Lock();

	for (map<SOCKET, CRaopContext*>::iterator i = c_mapConnection.begin(); i != c_mapConnection.end(); ++i)
	{
		if (i->second->m_bIsStreaming)
		{
			bResult = true;
			break;
		}
	}
	c_mtxConnection.Unlock();

	return bResult;
}

bool start_serving()
{
	// try to get hw_addr from registry
	string				strHwAddr;
	CTempBuffer<BYTE>	HwAddr;

	if (	GetValueFromRegistry(HKEY_LOCAL_MACHINE, "HwAddr", strHwAddr)
		&&	DecodeBase64(strHwAddr, HwAddr) == 6
		)
	{
		memcpy(hw_addr, HwAddr, 6);
	}
	else
	{
		for (long i = 1; i < 6; ++i)
		{
			hw_addr[i] = (256 * rand()) / RAND_MAX;
		}
	}
	hw_addr[0] = 0;

	if (!raop_v4_server.Run())
	{
		Log("can't listen on IPv4\n");
		::MessageBoxA(NULL, "Can't start network service on IPv4", strConfigName.c_str(), MB_ICONERROR|MB_OK);
		return false;
	}
	if (!Log("listening at least on IPv4...port %lu\n", (ULONG)ntohs(raop_v4_server.m_nPort)))
	{
		::MessageBoxA(NULL, "Can't log to file. Please run elevated", strConfigName.c_str(), MB_ICONERROR|MB_OK);
	}
	
	if (raop_v6_server.Run(raop_v4_server.m_nPort))
	{
		Log("listening on IPv6...port %lu\n", (ULONG)ntohs(raop_v6_server.m_nPort));
	}
	else
	{
		Log("can't listen on IPv6. Don't care!\n");
	}
	if (hDnsSD == NULL)
		hDnsSD = LoadLibraryA("dnssd.dll");

	if (hDnsSD == NULL)
	{
		raop_v4_server.Cancel();
		raop_v6_server.Cancel();

		if (IDOK == ::MessageBoxA(NULL, "Please install Bonjour for Windows from here: http://support.apple.com/kb/DL999 \n\nKlick OK to get forwarded", strConfigName.c_str(), MB_ICONINFORMATION|MB_OKCANCEL))
		{
			ShellExecute(NULL, L"open", L"http://support.apple.com/kb/DL999", NULL, NULL, SW_SHOWNORMAL);
		}
		return false;
	}
	libao_file	= NULL;
	libao_hfile	= INVALID_HANDLE_VALUE;

	if (!dlgMain.m_strSoundRedirection.IsEmpty() && dlgMain.m_strSoundRedirection != _T("no redirection"))
	{
		USES_CONVERSION;

		Log("Sound Redirection is: %ls\n", T2CW(dlgMain.m_strSoundRedirection));

		libao_file = "-";

		hRedirProcess = Exec(dlgMain.m_strSoundRedirection, 0, true);

		if (hRedirProcess == NULL)
		{
			libao_hfile	= INVALID_HANDLE_VALUE;

			Log("Sound Redirection Exec failed - got to be file output!\n");

			libao_file = new char[dlgMain.m_strSoundRedirection.GetLength() + 16];

			if (libao_file != NULL)
			{
				CString str(dlgMain.m_strSoundRedirection);

				str.MakeLower();

				strcpy(libao_file, T2CA(str));

				if (strlen(libao_file) < 4 || strcmp(libao_file+strlen(libao_file)-3, ".au") != 0)
					strcat(libao_file, ".au");

				FILE* fh = fopen(libao_file, "wb");

				if (fh == NULL)
				{
					Log("Creating of output file %s failed with error code: %lu!\n", libao_file, GetLastError());
					libao_file = NULL;
				}
				else
					fclose(fh);
			}
		}
	}
	bool bResult = dns_sd_register_server.Start(raop_v4_server.m_nPort);

	if (!bResult)
	{
		long nTry = 10;

		do
		{
			Log("Could not register Raop.Tcp Service on Port %lu with dnssd.dll\n", (ULONG)ntohs(raop_v4_server.m_nPort));

			Sleep(1000);
			bResult = dns_sd_register_server.Start(raop_v4_server.m_nPort);
			nTry--;
		}
		while(!bResult && nTry > 0);

		if (!bResult)
			::MessageBoxA(NULL, "Could not register Raop.Tcp Service with dnssd.dll", strConfigName.c_str(), MB_ICONERROR);
	}
	if (bResult)
	{
		Log("Registered Raop.Tcp Service ok\n");

		if (!dlgMain.m_bNoMediaControl)
		{
			if (!dns_sd_browse_dacp.Start("_dacp._tcp", WM_DACP_REG))
			{
				Log("Failed to start DACP browser!\n");
				ATLASSERT(FALSE);
			}
			else
			{
				Log("Succeeded to start DACP browser!\n");
			}
		}
		else
		{
			Log("Omitted to start DACP browser - no Media Controls desired\n");
		}
	}
	else
	{
		Log("Raop.Tcp Service failed to register - Dead End!\n");

		raop_v4_server.Cancel();
		raop_v6_server.Cancel();
	}
	return bResult;
}

void stop_serving()
{
	Log("Stopping Services .... ");

	raop_v6_server.Cancel();
	raop_v4_server.Cancel();

	dns_sd_browse_dacp.Stop();
	dns_sd_register_server.Stop();

	if (hRedirProcess != NULL)
	{
		::TerminateProcess(hRedirProcess, 0);
		CloseHandle(hRedirProcess);
		hRedirProcess = NULL;
	}
	Log("stopped ok!\n");
}

int Run(LPTSTR lpstrCmdLine = NULL)
{
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	dlgMain.ReadConfig();

	if (start_serving())
	{
		if (dlgMain.Create(NULL) == NULL)
		{
			ATLTRACE(_T("Main dialog creation failed!\n"));
			stop_serving();
			return 0;
		}
		bool bRunElevated			= lpstrCmdLine != NULL && _tcsstr(lpstrCmdLine, _T("Elevation")) != NULL;
		bool bWithChangeApName		= lpstrCmdLine != NULL && _tcsstr(lpstrCmdLine, _T("WithChangeApName")) != NULL;
		bool bWithChangeExtended	= lpstrCmdLine != NULL && _tcsstr(lpstrCmdLine, _T("WithChangeExtended")) != NULL;

		dlgMain.ShowWindow(dlgMain.m_bStartMinimized && !bRunElevated ? dlgMain.m_bTray ? SW_HIDE : SW_SHOWMINIMIZED : SW_SHOWDEFAULT);

		if (bRunElevated)
			::SetForegroundWindow(dlgMain.m_hWnd);
		if (bWithChangeApName)
			dlgMain.PostMessage(WM_COMMAND, IDC_CHANGENAME);
		if (bWithChangeExtended)
			dlgMain.PostMessage(WM_COMMAND, IDC_EXTENDED_OPTIONS);

		int nRet = theLoop.Run();

		_Module.RemoveMessageLoop();

		stop_serving();

		return nRet;
	}
	return 0;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	USES_CONVERSION;

    bIsWinXP = false;

	OSVERSIONINFOEX Info;

	Info.dwOSVersionInfoSize = sizeof(Info);

	if (GetVersionEx((LPOSVERSIONINFO)&Info))
	{
		if (Info.dwMajorVersion <= 5)
		{
			bIsWinXP = true;
		}
	}

	HRESULT hRes = ::CoInitializeEx(NULL, bIsWinXP ? COINIT_APARTMENTTHREADED : COINIT_MULTITHREADED);
	ATLASSERT(SUCCEEDED(hRes));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	::DefWindowProc(NULL, 0, 0, 0L);

	AtlInitCommonControls(ICC_BAR_CLASSES);	// add flags to support other controls

	setlocale(LC_ALL, "");

	LPCTSTR strConfig = NULL;

	if (lpstrCmdLine != NULL && (strConfig = _tcsstr(lpstrCmdLine, _T("/Config:"))) != NULL)
	{
		string	str;
		long	n = 8;

		while(*(strConfig+n) && *(strConfig+n) != _T(' '))
		{
			str += (char)*(strConfig+n);
			n++;
		}
		Trim(str, "\t\n\r\"");

		if (!str.empty())
		{
			bPrivateConfig	= true;
			strConfigName	= str;
			dlgMain.m_strApName = A2CT(strConfigName.c_str());
		}
	}

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	if (!mtxAppSessionInstance.Create(NULL, FALSE, CString(_T("__")) + CString(A2CT(strConfigName.c_str())) + CString(_T("SessionInstanceIsRunning"))) || WaitForSingleObject(mtxAppSessionInstance, 0) != WAIT_OBJECT_0)
	{
		if (lpstrCmdLine == NULL || _tcsstr(lpstrCmdLine, _T("Elevation")) == NULL)
			::MessageBoxA(NULL, "You can run only one instance of this configuration", strConfigName.c_str(), MB_ICONWARNING|MB_OK);
		::CoUninitialize();
		return -1;
	}
	if (!mtxAppGlobalInstance.Create(NULL, FALSE, CString(_T("Global\\__")) + CString(A2CT(strConfigName.c_str())) + CString(_T("GlobalInstanceIsRunning"))) || WaitForSingleObject(mtxAppGlobalInstance, 0) != WAIT_OBJECT_0)
	{
		::CoUninitialize();
		return -1;
	}

	CTempBuffer<BYTE> pKey;

	long sizeKey = DecodeBase64(strPrivateKey, pKey);
	ATLASSERT(pKey != NULL);

	const unsigned char* _pKey = pKey;

	d2i_RSAPrivateKey(&pRSA, &_pKey, sizeKey);
	ATLASSERT(pRSA != NULL);

	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR			gdiplusToken				= 0;

	gdiplusStartupInput.GdiplusVersion				= 1;
	gdiplusStartupInput.DebugEventCallback			= NULL;
	gdiplusStartupInput.SuppressBackgroundThread	= FALSE;
	gdiplusStartupInput.SuppressExternalCodecs		= FALSE; 
		
	ATLVERIFY(GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) == Ok);

	srand(GetTickCount());

	HMODULE hKernel32 = ::LoadLibraryA("Kernel32.dll");

	if (hKernel32 != NULL)
	{
		pSetThreadExecutionState = (_SetThreadExecutionState) GetProcAddress(hKernel32, "SetThreadExecutionState");
	}
	int nRet = Run(lpstrCmdLine);

	RSA_free(pRSA);

	c_mtxConnection.Lock();

	while (!c_mapConnection.empty())
	{
		delete c_mapConnection.begin()->second;
		c_mapConnection.erase(c_mapConnection.begin());
	}
	c_mtxConnection.Unlock();

	GdiplusShutdown(gdiplusToken);

	if (hKernel32 != NULL)
	{
		FreeLibrary(hKernel32);
	}

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
