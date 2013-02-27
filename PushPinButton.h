/*
 *
 *  PushPinButton.h
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

class CPushPinButton : public CWindowImpl<CButton>
{
public:
	CPushPinButton();
   
	void SetPinned(BOOL bPinned);
	BOOL IsPinned() { return m_bPinned; };
   
	void ReloadBitmaps(); 
   
	BOOL SubclassWindow(HWND hWnd);

protected:
	BEGIN_MSG_MAP_EX(CPushPinButton)
		MSG_OCM_DRAWITEM(OnDrawItem)
	END_MSG_MAP()
   
	void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
   
	void SizeToContent();
	void LoadBitmaps();
   
	BOOL    m_bPinned;
	Bitmap*	m_pPinnedBitmap;
	Bitmap*	m_pUnPinnedBitmap;
	CRect   m_MaxRect;
};
