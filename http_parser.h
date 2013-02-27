/*
 *
 *  http_parser.h
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

class CHttp
{
public:
	CHttp()
	{
		m_nStatus = 0;
	}
	~CHttp()
	{
	}
	bool Parse()
	{
		if (m_strBuf.length() == 0)
			return false;

		char* pBuf = new char[m_strBuf.length() + 1];
		
		if (pBuf != NULL)
		{
			map<long, string> mapByLine;

			string::size_type nSep = m_strBuf.find("\r\n\r\n");

			if (nSep == string::npos)
			{
				strcpy(pBuf, m_strBuf.c_str());
				m_strBody.clear();
			}
			else
			{
				strcpy(pBuf, m_strBuf.substr(0, nSep).c_str());
				m_strBody = m_strBuf.substr(nSep+4);
			}

			char* p = strtok(pBuf, "\r\n");

			long n = 0;

			while (p != NULL)
			{
				if (strlen(p) > 0)
					mapByLine[n++] = p;

				p = strtok(NULL, "\r\n");
			}
			for (map<long, string>::iterator i = mapByLine.begin(); i != mapByLine.end(); ++i)
			{
				if (i == mapByLine.begin())
				{
					strcpy(pBuf, i->second.c_str());

					p = strtok(pBuf, " \t");

					if (p != NULL)
					{
						m_strMethod = p;
	
						p = strtok(NULL, " \t");

						if (p != NULL)
						{
							m_strUrl = p;

							p = strtok(NULL, " \t");

							if (p != NULL)
							{
								m_strProtocol = p;
							}
						}
					}
					else
					{
						// improve parser!
						ATLASSERT(FALSE);
					}
				}
				else
				{
					string::size_type n = i->second.find(':');

					if (n != string::npos)
					{
						ci_string	strTok		= i->second.substr(0, n).c_str();
						string		strValue	= i->second.substr(n+1);

						Trim(strTok);
						Trim(strValue);

						m_mapHeader[strTok] = strValue;
					}
				}
			}
			delete [] pBuf;
			return true;
		}
		return false;
	}
	void Create(LPCSTR strProtocol = "HTTP/1.1", long nStatus = 200, const char* strStatus = "OK")
	{
		m_nStatus		= nStatus;
		m_strProtocol	= strProtocol;
		m_strStatus		= strStatus != NULL ? strStatus : "OK";
	}
	void SetStatus(long nStatus, const char* strStatus)
	{
		m_nStatus		= nStatus;
		m_strStatus		= strStatus;
	}
	string GetAsString(bool bResponse = true)
	{
		string strResult;

		char buf[1024];

		if (bResponse)
		{
			sprintf_s(buf, 256, "%s %ld %s\r\n", m_strProtocol.c_str(), m_nStatus, m_strStatus.c_str());
			strResult += buf;
		}
		else
		{
			sprintf_s(buf, 256, "%s %s %s\r\n", m_strMethod.c_str(), m_strUrl.c_str(), m_strProtocol.c_str());
			strResult += buf;
		}
		for (map<ci_string, string>::iterator i = m_mapHeader.begin(); i != m_mapHeader.end(); ++i)
		{
			strResult += i->first.c_str();
			strResult += ": ";
			strResult += i->second;
			strResult += "\r\n";
		}
		if (!bResponse)
			strResult += "\r\n";
		return strResult;
	}
	void InitNew()
	{
		m_mapHeader.clear();
		m_strMethod.clear();
		m_strUrl.clear();
		m_strProtocol.clear();
		m_strBody.clear();
		m_nStatus= 0;
		m_strStatus.clear();
		m_strBuf.clear();
	}
public:
	map<ci_string, string>	m_mapHeader;
	string					m_strMethod;
	string					m_strUrl;
	string					m_strProtocol;
	string					m_strBody;
	long					m_nStatus;
	string					m_strStatus;
	string					m_strBuf;
};
