/*
 *
 *  ChangeNameDlg.cpp 
 *
 *  Copyright (C) Frank Friemel, Daniel Latimer - April 2011
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

#include "changenamedlg.h"

ChangeNameDlg::ChangeNameDlg(CString airportName, CString password) :
	m_Name(airportName), m_Password(password), m_VerifyPassword(password)
{
}

CString ChangeNameDlg::getAirportName()
{
	return m_Name;
}

CString ChangeNameDlg::getPassword()
{
	return m_Password;
}

LRESULT ChangeNameDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CenterWindow(GetParent());

	// set icons
	HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	SetIcon(hIcon, TRUE);
	m_hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(m_hIconSmall, FALSE);

	DoDataExchange(FALSE);

	return TRUE;
}

LRESULT ChangeNameDlg::OnOkay(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	DoDataExchange(TRUE);

	if(m_Password != m_VerifyPassword)
	{
		MessageBox(_T("Password fields do not contain matching strings."), _T("Error"), MB_ICONERROR | MB_OK);
		return 0;
	}

	this->EndDialog(TRUE);
	return 0;
}

LRESULT ChangeNameDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	this->EndDialog(FALSE);
	return 0;
}
