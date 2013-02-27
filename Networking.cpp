/*
 *
 *  Networking.cpp
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
#include "Networking.h"

static int _send(SOCKET sd, const void* pBuf, int nLen)
{
	fd_set set;
	fd_set eset;
	   
	FD_ZERO(&set);
	FD_ZERO(&eset);
	FD_SET(sd, &set);
	FD_SET(sd, &eset);

	int nResult = select(0, NULL, &set, &eset, NULL);

	if (nResult != SOCKET_ERROR)
	{
		if (nResult == 0)
		   return 0;

		if (FD_ISSET(sd, &set))
			nResult = ::send(sd, (const char*)pBuf, nLen, 0);
		else
		{
			if (WSAGetLastError() == WSAENOTSOCK)
			{
				nResult = SOCKET_ERROR;
			}
		}
	}
	return nResult;
}

static int _recv(SOCKET sd, void* pBuf, int nLen)
{
	fd_set set;
	fd_set eset;

	FD_ZERO(&set);
	FD_ZERO(&eset);
	FD_SET(sd, &set);
	FD_SET(sd, &eset);

	int nResult = select(0, &set, NULL, &eset, NULL);

	if (nResult != SOCKET_ERROR)
	{
		if (nResult == 0)
		   return 0;

		if (FD_ISSET(sd, &set))
			nResult = ::recv(sd, (char*)pBuf, nLen, 0);
		else
		{
			if (WSAGetLastError() == WSAENOTSOCK)
			{
				nResult = SOCKET_ERROR;
			}
		}
	}
	return nResult;
}

CSocketBase::CSocketBase()
{
	m_sd		= INVALID_SOCKET;
	m_bBlock	= TRUE;

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void CSocketBase::Attach(SOCKET sd)
{
	ATLASSERT(m_sd == INVALID_SOCKET);
	m_sd = sd;
}

void CSocketBase::Detach()
{
	ATLASSERT(m_sd != INVALID_SOCKET);
	m_sd = INVALID_SOCKET;
}

CSocketBase::~CSocketBase()
{
	if (m_sd != INVALID_SOCKET)
	   Close();
}

BOOL CSocketBase::Create(int af)
{
	if (m_sd != INVALID_SOCKET)
		Close();

	m_sd		= socket(af, SOCK_STREAM, 0);
	m_bBlock	= TRUE;

	return m_sd != INVALID_SOCKET ? TRUE : FALSE;
}

BOOL CSocketBase::Connect(const SOCKADDR_IN* pSockAddr, int nSizeofSockAddr)
{
	BOOL bResult = TRUE;

	SetBlockMode(TRUE);

	if (connect(m_sd, (struct sockaddr *)pSockAddr, nSizeofSockAddr) == SOCKET_ERROR)
	{
		return FALSE;
	}
	return TRUE;
}

BOOL CSocketBase::WaitForIncomingData(DWORD dwTimeout)
{
	fd_set			read_set;
	fd_set			error_set;
	struct timeval	timer;
	   
	timerclear(&timer);
	
	if (dwTimeout != INFINITE)
	{
	   timer.tv_sec		= dwTimeout / 1000;
	   timer.tv_usec	= (dwTimeout % 1000) * 1000;
	}
	FD_ZERO(&read_set);
	FD_ZERO(&error_set);
	FD_SET(m_sd, &read_set);
	FD_SET(m_sd, &error_set);

	int nResult = select(0, &read_set, NULL, &error_set, (dwTimeout != INFINITE) ? &timer : NULL);

	if (nResult == SOCKET_ERROR || (FD_ISSET(m_sd, &error_set) && WSAGetLastError() == WSAENOTSOCK))
	{
		return FALSE;
	}
	return nResult > 0 ? TRUE : FALSE;
}

void CSocketBase::Close()
{
	if (m_sd != INVALID_SOCKET)
	{
		if (IsLinger())
			SetLinger(FALSE);
		closesocket(m_sd);
		m_sd = INVALID_SOCKET;
	}
}
			 
void CSocketBase::SetLinger(BOOL b, DWORD dwLinger)
{
	LINGER l;

	l.l_onoff = b;
	l.l_linger = (unsigned short)(dwLinger / 1000);

	setsockopt(m_sd, SOL_SOCKET, SO_LINGER, (const char*)&l, sizeof(l));
}

BOOL CSocketBase::IsLinger()
{
	LINGER	l;
	int		size = sizeof(l);

	getsockopt(m_sd, SOL_SOCKET, SO_LINGER, (char*)&l, &size);

	return l.l_onoff != 0;
}

void CSocketBase::SetBlockMode(BOOL b)
{
	DWORD dw = (b) ? FALSE : TRUE;

	ioctlsocket(m_sd, FIONBIO, &dw);
	m_bBlock = b;
}

BOOL CSocketBase::IsBlockMode()
{
	return m_bBlock;
}

BOOL CSocketBase::Send(const void* pBuf, int nLen)
{
	int n	= 0;
	int nW	= 0;

	while(n < nLen)
	{
		if ((nW = _send(m_sd, ((const char*)pBuf)+n, nLen-n)) == 0)
			return FALSE;
		if (nW == SOCKET_ERROR)
		{
			int nLastErr = WSAGetLastError();

			if (nLastErr == WSAEWOULDBLOCK)
			{
				Sleep(100);
				continue;
			}
			return FALSE;
	   }
	   n += nW;
	}
	return n == nLen;
}

int CSocketBase::recv(void* pBuf, int nLen)
{
	return _recv(m_sd, pBuf, nLen);
}

CNetworkServer::CNetworkServer()
{
	m_nPort	= 0;
}

CNetworkServer::~CNetworkServer()
{
	if (IsRunning())
	   Cancel();
}

BOOL CNetworkServer::Bind(USHORT nPort, DWORD dwAddr)
{
	struct sockaddr_in laddr_in;

	memset(&laddr_in, 0, sizeof(laddr_in));

	laddr_in.sin_family		 = AF_INET;
	laddr_in.sin_addr.s_addr = dwAddr;
	laddr_in.sin_port		 = nPort;

	if (bind(m_sd, (LPSOCKADDR)&laddr_in, sizeof(laddr_in)) == SOCKET_ERROR)
		return FALSE;

	m_nPort = nPort;
	return TRUE;
}

BOOL CNetworkServer::Bind(const SOCKADDR_IN6& in_addr)
{
	if (bind(m_sd, (const struct sockaddr*)&in_addr, sizeof(in_addr)) == SOCKET_ERROR)
		return FALSE;
	m_nPort = in_addr.sin6_port;
	return TRUE;
}

BOOL CNetworkServer::Listen(int backlog)
{
	return listen(m_sd, backlog) != SOCKET_ERROR ? TRUE : FALSE;
}

BOOL CNetworkServer::Run(USHORT nPort, DWORD dwAddr)
{
	if (IsRunning())
	   return FALSE;

	if (!Create(AF_INET))
		return FALSE;

	if (!Bind(nPort, dwAddr))
	{
		Close();
		return FALSE;
	}
	if (!Listen())
	{
		Close();
		return FALSE;
	}
	return Start();
}

BOOL CNetworkServer::Run(const SOCKADDR_IN6& in_addr)
{
	if (IsRunning())
	   return FALSE;

	if (!Create(AF_INET6))
		return FALSE;

	if (!Bind(in_addr))
	{
		Close();
		return FALSE;
	}
	if (!Listen())
	{
		Close();
		return FALSE;
	}
	return Start();
}

BOOL CNetworkServer::OnStart()
{
	ATLASSERT(m_hEvent != NULL);

	if (IsBlockMode())
	   SetBlockMode(FALSE);

	CloseHandle(m_hEvent);
	m_hEvent = WSACreateEvent();

	if (m_hEvent == WSA_INVALID_EVENT)
	{
		ATLASSERT(FALSE);
		return FALSE;
	}
	if (WSAEventSelect(m_sd, m_hEvent, FD_ACCEPT) != 0)
	{
		ATLASSERT(FALSE);
		return FALSE;
	}
	return TRUE;
}

void CNetworkServer::OnEvent()
{
	WSANETWORKEVENTS NetworkEvents;

	if (WSAEnumNetworkEvents(m_sd, m_hEvent, &NetworkEvents) == 0)
	{
		if (NetworkEvents.lNetworkEvents & FD_ACCEPT)
		{
			ResetEvent(m_hEvent);

			SOCKET sd = accept(m_sd, NULL, NULL);

			if (sd != INVALID_SOCKET)
			{
				WSAEventSelect(sd, NULL, 0);

				DWORD dw = TRUE;

				if (ioctlsocket(sd, FIONBIO, &dw) == SOCKET_ERROR)
				{
					ATLASSERT(FALSE);
				}
				Accept(sd);
			}
		}
	}
}

void CNetworkServer::Accept(SOCKET sd)
{
	m_cMutex.Lock();

	map<SOCKET, CCThread*>	::iterator i = m_clients.find(sd);
	
	if (i != m_clients.end())
	{
		CCThread* pC = i->second;
		m_clients.erase(i);

		m_cMutex.Unlock();

		OnDisconnect(sd);
		pC->Stop();
		delete pC;
	}
	else
		m_cMutex.Unlock();

	if (OnAccept(sd))
	{
		CCThread* pClient = new CCThread;

		pClient->m_pServer	= this;
		pClient->Attach(sd);

		m_cMutex.Lock();
		m_clients[sd] = pClient;
		m_cMutex.Unlock();

		pClient->Start();
	}
	else
	{
		CSocketBase sock;

		sock.Attach(sd);
		sock.Close();
	}
}

BOOL CNetworkServer::Cancel()
{
	if (!IsRunning())
	   return FALSE;

 	m_cMutex.Lock();

	map<SOCKET, CCThread*>::iterator i;
	
	while ((i = m_clients.begin()) != m_clients.end())
	{
		CCThread* pC = i->second;

		m_clients.erase(i);
	 	m_cMutex.Unlock();

		pC->Stop();
		OnDisconnect(pC->m_sd);

		delete pC;

	 	m_cMutex.Lock();
	}
 	m_cMutex.Unlock(); 

	Stop();
	Close();
	return TRUE;
}

BOOL CNetworkServer::CCThread::OnStart()
{
	ATLASSERT(m_pServer != NULL);
	ATLASSERT(m_hEvent != NULL);

	CloseHandle(m_hEvent);
	m_hEvent = WSACreateEvent();

	if (m_hEvent == WSA_INVALID_EVENT)
	{
		ATLASSERT(FALSE);
		return FALSE;
	}
	if (WSAEventSelect(m_sd, m_hEvent, FD_READ|FD_CLOSE) != 0)
	{
		ATLASSERT(FALSE);
		return FALSE;
	}
	return TRUE;
}

void CNetworkServer::CCThread::OnEvent()
{
	WSANETWORKEVENTS	NetworkEvents;
	bool				bClose = false;

	if (WSAEnumNetworkEvents(m_sd, m_hEvent, &NetworkEvents) == 0)
	{
  		if (NetworkEvents.lNetworkEvents & FD_READ)
		{
			ResetEvent(m_hEvent);

			DWORD dw = 0;

			ioctlsocket(m_sd, FIONREAD, &dw);

			if (dw > 0)
				m_pServer->OnRequest(m_sd);
			else
			{
				bClose = true;
			}
		}
		if (NetworkEvents.lNetworkEvents & FD_CLOSE)
		{
			ResetEvent(m_hEvent);
			bClose = true;
		}
	}
	else
		bClose = true;

	if (bClose)
	{
		map<SOCKET, CCThread*>::iterator	i;
		bool								bFound = false;

		m_pServer->m_cMutex.Lock();

		for (i = m_pServer->m_clients.begin(); i != m_pServer->m_clients.end(); ++i)
		{
			if (i->second == this)
			{
				m_pServer->m_clients.erase(i);
				bFound = true;
				break;
			}
		}
		m_pServer->m_cMutex.Unlock();

		if (bFound)
		{
			m_pServer->OnDisconnect(m_sd);
			Close();

			SetEvent(m_hStopEvent);
			m_bSelfDelete = true;
		}
	}
}


