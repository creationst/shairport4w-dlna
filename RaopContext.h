/*
 *
 *  RaopContext.h
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

class CRaopContext : public CMyMutex
{
public:
	CRaopContext()
	{
		m_bIsStreaming		= false;
		m_bV4				= true;
		m_bmInfo			= NULL;
		Announce();
	}
	~CRaopContext()
	{
		if (m_bmInfo != NULL)
			delete m_bmInfo;
	}
	void Announce()
	{
		m_lfFreq			= 44100.0l;
		m_nTimeStamp		= ::time(NULL);
		m_nTimeTotal		= 0;
		m_nTimeCurrentPos	= 0;

		m_nDurHours			= 0;
		m_nDurMins			= 0;
		m_nDurSecs			= 0;

		if (m_bmInfo != NULL)
		{
			delete m_bmInfo;
			m_bmInfo = NULL;
		}
	}
	void ResetSongInfos()
	{
		m_str_daap_songalbum.Empty();
		m_str_daap_songartist.Empty();
		m_str_daap_trackname.Empty();
	}
public:
	CHttp					m_CurrentHttpRequest;
	CTempBuffer<BYTE>		m_Aesiv;
	long					m_sizeAesiv;
	CTempBuffer<BYTE>		m_Rsaaeskey;
	long					m_sizeRsaaeskey;
	string					m_strFmtp;
	string					m_strCport;
	string					m_strTport;
	string					m_strSport;
	bool					m_bIsStreaming;
	string					m_strNonce;
	bool					m_bV4;
	list<string>			m_listFmtp;
	double					m_lfFreq;
	time_t					m_nTimeStamp;			// in Secs
	time_t					m_nTimeTotal;			// in Secs
	time_t					m_nTimeCurrentPos;		// in Secs
	long					m_nDurHours;
	long					m_nDurMins;
	long					m_nDurSecs;
	Bitmap*					m_bmInfo;
	WTL::CString			m_str_daap_songalbum;
	WTL::CString			m_str_daap_songartist;
	WTL::CString			m_str_daap_trackname;
};
