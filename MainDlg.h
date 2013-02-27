/*
 *
 *  MainDlg.h 
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

#include "PushPinButton.h"
#include "MyBitmapButton.h"
#include "TrayIcon.h"

#define	WM_RAOP_CTX		(WM_APP+1)
#define	WM_INFO_BMP		(WM_APP+2)
#define	WM_PAUSE		(WM_APP+3)
#define	WM_STOP			WM_PAUSE
#define	WM_RESUME		(WM_APP+4)
#define	WM_SONG_INFO	(WM_APP+5)
#define	WM_DACP_REG		(WM_APP+6)
#define	WM_SET_MMSTATE	(WM_APP+7)

class CMainDlg : public CDialogImpl<CMainDlg>, public CUpdateUI<CMainDlg>,
		public CMessageFilter, public CIdleHandler, public CWinDataExchange<CMainDlg>
		, public CTrayIconImpl<CMainDlg>, protected CServiceResolveCallback
{
public:
	typedef enum
	{
		pause,
		play,
		ffw,
		rew,
		skip_next,
		skip_prev
	} typeMMState;

	CMainDlg() : m_ctlInfoBmp(this, 1), m_ctlTimeBarBmp(this, 2)
	{
		m_strApName.LoadStringW(IDR_MAINFRAME);

		m_bStartMinimized		= FALSE;
		m_bAutostart			= FALSE;
		m_bTray					= TRUE;
		m_strPassword			= L"";
		m_strSoundRedirection	= L"no redirection";
		m_bLogToFile			= FALSE;
		m_bNoMetaInfo			= FALSE;
		m_bNoMediaControl		= FALSE;
		m_bInfoPanel			= TRUE;
		m_pRaopContext			= NULL;
		m_bmWmc					= NULL;
		m_bmProgressShadow		= NULL;
		m_bmAdShadow			= NULL;
		m_bmArtShadow			= NULL;
		m_bPin					= TRUE;
		m_strTimeMode			= "time_remaining";
		m_nCurPos				= 0;
		m_bAd					= false;
		m_nMMState				= pause;
		m_bVisible				= false;
		m_bTrayTrackInfo		= TRUE;

		if (libao_deviceid != NULL)
			m_strSoundcardId = libao_deviceid;
	}
	enum { IDD = IDD_MAINDLG };


	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL OnIdle();

	inline bool HasMultimediaControl()
	{
		return !m_strDacpHost.empty();
	}

	BEGIN_UPDATE_UI_MAP(CMainDlg)
		UPDATE_ELEMENT(ID_EDIT_TRAYTRACKINFO, UPDUI_MENUPOPUP)
	END_UPDATE_UI_MAP()

	BEGIN_DDX_MAP(CMainDlg)
		DDX_TEXT(IDC_APNAME, m_strApName)
		DDX_TEXT(IDC_PASSWORD, m_strPassword)
		DDX_CHECK(IDC_START_MINIMIZED, m_bStartMinimized)
		DDX_CHECK(IDC_AUTOSTART, m_bAutostart)
		DDX_CHECK(IDC_TRAY, m_bTray)
		DDX_CHECK(IDC_INFOPANEL, m_bInfoPanel)
		DDX_TEXT(IDC_STATIC_TIME_INFO, m_strTimeInfo)
		DDX_TEXT(IDC_STATIC_TIME_INFO_CURRENT, m_strTimeInfoCurrent)
		DDX_CONTROL(IDC_MM_PLAY, m_ctlPlay)
		DDX_CONTROL(IDC_MM_SKIP_PREV, m_ctlSkipPrev)
		DDX_CONTROL(IDC_MM_REW, m_ctlRew)
		DDX_CONTROL(IDC_MM_PAUSE, m_ctlPause)
		DDX_CONTROL(IDC_MM_FFW, m_ctlFFw)
		DDX_CONTROL(IDC_MM_SKIP_NEXT, m_ctlSkipNext)
	END_DDX_MAP()

public:
	HICON								m_hIconSmall;
	CString								m_strApName;
	CString								m_strPassword;
	CString								m_strSoundRedirection;
	string								m_strSoundcardId;
	string								m_strHostname;
	BOOL								m_bStartMinimized;
	BOOL								m_bAutostart;
	BOOL								m_bTray;
	BOOL								m_bLogToFile;
	BOOL								m_bNoMetaInfo;
	BOOL								m_bNoMediaControl;

	BOOL								m_bInfoPanel;
	BOOL								m_bPin;
	CRect								m_rectOrig;
	HCURSOR								m_hHandCursor;
	Bitmap*								m_bmWmc;
	Bitmap*								m_bmProgressShadow;
	Bitmap*								m_bmAdShadow;
	Bitmap*								m_bmArtShadow;
	CString								m_strTimeInfo;
	CString								m_strTimeInfoCurrent;
	string								m_strTimeMode;
	int									m_nCurPos;
	bool								m_bAd;
	bool								m_bVisible;
	BOOL								m_bTrayTrackInfo;

	ci_string							m_strActiveRemote;
	ci_string							m_strDACP_ID;

protected:
	ci_string							m_strDacpHost;
	USHORT								m_nDacpPort;
	typeMMState							_m_nMMState;
	__declspec(property(get=GetMMState,put=PutMMState))	typeMMState	m_nMMState;

	inline typeMMState GetMMState()
	{
		return _m_nMMState;
	}
	void PutMMState(typeMMState nMmState);

public:
	CButton								m_ctlAutostart;
	CButton								m_ctlUpdate;
	CButton								m_ctlSet;
	CButton								m_ctlMinimized;
	CButton								m_ctlTray;
	CEdit								m_ctlName;
	CEdit								m_ctlPassword;
	CStatic								m_ctlPanelFrame;
	CStatic								m_ctlControlFrame;
	CStatic								m_ctlTimeInfo;
	CContainedWindowT<CStatic>			m_ctlInfoBmp;
	CContainedWindowT<CStatic>			m_ctlTimeBarBmp;
	CEdit								m_ctlArtist;
	CEdit								m_ctlAlbum;
	CEdit								m_ctlTrack;
	CToolTipCtrl						m_ctlToolTipTrackName;
	CToolTipCtrl						m_ctlToolTipAlbumName;
	CToolTipCtrl						m_ctlToolTipArtistName;
	CPushPinButton						m_ctlPushPin;
	CRaopContext*						m_pRaopContext;

	CMyBitmapButton						m_ctlPlay;
	CMyBitmapButton						m_ctlPause;
	CMyBitmapButton						m_ctlFFw;
	CMyBitmapButton						m_ctlRew;
	CMyBitmapButton						m_ctlSkipNext;
	CMyBitmapButton						m_ctlSkipPrev;

	void ReadConfig();
	bool WriteConfig();

	BEGIN_MSG_MAP_EX(CMainDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER_EX(IDC_CHANGENAME, OnClickedSetApName)
		COMMAND_ID_HANDLER_EX(IDC_START_MINIMIZED, OnClickedMinimizeAtStartup)
		COMMAND_ID_HANDLER_EX(IDC_AUTOSTART, OnClickedAutostart)
		COMMAND_ID_HANDLER_EX(IDC_UPDATE, OnClickedUpdate)
		MSG_WM_SYSCOMMAND(OnSysCommand)
		COMMAND_ID_HANDLER_EX(IDC_SHOW, OnShow)
		COMMAND_ID_HANDLER_EX(IDC_TRAY, OnClickedTrayOption)
		COMMAND_ID_HANDLER_EX(IDC_EXTENDED_OPTIONS, OnClickedExtendedOptions)
		COMMAND_ID_HANDLER_EX(IDC_INFOPANEL, OnClickedInfoPanel)
		COMMAND_ID_HANDLER_EX(ID_EDIT_TRAYTRACKINFO, OnEditTrayTrackInfo)
		MESSAGE_HANDLER(WM_RAOP_CTX, OnRaopContext)
		MESSAGE_HANDLER(WM_INFO_BMP, OnInfoBitmap)
		MESSAGE_HANDLER(WM_PAUSE, Pause)
		MESSAGE_HANDLER(WM_RESUME, Resume)
		MESSAGE_HANDLER(WM_SONG_INFO, OnSongInfo)
		MESSAGE_HANDLER(WM_DACP_REG, OnDACPRegistered)
		MESSAGE_HANDLER(WM_SET_MMSTATE, OnSetMultimediaState)
		MSG_WM_TIMER(OnTimer)
		COMMAND_ID_HANDLER_EX(IDC_PUSH_PIN, OnPin)
		MSG_WM_SHOWWINDOW(OnShowWindow)
		MSG_WM_LBUTTONDOWN(OnLButtonDown)
		MSG_WM_ACTIVATE(OnActivate)
		MSG_WM_SETCURSOR(OnSetCursor)
		COMMAND_ID_HANDLER_EX(IDC_MM_PLAY,		OnPlay)
		COMMAND_ID_HANDLER_EX(IDC_MM_SKIP_PREV, OnSkipPrev)
		COMMAND_ID_HANDLER_EX(IDC_MM_REW,		OnRewind)
		COMMAND_ID_HANDLER_EX(IDC_MM_PAUSE,		OnPause)
		COMMAND_ID_HANDLER_EX(IDC_MM_FFW,		OnForward)
		COMMAND_ID_HANDLER_EX(IDC_MM_SKIP_NEXT, OnSkipNext)
		MSG_WM_APPCOMMAND(OnAppCommand)
		MESSAGE_HANDLER(WM_POWERBROADCAST, OnPowerBroadcast)
		REFLECT_NOTIFICATIONS()
		CHAIN_MSG_MAP(CUpdateUI<CMainDlg>)
		CHAIN_MSG_MAP(CTrayIconImpl<CMainDlg>) 
	ALT_MSG_MAP(1)
		MSG_WM_PAINT(OnInfoBmpPaint)
	ALT_MSG_MAP(2)
		MSG_WM_PAINT(OnTimeBarBmpPaint)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	void OnClickedSetApName(UINT, int, HWND);
	void OnClickedMinimizeAtStartup(UINT, int, HWND);
	void OnClickedAutostart(UINT, int, HWND);
	void OnClickedUpdate(UINT, int, HWND);
	void OnSysCommand(UINT wParam, CPoint pt);
	void OnShow(UINT, int, HWND);
	void OnClickedTrayOption(UINT, int, HWND);
	void OnClickedExtendedOptions(UINT, int, HWND);
	LRESULT OnRaopContext(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	void OnClickedInfoPanel(UINT, int, HWND);
	void OnEditTrayTrackInfo(UINT, int, HWND);
	LRESULT OnInfoBitmap(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	void OnInfoBmpPaint(HDC wParam);
	void OnTimeBarBmpPaint(HDC wParam);
	BOOL OnSetCursor(CWindow wnd, UINT nHitTest, UINT message);
	void OnTimer(UINT_PTR nIDEvent);
	LRESULT Pause(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT Resume(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSongInfo(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDACPRegistered(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnSetMultimediaState(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	void OnPin(UINT, int nID, HWND);
	void OnShowWindow(BOOL bShow, UINT nStatus);
	void OnLButtonDown(UINT nFlags, CPoint point);
	void OnActivate(UINT nState, BOOL bMinimized, CWindow wndOther);
	LRESULT OnPowerBroadcast(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	BOOL OnAppCommand(CWindow wndFocus, short cmd, WORD uDevice, int dwKeys);

	void OnPlay(UINT, int nID, HWND);
	void OnSkipPrev(UINT, int nID, HWND);
	void OnRewind(UINT, int nID, HWND);
	void OnPause(UINT, int nID, HWND);
	void OnForward(UINT, int nID, HWND);
	void OnSkipNext(UINT, int nID, HWND);

	void CloseDialog(int nVal);

protected:
	void RestartService();
	void Elevate(bool bWithChangeApName = false, bool bWithChangeExtended = false);
	bool SendDacpCommand(const char* strCmd);

	inline void Pause()
	{
		BOOL bDummy;
		Pause(0, 0, 0, bDummy);
	}
	inline void Resume()
	{
		BOOL bDummy;
		Resume(0, 0, 0, bDummy);
	}
	virtual void OnServiceResolved(const char* strHost, USHORT nPort);
};
