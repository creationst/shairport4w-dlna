/*
 *
 *  MyBitmapButton.h
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

class CMyBitmapButton : public CBitmapButtonImpl<CMyBitmapButton>
{
public:
	DECLARE_WND_SUPERCLASS(_T("WTL_BitmapButton"), GetWndClassName())

	CMyBitmapButton(DWORD dwExtendedStyle = BMPBTN_AUTOSIZE, HIMAGELIST hImageList = NULL) : 
		CBitmapButtonImpl<CMyBitmapButton>(dwExtendedStyle, hImageList)
	{
		m_bmNormal		= NULL;
		m_bmDisabled	= NULL;
		m_bmPressed		= NULL;
	}
	~CMyBitmapButton()
	{
	}

	BEGIN_MSG_MAP(CMyBitmapButton)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
		MSG_WM_DESTROY(OnDestroy)
		CHAIN_MSG_MAP(__super) 
	END_MSG_MAP()

	LRESULT OnEraseBackground(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		if (wParam != NULL)
		{
			CDCHandle dcPaint((HDC)wParam);

			CRect rect;
			GetClientRect(&rect);

			CBrush br;

			br.CreateSolidBrush(::GetSysColor(COLOR_3DFACE));

			dcPaint.FillRect(&rect, br);
		}
		return 1;
	}
	void Set(int nNormalID, int PressedID, int nDisabledID);

	void OnDestroy()
	{
		if (m_bmNormal != NULL)
		{
			delete m_bmNormal;
			m_bmNormal = NULL;
		}
		if (m_bmDisabled != NULL)
		{
			delete m_bmDisabled;
			m_bmDisabled = NULL;
		}
		if (m_bmPressed != NULL)
		{
			delete m_bmPressed;
			m_bmPressed = NULL;
		}
	}
public:
	Bitmap*								m_bmNormal;
	Bitmap*								m_bmDisabled;
	Bitmap*								m_bmPressed;
};

