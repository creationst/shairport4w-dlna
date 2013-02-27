/*
 *
 *  ChangeNameDlg.h 
 *
 *  Copyright (C) Frank Friemel, Daniel Latimer - July 2011
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


class ChangeNameDlg : public CDialogImpl<ChangeNameDlg>, public CWinDataExchange<ChangeNameDlg>
{
public:
	enum { IDD = IDD_CHANGEUSER };

	ChangeNameDlg(CString airportName, CString password);

	// Accessors
	CString getAirportName();
	CString getPassword();

protected:
	BEGIN_DDX_MAP(CMainDlg)
		DDX_TEXT(IDC_NAME, m_Name)
		DDX_TEXT(IDC_PASS, m_Password)
		DDX_TEXT(IDC_VERIFYPASS, m_VerifyPassword)
	END_DDX_MAP()

	BEGIN_MSG_MAP(ChangeNameDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOkay)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOkay(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	HICON		m_hIconSmall;
	
	CString m_Name;
	CString m_Password;
	CString m_VerifyPassword;

};
