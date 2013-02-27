/*
 *
 *  Networking.h 
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
#pragma once

#include <in6addr.h>
#include <ws2ipdef.h>

#include "myMutex.h"
#include "myThread.h"

class CSocketBase
{
public:
	CSocketBase();
	virtual ~CSocketBase();

	BOOL Create(int af = AF_INET);
	void Attach(SOCKET sd);
	void Detach();
	void Close();

	int  recv(void* pBuf, int nLen);	
	BOOL Send(const void* pBuf, int nLen);	

	void SetBlockMode(BOOL b);
	BOOL IsBlockMode();

	BOOL Connect(const SOCKADDR_IN* pSockAddr, int nSizeofSockAddr);
	BOOL WaitForIncomingData(DWORD dwTimeout);

	void SetLinger(BOOL b, DWORD dwLinger = 0);
	BOOL IsLinger();

public:
	SOCKET	m_sd;
	BOOL	m_bBlock;
};

class CNetworkServer : public CSocketBase,  protected CMyThread
{
public:
	CNetworkServer();
	~CNetworkServer();

	BOOL Run(USHORT nPort, DWORD dwAddr = INADDR_ANY);
	BOOL Run(const SOCKADDR_IN6& in_addr);
	BOOL Cancel();

public:
	USHORT	m_nPort; // network order

protected:
	BOOL Bind(USHORT nPort, DWORD dwAddr = INADDR_ANY);
	BOOL Bind(const SOCKADDR_IN6& in_addr);
	BOOL Listen(int backlog = SOMAXCONN);
	void Accept(SOCKET sd);

	virtual bool OnAccept(SOCKET sd)
	{
		return true;
	}
	virtual void OnDisconnect(SOCKET sd)
	{
	}
	virtual void OnRequest(SOCKET sd) = 0;

	virtual void OnEvent();
	virtual BOOL OnStart();

	class CCThread : public CMyThread, public CSocketBase
	{
		protected:
			virtual void OnEvent();
			virtual BOOL OnStart();
		public:
			CNetworkServer*	m_pServer;
	};
	friend class CCThread;

protected:
	map<SOCKET, CCThread*>	m_clients;
	CMyMutex				m_cMutex;
};

