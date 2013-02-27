/*
 *
 *  TrayIcon.h
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

#define WM_TRAYICON		(WM_APP+0)

template <class T>
class CTrayIconImpl
{
public:	
	CTrayIconImpl()
	{
		m_bInit			= false;
		memset(&m_nid, 0, sizeof(m_nid));
	}
	~CTrayIconImpl()
	{
		RemoveTray();
	}
	bool InitTray(LPCTSTR lpszToolTip, HICON hIcon, UINT nID)
	{
		if (m_bInit)
			return true;

		T* pT = static_cast<T*>(this);

		_tcscpy(m_nid.szTip, lpszToolTip);

		m_nid.cbSize			= sizeof(m_nid);
		m_nid.hWnd				= pT->m_hWnd;
		m_nid.uID				= nID;
		m_nid.hIcon				= hIcon;
		m_nid.uFlags			= NIF_MESSAGE | NIF_ICON | NIF_TIP;
		m_nid.uCallbackMessage	= WM_TRAYICON;
		m_bInit					= Shell_NotifyIcon(NIM_ADD, &m_nid) ? true : false;

		return m_bInit;
	}
	void SetTooltipText(LPCTSTR pszTooltipText)
	{
		if (m_bInit && pszTooltipText != NULL)
		{
			m_nid.uFlags = NIF_TIP;
			_tcscpy(m_nid.szTip, pszTooltipText);

			Shell_NotifyIcon(NIM_MODIFY, &m_nid);
		}
	}
	void SetInfoText(LPCTSTR pszInfoTitle, LPCTSTR pszInfoText)
	{
		if (m_bInit && pszInfoText != NULL)
		{
			m_nid.uFlags		= NIF_INFO;
			m_nid.dwInfoFlags	= NIIF_INFO;
			m_nid.uTimeout		= 15000;

			if (pszInfoTitle != NULL)
				_tcscpy_s(m_nid.szInfoTitle, _countof(m_nid.szInfoTitle), pszInfoTitle);
			_tcscpy_s(m_nid.szInfo, _countof(m_nid.szInfo), pszInfoText);

			Shell_NotifyIcon(NIM_MODIFY, &m_nid);
		}
	}
	void RemoveTray()
	{
		if (m_bInit)
		{
			m_nid.uFlags = 0;
			Shell_NotifyIcon(NIM_DELETE, &m_nid);
			m_bInit = false;
		}
	}

	BEGIN_MSG_MAP(CTrayIconImpl<T>)
		MESSAGE_HANDLER(WM_TRAYICON, OnTrayIcon)
	END_MSG_MAP()

	LRESULT OnTrayIcon(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		if (wParam != m_nid.uID)
			return 0;

		T* pT = static_cast<T*>(this);

		if (LOWORD(lParam) == WM_RBUTTONUP)
		{
			CMenu Menu;

			if (!Menu.LoadMenu(m_nid.uID))
				return 0;

			CMenuHandle Popup(Menu.GetSubMenu(0));

			CPoint pos;
			GetCursorPos(&pos);

			SetForegroundWindow(pT->m_hWnd);

			Popup.SetMenuDefaultItem(0, TRUE);

			Popup.TrackPopupMenu(TPM_LEFTALIGN, pos.x, pos.y, pT->m_hWnd);
			pT->PostMessage(WM_NULL);
			Menu.DestroyMenu();
		}
		else if (LOWORD(lParam) == WM_LBUTTONDBLCLK)
		{
			::ShowWindow(pT->m_hWnd, SW_SHOWNORMAL);
			::SetForegroundWindow(pT->m_hWnd);
		}
		return 0;
	}
public:
	NOTIFYICONDATA		m_nid;
	bool				m_bInit;
};
