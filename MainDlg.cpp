/*
 *
 *  MainDlg.cpp 
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
#include "resource.h"

#include "aboutdlg.h"
#include "MainDlg.h"
#include "ExtOptsDlg.h"
#include "ChangeNameDlg.h"

static const CString	c_strEmptyTimeInfo = _T("--:--");

#define STR_EMPTY_TIME_INFO c_strEmptyTimeInfo
#define	REPLACE_CONTROL(__ctl,___offset) 	__ctl.GetClientRect(&rect);							\
											__ctl.MapWindowPoints(m_hWnd, &rect);				\
											__ctl.SetWindowPos(NULL, rect.left, rect.top+(___offset), 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_FRAMECHANGED); 

Bitmap* BitmapFromResource(HINSTANCE hInstance, LPCTSTR strID, LPCTSTR strType /* = _T("PNG") */)
{
	Bitmap* pResult		= NULL;
	HRSRC	hResource	= FindResource(hInstance, strID, strType);

	if (hResource != NULL)
	{
		DWORD	nSize			= SizeofResource(hInstance, hResource);
	    HANDLE	hResourceMem	= LoadResource(hInstance, hResource);
	    BYTE*	pData			= (BYTE*)LockResource(hResourceMem);

		if (pData != NULL)
		{
			HGLOBAL hBuffer  = GlobalAlloc(GMEM_MOVEABLE, nSize);

			if (hBuffer != NULL) 
			{
				void* pBuffer = GlobalLock(hBuffer);
				
				if (pBuffer != NULL)
				{
					CopyMemory(pBuffer, pData, nSize);
					GlobalUnlock(hBuffer);

					IStream* pStream = NULL;
			
					if (SUCCEEDED(CreateStreamOnHGlobal(hBuffer, TRUE, &pStream)))
					{
						pResult = Bitmap::FromStream(pStream);
						pStream->Release();
					}
					else
						GlobalFree(hBuffer);
				}
				else
					GlobalFree(hBuffer);
			}
		}
	}
	return pResult;
}

static BOOL CreateToolTip(HWND nWndTool, CToolTipCtrl& ctlToolTipCtrl, int nIDS)
{
	ATLASSERT(ctlToolTipCtrl.m_hWnd == NULL);

	ctlToolTipCtrl.Create(::GetParent(nWndTool));

	CToolInfo ti(TTF_SUBCLASS, nWndTool, 0, NULL, MAKEINTRESOURCE(nIDS));
	
	return ctlToolTipCtrl.AddTool(ti);
}

static BOOL RunAsAdmin(HWND hWnd, LPCTSTR lpFile, LPCTSTR lpParameters)
{
	mtxAppSessionInstance.Release();
	mtxAppGlobalInstance.Release();

    SHELLEXECUTEINFO   sei;
    
	ZeroMemory( &sei, sizeof(sei) );

    sei.cbSize          = sizeof(SHELLEXECUTEINFOW);
    sei.hwnd            = hWnd;
    sei.fMask           = SEE_MASK_FLAG_DDEWAIT | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb          = _TEXT("runas");
    sei.lpFile          = lpFile;
    sei.lpParameters    = lpParameters;
    sei.nShow           = SW_SHOWNORMAL;

	return ShellExecuteEx(&sei);
}

bool IsAdmin()
{
	typedef BOOL (WINAPI *_IsUserAnAdmin)(void);

	_IsUserAnAdmin IsUserAnAdmin = NULL;
	
	HINSTANCE hDll = LoadLibrary(_T("shell32.dll"));
				  
	if (hDll != NULL)
	{
		IsUserAnAdmin = (_IsUserAnAdmin)GetProcAddress(hDll, "IsUserAnAdmin");

		if (IsUserAnAdmin == NULL)
			IsUserAnAdmin = (_IsUserAnAdmin)GetProcAddress(hDll, (LPCSTR)0x02A8);

		if (IsUserAnAdmin != NULL)
			return IsUserAnAdmin() ? true : false;
	}
	return true;
}

typedef DWORD _ARGB;

static void InitBitmapInfo(__out_bcount(cbInfo) BITMAPINFO *pbmi, ULONG cbInfo, LONG cx, LONG cy, WORD bpp)
{
	ZeroMemory(pbmi, cbInfo);

	pbmi->bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biPlanes		= 1;
	pbmi->bmiHeader.biCompression	= BI_RGB;
	pbmi->bmiHeader.biWidth			= cx;
	pbmi->bmiHeader.biHeight		= cy;
	pbmi->bmiHeader.biBitCount		= bpp;
}

static HRESULT Create32BitHBITMAP(HDC hdc, const SIZE *psize, __deref_opt_out void **ppvBits, __out HBITMAP* phBmp)
{
	*phBmp = NULL;

	BITMAPINFO bmi;

	InitBitmapInfo(&bmi, sizeof(bmi), psize->cx, psize->cy, 32);

	HDC hdcUsed = hdc ? hdc : GetDC(NULL);

	if (hdcUsed)
	{
		*phBmp = CreateDIBSection(hdcUsed, &bmi, DIB_RGB_COLORS, ppvBits, NULL, 0);
		
		if (hdc != hdcUsed)
		{
			ReleaseDC(NULL, hdcUsed);
		}
	}
	return (NULL == *phBmp) ? E_OUTOFMEMORY : S_OK;
}

static HRESULT AddBitmapToMenuItem(HMENU hmenu, int iItem, BOOL fByPosition, HBITMAP hbmp)
{
	HRESULT hr = E_FAIL;

	MENUITEMINFO mii = { sizeof(mii) };
	mii.fMask = MIIM_BITMAP;
	mii.hbmpItem = hbmp;

	if (SetMenuItemInfo(hmenu, iItem, fByPosition, &mii))
	{
		hr = S_OK;
	}
	return hr;
}

static HRESULT MyLoadIconMetric(HINSTANCE hinst, PCWSTR pszName, int lims, __out HICON *phico)
{
	HRESULT	hr		= E_FAIL;
	HMODULE hMod	= LoadLibraryW(L"Comctl32.dll");

	if (hMod != NULL)
	{
		typedef HRESULT (WINAPI *_LoadIconMetric)(HINSTANCE, PCWSTR, int, __out HICON*);

		_LoadIconMetric pLoadIconMetric = (_LoadIconMetric)GetProcAddress(hMod, "LoadIconMetric");

		if (pLoadIconMetric != NULL)
		{
			hr = pLoadIconMetric(hinst, pszName, lims, phico);
		}
	}
	return hr;
}

static void AddIconToMenu(HMENU hMenu, int nPos, HINSTANCE hinst, PCWSTR pszName)
{
	IWICImagingFactory* pFactory = NULL;	
 
	HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
 
	if (SUCCEEDED(hr))
	{
		HICON hicon = NULL;

		if (SUCCEEDED(MyLoadIconMetric(hinst, pszName, 0 /*LIM_SMALL*/, &hicon)))
		{
			IWICBitmap *pBitmap = NULL;
 
			hr = pFactory->CreateBitmapFromHICON(hicon, &pBitmap);
 
			if (SUCCEEDED(hr))
			{
				IWICFormatConverter *pConverter = NULL;
				
				hr = pFactory->CreateFormatConverter(&pConverter);

				if (SUCCEEDED(hr))
				{
					hr = pConverter->Initialize(pBitmap, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);

					UINT cx;
					UINT cy;
 
					hr = pBitmap->GetSize(&cx, &cy);
 
					if (SUCCEEDED(hr))
					{
						HBITMAP		hbmp	= NULL;
						const SIZE	sizIcon = { (int)cx, -(int)cy };
 
						BYTE* pbBuffer = NULL;
 
						hr = Create32BitHBITMAP(NULL, &sizIcon, reinterpret_cast<void **>(&pbBuffer), &hbmp);
 
						if (SUCCEEDED(hr))
						{
							const UINT cbStride = cx * sizeof(_ARGB);
							const UINT cbBuffer = cy * cbStride;
 
							hr = pBitmap->CopyPixels(NULL, cbStride, cbBuffer, pbBuffer);

							if (SUCCEEDED(hr))
							{
								AddBitmapToMenuItem(hMenu, nPos, TRUE, hbmp);
							}
							else
							{
								DeleteObject(hbmp);
							}
						}
					}
					pConverter->Release();
				}
				pBitmap->Release();
			}
		}
		pFactory->Release();
	}
}

void CMyBitmapButton::Set(int nNormalID, int PressedID, int nDisabledID)
{
	m_bmNormal	= BitmapFromResource(_Module.GetResourceInstance(), MAKEINTRESOURCE(nNormalID));
	ATLASSERT(m_bmNormal != NULL);
	m_bmDisabled= BitmapFromResource(_Module.GetResourceInstance(), MAKEINTRESOURCE(nDisabledID));
	ATLASSERT(m_bmDisabled != NULL);
	m_bmPressed	= BitmapFromResource(_Module.GetResourceInstance(), MAKEINTRESOURCE(PressedID));
	ATLASSERT(m_bmPressed != NULL);
	
	HBITMAP		hbm = NULL;
	Color		bck_col(::GetSysColor(COLOR_3DFACE));
	CImageList	il;

	il.Create(m_bmNormal->GetWidth(), m_bmNormal->GetHeight(), ILC_COLOR32|ILC_MASK, 0, 1);

	m_bmNormal->GetHBITMAP(bck_col, &hbm);
	il.Add(hbm);
	m_bmPressed->GetHBITMAP(bck_col, &hbm);
	il.Add(hbm);
	m_bmDisabled->GetHBITMAP(bck_col, &hbm);
	il.Add(hbm);

	this->SetImageList(il);
	this->SetImages(0, 1, -1, 2);
}

void CMainDlg::Elevate(bool bWithChangeApName /*= false*/, bool bWithChangeExtended /*= false*/)
{
	if (is_streaming())
	{
		if (IDNO == ::MessageBoxA(m_hWnd, "Elevation will interrupt current stream\n\nAre you sure?", strConfigName.c_str(), MB_ICONQUESTION|MB_YESNO))
			return;
	}
	USES_CONVERSION;
	char FilePath[MAX_PATH];
		
	ATLVERIFY(GetModuleFileNameA(NULL, FilePath, sizeof(FilePath)) > 0);

	CString strParameters(_T("/Elevation"));

	if (bPrivateConfig)
	{
		strParameters += _T(" /Config:");
		strParameters += A2CT(strConfigName.c_str());
	}
	if (bWithChangeApName)
		strParameters += _T(" /WithChangeApName");
	if (bWithChangeExtended)
		strParameters += _T(" /WithChangeExtended");

	if (RunAsAdmin(m_hWnd, A2T(FilePath), strParameters))
	{
		PostMessage(WM_COMMAND, IDCANCEL);
	}
}

void CMainDlg::ReadConfig()
{
	USES_CONVERSION;

	string _strApName;

	if (GetValueFromRegistry(HKEY_LOCAL_MACHINE, "ApName", _strApName))
		m_strApName = _strApName.c_str();

	string _strStartMinimized;

	if (GetValueFromRegistry(HKEY_LOCAL_MACHINE, "StartMinimized", _strStartMinimized))
		m_bStartMinimized = _strStartMinimized != "no" ? TRUE : FALSE;

	string _strAutostart;
	string _strAutostartName(strConfigName);
	
	if (bPrivateConfig)
		_strAutostartName = string("Shairport4w_") + strConfigName;

	if (GetValueFromRegistry(HKEY_LOCAL_MACHINE, _strAutostartName.c_str(), _strAutostart, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"))
		m_bAutostart = _strAutostart.empty() ? FALSE : TRUE;

	string _strTray;

	if (GetValueFromRegistry(HKEY_LOCAL_MACHINE, "Tray", _strTray))
		m_bTray = _strTray != "no" ? TRUE : FALSE;

	string _strPassword;

	if (GetValueFromRegistry(HKEY_LOCAL_MACHINE, "Password", _strPassword))
		m_strPassword = _strPassword.c_str();

	string _strSoundRedirection;

	if (GetValueFromRegistry(HKEY_LOCAL_MACHINE, "SoundRedirection", _strSoundRedirection))
		m_strSoundRedirection = _strSoundRedirection.c_str();

	string _strLogToFile;

	if (GetValueFromRegistry(HKEY_LOCAL_MACHINE, "LogToFile", _strLogToFile))
		m_bLogToFile = _strLogToFile != "no" ? TRUE : FALSE;

	string _strNoMetaInfo;

	if (GetValueFromRegistry(HKEY_LOCAL_MACHINE, "NoMetaInfo", _strNoMetaInfo))
		m_bNoMetaInfo = _strNoMetaInfo != "no" ? TRUE : FALSE;	
	
	string _strNoMediaControl;

	if (GetValueFromRegistry(HKEY_LOCAL_MACHINE, "NoMediaControl", _strNoMediaControl))
		m_bNoMediaControl = _strNoMediaControl != "no" ? TRUE : FALSE;	

	string _strSoundcardId;

	if (GetValueFromRegistry(HKEY_LOCAL_MACHINE, "SoundcardId", _strSoundcardId))
	{
		m_strSoundcardId	= _strSoundcardId;
		libao_deviceid		= m_strSoundcardId.c_str();
	}
	string _strStartFillPos;

	if (GetValueFromRegistry(HKEY_LOCAL_MACHINE, "StartFillPos", _strStartFillPos))
	{
		int n = atoi(_strStartFillPos.c_str());

		if (n > MAX_FILL)
			n = MAX_FILL;
		else if (n < MIN_FILL)
			n = MIN_FILL;

		SetStartFill(n);
	}

	string _strInfoPanel;

	if (GetValueFromRegistry(HKEY_CURRENT_USER, "InfoPanel", _strInfoPanel))
		m_bInfoPanel = _strInfoPanel != "no" ? TRUE : FALSE;

	string _strPin;

	if (GetValueFromRegistry(HKEY_CURRENT_USER, "Pin", _strPin))
		m_bPin = _strPin != "no" ? TRUE : FALSE;

	string _strTimeMode;

	if (GetValueFromRegistry(HKEY_CURRENT_USER, "TimeMode", _strTimeMode))
		 m_strTimeMode = _strTimeMode;

	string _strTrayTrackInfo;

	if (GetValueFromRegistry(HKEY_CURRENT_USER, "TrayTrackInfo", _strTrayTrackInfo))
		m_bTrayTrackInfo = _strTrayTrackInfo != "no" ? TRUE : FALSE;
}

bool CMainDlg::WriteConfig()
{
	char buf[256];

	USES_CONVERSION;

	PutValueToRegistry(HKEY_CURRENT_USER, "InfoPanel", m_bInfoPanel ? "yes" : "no");
	PutValueToRegistry(HKEY_CURRENT_USER, "Pin", m_bPin ? "yes" : "no");
	PutValueToRegistry(HKEY_CURRENT_USER, "TimeMode", m_strTimeMode.c_str());
	PutValueToRegistry(HKEY_CURRENT_USER, "TrayTrackInfo", m_bTrayTrackInfo ? "yes" : "no");

	if (!PutValueToRegistry(HKEY_LOCAL_MACHINE, "ApName", T2CA(m_strApName)))
		return false;
	else
	{
		string strHwAddr;

		EncodeBase64(hw_addr, 6, strHwAddr, false);
		ATLASSERT(!strHwAddr.empty());
		PutValueToRegistry(HKEY_LOCAL_MACHINE, "HwAddr", strHwAddr.c_str());
	}
	if (!PutValueToRegistry(HKEY_LOCAL_MACHINE, "StartMinimized", m_bStartMinimized ? "yes" : "no"))
		return false;
	if (!PutValueToRegistry(HKEY_LOCAL_MACHINE, "Tray", m_bTray ? "yes" : "no"))
		return false;
	if (!PutValueToRegistry(HKEY_LOCAL_MACHINE, "Password", T2CA(m_strPassword)))
		return false;
	if (!PutValueToRegistry(HKEY_LOCAL_MACHINE, "SoundRedirection", T2CA(m_strSoundRedirection)))
		return false;
	if (!PutValueToRegistry(HKEY_LOCAL_MACHINE, "LogToFile", m_bLogToFile ? "yes" : "no"))
		return false;
	if (!PutValueToRegistry(HKEY_LOCAL_MACHINE, "NoMetaInfo", m_bNoMetaInfo ? "yes" : "no"))
		return false;
	if (!PutValueToRegistry(HKEY_LOCAL_MACHINE, "NoMediaControl", m_bNoMediaControl ? "yes" : "no"))
		return false;
	if (!PutValueToRegistry(HKEY_LOCAL_MACHINE, "StartFillPos", itoa(GetStartFill(), buf, 10)))
		return false;
	if (!m_strSoundcardId.empty())
	{
		PutValueToRegistry(HKEY_LOCAL_MACHINE, "SoundcardId", m_strSoundcardId.c_str());
		libao_deviceid = m_strSoundcardId.c_str();
	}
	else
	{
		RemoveValueFromRegistry(HKEY_LOCAL_MACHINE, "SoundcardId");
		libao_deviceid = NULL;
	}

	char FilePath[MAX_PATH];
	ATLVERIFY(GetModuleFileNameA(NULL, FilePath, sizeof(FilePath)) > 0);

	if (m_bAutostart)
	{
		string _strAutostart = string("\"") + string(FilePath) + string("\"");

		if (bPrivateConfig)
		{
			_strAutostart += " /Config:";
			_strAutostart += strConfigName;
		}
		string _strAutostartName(strConfigName);
	
		if (bPrivateConfig)
			_strAutostartName = string("Shairport4w_") + strConfigName;
		PutValueToRegistry(HKEY_LOCAL_MACHINE, _strAutostartName.c_str(), _strAutostart.c_str(), "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
	}
	else
	{
		string _strAutostartName(strConfigName);
	
		if (bPrivateConfig)
			_strAutostartName = string("Shairport4w_") + strConfigName;
		RemoveValueFromRegistry(HKEY_LOCAL_MACHINE, _strAutostartName.c_str(), "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
	}
	return true;
}

void CMainDlg::RestartService()
{
	if (is_streaming())
	{
		::MessageBoxA(m_hWnd, "Current stream will be interrupted for reconfiguration, sorry!", strConfigName.c_str(), MB_ICONINFORMATION|MB_OK);
	}
	CWaitCursor wait;

	stop_serving();
	Sleep(1000);
	deinit_ao();
	start_serving();
}

void CMainDlg::PutMMState(typeMMState nMmState)
{
	if (_m_nMMState != nMmState)
	{
		_m_nMMState = nMmState;
	
		BOOL bDummy = true;
		OnSetMultimediaState(0, 0, 0, bDummy);
	}
}

bool CMainDlg::SendDacpCommand(const char* strCmd)
{
	ATLASSERT(strCmd != NULL);
	bool bResult = false;

	if (HasMultimediaControl())
	{
		struct addrinfo		aiHints;
		struct addrinfo*	aiList = NULL;

		memset(&aiHints, 0, sizeof(aiHints));

		aiHints.ai_family	= AF_INET;
		aiHints.ai_socktype	= SOCK_STREAM;
		aiHints.ai_protocol	= IPPROTO_TCP;

		char buf[256];

		sprintf(buf, "%lu", (ULONG)ntohs(m_nDacpPort));

		if ((getaddrinfo(m_strDacpHost.c_str(), buf, &aiHints, &aiList)) == 0) 
		{
			struct addrinfo* ai = aiList;

			while (ai != NULL)
			{
				CSocketBase client;

				if (client.Create(ai->ai_family))
				{
					Log("Trying to connect to DACP Host %s:%lu (AF:%lu) ... ", m_strDacpHost.c_str(), (ULONG)ntohs(m_nDacpPort), (ULONG)ai->ai_family);

					if (client.Connect((SOCKADDR_IN*)ai->ai_addr, ai->ai_addrlen))
					{
						Log("succeeded\n");

						CHttp request;

						request.Create();

						request.m_strMethod						= "GET";
						request.m_strUrl						= string("/ctrl-int/1/") + string(strCmd);
						request.m_mapHeader["Host"]				= m_strHostname + string(".local.");
						request.m_mapHeader["Active-Remote"]	= m_strActiveRemote.c_str();

						string strRequest = request.GetAsString(false);
						
						Log("Sending DACP request: %s\n", strCmd);

						client.Send(strRequest.c_str(), strRequest.length());
						
						if (client.WaitForIncomingData(2000))
						{
							memset(buf, 0, sizeof(buf));
							client.recv(buf, sizeof(buf)-1);

							Log("Received DACP response: %s\n", buf);

							// todo: check response for success code
							bResult = true;
						}
						else
						{
							Log("Waiting for DACP response *timedout*\n");
						}
						client.Close();

						break;
					}
					else
					{
						Log("*failed*\n");
					}
				}
				ai = ai->ai_next;
			}
			freeaddrinfo(aiList);
		}
		else
		{
			Log("Couldn't resolve IP Address of DACP Host %s:%lu\n", m_strDacpHost.c_str(), (ULONG)ntohs(m_nDacpPort));
		}
	}
	return bResult;
}

BOOL CMainDlg::PreTranslateMessage(MSG* pMsg)
{
	return CWindow::IsDialogMessage(pMsg);
}

BOOL CMainDlg::OnIdle()
{
	UIUpdateMenuBar();
	return FALSE;
}

void CMainDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
	m_bVisible = bShow ? true : false;

	if (bShow)
		OnPin(0, 0, 0);
}

void CMainDlg::OnPin(UINT, int nID, HWND)
{
	if (nID != 0)
	{
		m_bPin = !m_bPin;

		PutValueToRegistry(HKEY_CURRENT_USER, "Pin", m_bPin ? "yes" : "no");
	}
	m_ctlPushPin.SetPinned(m_bPin ? TRUE : FALSE);
	SetWindowPos(m_bPin ? HWND_TOPMOST : HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

LRESULT CMainDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	USES_CONVERSION;

	char buf[256];

	memset(buf, 0, sizeof(buf));

	if (gethostname(buf, sizeof(buf)-1) == 0)
	{
		m_strHostname = buf;
	}
	else
	{
		m_strHostname = strConfigName.c_str();
	}

	// center the dialog on the screen
	CenterWindow();

	// set icons
	HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	SetIcon(hIcon, TRUE);
	m_hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(m_hIconSmall, FALSE);

	m_hHandCursor			= LoadCursor(NULL, IDC_HAND);
	m_strTimeInfo			= STR_EMPTY_TIME_INFO;
	m_strTimeInfoCurrent	= STR_EMPTY_TIME_INFO;

	m_bmWmc				= BitmapFromResource(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDB_WMC));
	m_bmProgressShadow	= BitmapFromResource(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDB_PROGRESS_SHADOW));
	m_bmAdShadow		= BitmapFromResource(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDB_AD_SHADOW));
	m_bmArtShadow		= BitmapFromResource(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDB_ART_SHADOW));

	GetWindowRect(&m_rectOrig);

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	UIAddChildWindowContainer(m_hWnd);
	UIAddMenuBar(m_hWnd);

	if (m_bTray)
	{
		USES_CONVERSION;

		InitTray(A2CT(strConfigName.c_str()), m_hIconSmall, IDR_TRAY); 
	}
	m_ctlAutostart.Attach(GetDlgItem(IDC_AUTOSTART));
	m_ctlMinimized.Attach(GetDlgItem(IDC_START_MINIMIZED));
	m_ctlTray.Attach(GetDlgItem(IDC_TRAY));
	m_ctlUpdate.Attach(GetDlgItem(IDC_UPDATE));
	m_ctlName.Attach(GetDlgItem(IDC_APNAME));
	m_ctlPassword.Attach(GetDlgItem(IDC_PASSWORD));
	m_ctlSet.Attach(GetDlgItem(IDC_CHANGENAME));
	m_ctlPanelFrame.Attach(GetDlgItem(IDC_STATIC_PANEL));
	m_ctlControlFrame.Attach(GetDlgItem(IDC_CONTROL_PANEL));
	m_ctlArtist.Attach(GetDlgItem(IDC_STATIC_ARTIST));
	m_ctlAlbum.Attach(GetDlgItem(IDC_STATIC_ALBUM));
	m_ctlTrack.Attach(GetDlgItem(IDC_STATIC_TRACK));	
	m_ctlTimeInfo.Attach(GetDlgItem(IDC_STATIC_TIME_INFO));
	m_ctlPushPin.SubclassWindow(GetDlgItem(IDC_PUSH_PIN));
	m_ctlInfoBmp.SubclassWindow(GetDlgItem(IDC_INFO_BMP));
	m_ctlTimeBarBmp.SubclassWindow(GetDlgItem(IDC_TIME_BAR));

	CRect rect;
	
	m_ctlTimeBarBmp.GetClientRect(&rect);
	m_ctlTimeBarBmp.MapWindowPoints(m_hWnd, &rect);

	rect.left	-= 4;
	rect.top	-= 3;
	rect.right	+= 7;
	rect.bottom += 7;

	m_ctlTimeBarBmp.MoveWindow(rect, FALSE);

	m_ctlInfoBmp.SetWindowPos(NULL, 0, 0, 114, 114, SWP_NOZORDER | SWP_NOMOVE | SWP_FRAMECHANGED);

	m_ctlInfoBmp.GetClientRect(&rect);
	m_ctlInfoBmp.MapWindowPoints(m_hWnd, &rect);

	rect.left	-= 3;
	rect.top	-= 3;
	rect.right	+= 8;
	rect.bottom += 9;

	m_ctlInfoBmp.MoveWindow(rect, FALSE);

	GetDlgItem(IDC_INFOPANEL).EnableWindow(m_bNoMetaInfo ? FALSE : TRUE);

	m_ctlPlay.Set(IDB_PLAY, IDB_PLAY_PRESSED, IDB_PLAY_DISABLED);
	m_ctlPause.Set(IDB_PAUSE, IDB_PAUSE_PRESSED, IDB_PAUSE_DISABLED);
	m_ctlFFw.Set(IDB_FFW, IDB_FFW_PRESSED, IDB_FFW_DISABLED);
	m_ctlRew.Set(IDB_REW, IDB_REW_PRESSED, IDB_REW_DISABLED);
	m_ctlSkipNext.Set(IDB_SKIP_NEXT, IDB_SKIP_NEXT_PRESSED, IDB_SKIP_NEXT_DISABLED);
	m_ctlSkipPrev.Set(IDB_SKIP_PREV, IDB_SKIP_PREV_PRESSED, IDB_SKIP_PREV_DISABLED);

	DoDataExchange(FALSE);

	Button_SetElevationRequiredState(m_ctlUpdate, TRUE);
	Button_SetElevationRequiredState(m_ctlSet, TRUE);

	AddIconToMenu(GetSubMenu(GetMenu(), 1), 0, NULL, IDI_SHIELD);

	if (IsAdmin())
	{
		m_ctlUpdate.EnableWindow(FALSE);
	}
	else
	{
		m_ctlName.EnableWindow(FALSE);
		m_ctlMinimized.EnableWindow(FALSE);
		m_ctlTray.EnableWindow(FALSE);
		m_ctlAutostart.EnableWindow(FALSE);
	}
	CString strMainFrame;

	if (!strMainFrame.LoadString(IDR_MAINFRAME) || strMainFrame != CString(A2CT(strConfigName.c_str())))
		SetWindowText(A2CT(strConfigName.c_str()));

	OnClickedInfoPanel(0, 0, NULL);

	SendMessage(WM_RAOP_CTX); 
	SendMessage(WM_SET_MMSTATE); 

	ATLVERIFY(CreateToolTip(m_ctlTrack, m_ctlToolTipTrackName, IDS_TRACK_LABEL));
	ATLVERIFY(CreateToolTip(m_ctlAlbum, m_ctlToolTipAlbumName, IDS_ALBUM_LABEL));
	ATLVERIFY(CreateToolTip(m_ctlArtist, m_ctlToolTipArtistName, IDS_ARTIST_LABEL));

	UISetCheck(ID_EDIT_TRAYTRACKINFO, m_bTrayTrackInfo);

	return TRUE;
}

LRESULT CMainDlg::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	if (m_bmWmc != NULL)
	{
		delete m_bmWmc;
		m_bmWmc = NULL;
	}
	if (m_bmProgressShadow != NULL)
	{
		delete m_bmProgressShadow;
		m_bmProgressShadow = NULL;
	}
	if (m_bmAdShadow != NULL)
	{
		delete m_bmAdShadow;
		m_bmAdShadow = NULL;
	}
	if (m_bmArtShadow != NULL)
	{
		delete m_bmArtShadow;
		m_bmArtShadow = NULL;
	}
	return 0;
}

LRESULT CMainDlg::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}

LRESULT CMainDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// TODO: Add validation code 
	CloseDialog(wID);
	return 0;
}

LRESULT CMainDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CloseDialog(wID);
	return 0;
}

void CMainDlg::CloseDialog(int nVal)
{
	DestroyWindow();
	::PostQuitMessage(nVal);
}

void CMainDlg::OnClickedSetApName(UINT, int, HWND)
{
	if (!IsAdmin())
	{
		Elevate(true);
		return;
	}
	ChangeNameDlg dialog(m_strApName, m_strPassword);

	if (dialog.DoModal() == IDOK)
	{
		m_strApName		= dialog.getAirportName();
		m_strPassword	= dialog.getPassword();

		WriteConfig();
		DoDataExchange(FALSE);

		RestartService();
	}
}

void CMainDlg::OnClickedMinimizeAtStartup(UINT, int, HWND)
{
	DoDataExchange(TRUE, IDC_START_MINIMIZED);
	WriteConfig();
}

void CMainDlg::OnClickedAutostart(UINT, int, HWND)
{
	DoDataExchange(TRUE, IDC_AUTOSTART);
	WriteConfig();
}

void CMainDlg::OnClickedUpdate(UINT, int, HWND)
{
	Elevate();
}

void CMainDlg::OnSysCommand(UINT wParam, CPoint pt)
{
	SetMsgHandled(FALSE);

	if (m_bTray)
	{
		if (GET_SC_WPARAM(wParam) == SC_MINIMIZE)
		{
			ShowWindow(SW_HIDE);		
			SetMsgHandled(TRUE);

			string strDummy;
			
			if (!GetValueFromRegistry(HKEY_CURRENT_USER, "TrayAck", strDummy))
			{
				CString strAck;
				USES_CONVERSION;

				ATLVERIFY(strAck.LoadString(IDS_TRAY_ACK));
				
				SetInfoText(A2CT(strConfigName.c_str()), strAck);
				PutValueToRegistry(HKEY_CURRENT_USER, "TrayAck", "ok");
			}
		}
	}
}

void CMainDlg::OnShow(UINT, int, HWND)
{
	ShowWindow(SW_SHOWNORMAL);
	::SetForegroundWindow(m_hWnd);
}

void CMainDlg::OnClickedTrayOption(UINT, int, HWND)
{
	DoDataExchange(TRUE, IDC_TRAY);
	WriteConfig();

	if (m_bTray)
	{
		USES_CONVERSION;

		InitTray(A2CT(strConfigName.c_str()), m_hIconSmall, IDR_TRAY); 
	}
	else
		RemoveTray();
}

void CMainDlg::OnClickedExtendedOptions(UINT, int, HWND)
{
	if (!IsAdmin())
	{
		Elevate(false, true);
		return;
	}
	CExtOptsDlg dlg(m_bLogToFile, m_bNoMetaInfo, m_bNoMediaControl, m_strSoundRedirection, GetStartFill(), m_strSoundcardId.empty() ? 0 : (UINT)atoi(m_strSoundcardId.c_str()));

	if (dlg.DoModal(m_hWnd) == IDOK)
	{
		USES_CONVERSION;

		const char* strSoundRedirection = T2CA(dlg.getSoundRedirection());
		
		for (long n = 0; n < 20; ++n)
		{
			char	buf[64];
			string	str;

			sprintf_s(buf, 64, "RecentRedir%ld", n);
			
			if (!GetValueFromRegistry(HKEY_CURRENT_USER, buf, str))
			{
				PutValueToRegistry(HKEY_CURRENT_USER, buf, strSoundRedirection);
				break;
			}
			else if (strcmp(str.c_str(), strSoundRedirection) == 0)
				break;
		}
		string _strSoundcardId;

		if (dlg.getSoundcardId() > 0)
		{
			char buf[64];

			sprintf_s(buf, 64, "%ld", dlg.getSoundcardId());
			_strSoundcardId = buf;
		}
		const bool bRestartService =	(m_bLogToFile &&		!dlg.getLogToFile())
									||	(m_bNoMetaInfo			!= dlg.getNoMetaInfo())
									||	(m_bNoMediaControl		!= dlg.getNoMediaControls())
									||	(m_strSoundRedirection	!= dlg.getSoundRedirection())
									||	(m_strSoundcardId		!= _strSoundcardId)
									? true : false;

		if (m_bNoMetaInfo != dlg.getNoMetaInfo())
		{
			m_bNoMetaInfo	= dlg.getNoMetaInfo();
			m_bInfoPanel	= m_bNoMetaInfo ? FALSE : TRUE;

			DoDataExchange(FALSE);
			OnClickedInfoPanel(0, 0, NULL);

			GetDlgItem(IDC_INFOPANEL).EnableWindow(m_bInfoPanel);
		}
		m_bNoMediaControl		= dlg.getNoMediaControls();
		m_bLogToFile			= dlg.getLogToFile();
		m_bNoMetaInfo			= dlg.getNoMetaInfo();
		m_strSoundRedirection	= dlg.getSoundRedirection();
		m_strSoundcardId		= _strSoundcardId;

		SetStartFill(dlg.getPos());

		WriteConfig();

		if (bRestartService)
		{
			RestartService();
		}
	}
}

void CMainDlg::OnTimer(UINT_PTR nIDEvent)
{
	switch(nIDEvent)
	{
		case 1001:
		{
			CString strTimeInfo			= STR_EMPTY_TIME_INFO;
			CString strTimeInfoCurrent	= STR_EMPTY_TIME_INFO;

			if (m_pRaopContext != NULL)
			{
				m_pRaopContext->Lock();

				if (m_pRaopContext->m_nTimeStamp != 0 && m_pRaopContext->m_nTimeTotal != 0)
				{
					time_t nCurPosInSecs = m_pRaopContext->m_nTimeCurrentPos 
										+ (::time(NULL) - m_pRaopContext->m_nTimeStamp)
										- 2;
					if (nCurPosInSecs >= 0)
					{
						int nNewCurPos = (int) ((nCurPosInSecs * 100) / m_pRaopContext->m_nTimeTotal);

						if (nNewCurPos > 100)
							nNewCurPos = 100;
	
						if (nNewCurPos != m_nCurPos)
						{
							m_nCurPos = nNewCurPos;
							m_ctlTimeBarBmp.Invalidate();
						}

						// allow max 2 secs overtime
						if (nCurPosInSecs - m_pRaopContext->m_nTimeTotal > 2)
							nCurPosInSecs = m_pRaopContext->m_nTimeTotal+2;

						if (m_pRaopContext->m_nDurHours == 0)
						{
							long nMinCurrent = (long)(nCurPosInSecs / 60);
							long nSecCurrent = (long)(nCurPosInSecs % 60);

							if (m_strTimeMode == string("time_total"))
							{
								strTimeInfoCurrent.Format(_T("%02d:%02d")	, nMinCurrent               , nSecCurrent);
								strTimeInfo.Format		 (_T("%02d:%02d")	, m_pRaopContext->m_nDurMins, m_pRaopContext->m_nDurSecs);
							}
							else
							{
								long nRemain = m_pRaopContext->m_nTimeTotal >= nCurPosInSecs 
																? (long)(m_pRaopContext->m_nTimeTotal - nCurPosInSecs)
																: (long)(nCurPosInSecs - m_pRaopContext->m_nTimeTotal);

								long nMinRemain = nRemain / 60;
								long nSecRemain = nRemain % 60;

								strTimeInfoCurrent.Format(_T("%02d:%02d"), nMinCurrent               , nSecCurrent);
								strTimeInfo.Format		 (_T("%s%02d:%02d")
															, nCurPosInSecs > m_pRaopContext->m_nTimeTotal ? _T("+") : _T("-")																			
															, nMinRemain				, nSecRemain);
							}
						}
						else
						{
							long	nHourCurrent  = (long)(nCurPosInSecs / 3600);
							time_t _nCurPosInSecs = nCurPosInSecs - (nHourCurrent*3600);

							long nMinCurrent = (long)(_nCurPosInSecs / 60);
							long nSecCurrent = (long)(_nCurPosInSecs % 60);

							if (m_strTimeMode == string("time_total"))
							{
								strTimeInfoCurrent.Format(_T("%02d:%02d:%02d")
															, nHourCurrent               , nMinCurrent               , nSecCurrent);
								strTimeInfo.Format		 (_T("%02d:%02d:%02d")
															, m_pRaopContext->m_nDurHours, m_pRaopContext->m_nDurMins, m_pRaopContext->m_nDurSecs);
							}
							else
							{
								long nRemain = m_pRaopContext->m_nTimeTotal >= nCurPosInSecs
																? (long)(m_pRaopContext->m_nTimeTotal - nCurPosInSecs)
																: (long)(nCurPosInSecs - m_pRaopContext->m_nTimeTotal);

								long nHourRemain = nRemain / 3600;
								nRemain -= (nHourRemain*3600);

								long nMinRemain = nRemain / 60;
								long nSecRemain = nRemain % 60;

								strTimeInfoCurrent.Format(_T("%02d:%02d:%02d")	
															, nHourCurrent               , nMinCurrent               , nSecCurrent);
								strTimeInfo.Format		 (_T("%s%02d:%02d:%02d")
															, nCurPosInSecs > m_pRaopContext->m_nTimeTotal ? _T("+") : _T("-")
															, nHourRemain				 , nMinRemain				 , nSecRemain);
							}
						}
					}
				}
				m_pRaopContext->Unlock();
			}
			if (m_strTimeInfo != strTimeInfo)
			{
				m_strTimeInfo = strTimeInfo;
				DoDataExchange(FALSE, IDC_STATIC_TIME_INFO);
			}
			if (m_strTimeInfoCurrent != strTimeInfoCurrent)
			{
				m_strTimeInfoCurrent = strTimeInfoCurrent;
				DoDataExchange(FALSE, IDC_STATIC_TIME_INFO_CURRENT);

				if (m_strTimeInfoCurrent == STR_EMPTY_TIME_INFO)
				{
					if (0 != m_nCurPos)
					{
						m_nCurPos = 0;
						m_ctlTimeBarBmp.Invalidate();
					}
				}
			}
		}
		break;

		default:
		{
			KillTimer(nIDEvent);
			ATLASSERT(FALSE);
		}
		break;
	}
}

void  CMainDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	CWindow wnd = ::RealChildWindowFromPoint(m_hWnd, point);

	if (wnd.m_hWnd == m_ctlTimeInfo.m_hWnd)
	{
		if (m_pRaopContext != NULL && m_strTimeInfo != STR_EMPTY_TIME_INFO)
		{
			if (m_strTimeMode == string("time_total"))
				m_strTimeMode = string("time_remaining");
			else
				m_strTimeMode = string("time_total");

			OnTimer(1001);
			PutValueToRegistry(HKEY_CURRENT_USER, "TimeMode", m_strTimeMode.c_str());
		}
	}
	else if (wnd.m_hWnd == m_ctlInfoBmp.m_hWnd)
	{
		if (m_bAd)
		{
			CWaitCursor wait;

			ShellExecuteW(m_hWnd, L"open", L"http://www.lightweightdream.com/shairport4w", NULL, NULL, SW_SHOWNORMAL);
		}
	}
	SetMsgHandled(FALSE);
}

LRESULT CMainDlg::Pause(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// omit pause in case we're receiving this in order to a "FLUSH" while skipping
	if (wParam == 0 || (m_nMMState != skip_next && m_nMMState != skip_prev))
	{
		if (m_nMMState != pause)
		{
			KillTimer(1001);
			m_nMMState = pause;
		}
	}
	return 0l;
}

LRESULT CMainDlg::Resume(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// omit play in case we're receiving this in order to a "SET_PARAMETER/progress" while ffw or rew
	if (wParam == 0 || (m_nMMState != ffw && m_nMMState != rew))
	{
		if (m_nMMState != play)
		{
			m_nMMState = play;
			SetTimer(1001, 1000);
		}
	}
	return 0l;
}

LRESULT CMainDlg::OnRaopContext(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	BOOL bDummy = TRUE;

	m_pRaopContext = (CRaopContext*) lParam;

	// prepare info bmp
	m_ctlInfoBmp.Invalidate();

	// prepare time info
	m_strTimeInfo			= STR_EMPTY_TIME_INFO;
	m_strTimeInfoCurrent	= STR_EMPTY_TIME_INFO;
	m_nCurPos				= 0;

	m_ctlTimeBarBmp.Invalidate();
	DoDataExchange(FALSE, IDC_STATIC_TIME_INFO);
	DoDataExchange(FALSE, IDC_STATIC_TIME_INFO_CURRENT);

	// prepare song info
	OnSongInfo(0, 0, 0, bDummy);

	if (m_pRaopContext != NULL)
	{
		Resume();
	}
	else
	{
		Pause();
		PostMessage(WM_SET_MMSTATE);
	}
	return 0l;
}

LRESULT CMainDlg::OnDACPRegistered(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	ATLASSERT(lParam != NULL);

	CRegisterServiceEvent* pEvent = (CRegisterServiceEvent*) lParam;

	Log("Service %sregistered: %s, %s\n", pEvent->m_bRegister ? "" : "de", pEvent->m_strService.c_str(), pEvent->m_strRegType.c_str());

	if (m_strDACP_ID == pEvent->m_strService)
	{
		if (pEvent->m_bRegister)
		{
			pEvent->Resolve(this);
		}
		else
		{
			m_strDacpHost.clear();
			m_nDacpPort = 0;
				
			PostMessage(WM_SET_MMSTATE);
		}
	}
	else
	{
		Log("Service %s, %s *ignored*\n", pEvent->m_strService.c_str(), pEvent->m_strRegType.c_str());
	}
	if (pEvent != NULL)
		delete pEvent;

	return 0l;
}

LRESULT CMainDlg::OnSongInfo(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	USES_CONVERSION;

	WTL::CString	str_daap_songalbum;
	WTL::CString	str_daap_songartist;	
	WTL::CString	str_daap_trackname;

	if (m_pRaopContext != NULL)
	{
		m_pRaopContext->Lock();

		str_daap_songalbum		= m_pRaopContext->m_str_daap_songalbum;
		str_daap_songartist		= m_pRaopContext->m_str_daap_songartist;
		str_daap_trackname		= m_pRaopContext->m_str_daap_trackname;
		
		m_pRaopContext->Unlock();
	}
	m_ctlArtist.SetWindowText(str_daap_songartist);
	m_ctlAlbum.SetWindowText(str_daap_songalbum);
	m_ctlTrack.SetWindowText(str_daap_trackname);

	m_ctlArtist.EnableWindow(!str_daap_songartist.IsEmpty());
	m_ctlAlbum.EnableWindow(!str_daap_songalbum.IsEmpty());
	m_ctlTrack.EnableWindow(!str_daap_trackname.IsEmpty());

	if (	m_bTray && m_bTrayTrackInfo
		&&	(	!str_daap_songartist.IsEmpty()
			 || !str_daap_songalbum.IsEmpty()
			 || !str_daap_trackname.IsEmpty()
			 )
		)
	{
		if (!m_bVisible)
		{
			WTL::CString	strFmtNowPlaying;
			WTL::CString	strTrackLabel;
			WTL::CString	strAlbumLabel;
			WTL::CString	strArtistLabel;
			WTL::CString	strNowPlayingLabel;

			WTL::CString	strSep(_T(": \t"));
			WTL::CString	strNewLine(_T("\n"));

			ATLVERIFY(strTrackLabel.LoadString(IDS_TRACK_LABEL));
			ATLVERIFY(strAlbumLabel.LoadString(IDS_ALBUM_LABEL));
			ATLVERIFY(strArtistLabel.LoadString(IDS_ARTIST_LABEL));
			ATLVERIFY(strNowPlayingLabel.LoadString(IDS_NOW_PLAYING));

			if (!str_daap_songartist.IsEmpty())
			{
				if (!strFmtNowPlaying.IsEmpty())
					strFmtNowPlaying += strNewLine;

				strFmtNowPlaying += (strArtistLabel + strSep + str_daap_songartist);
			}
			if (!str_daap_trackname.IsEmpty())
			{
				if (!strFmtNowPlaying.IsEmpty())
					strFmtNowPlaying += strNewLine;

				strFmtNowPlaying += (strTrackLabel + strSep + str_daap_trackname);
			}
			if (!str_daap_songalbum.IsEmpty())
			{
				if (!strFmtNowPlaying.IsEmpty())
					strFmtNowPlaying += strNewLine;

				strFmtNowPlaying += (strAlbumLabel + strSep + str_daap_songalbum);
			}
			SetInfoText(strNowPlayingLabel, strFmtNowPlaying);
		}
	}
	return 0l;
}

void CMainDlg::OnInfoBmpPaint(HDC wParam)
{
	CRect rect;

	if (m_ctlInfoBmp.GetUpdateRect(&rect))
	{
		CPaintDC dcPaint(m_ctlInfoBmp.m_hWnd);

		if (!dcPaint.IsNull())
		{
			CBrush br;

			br.CreateSolidBrush(::GetSysColor(COLOR_3DFACE));

			dcPaint.FillRect(&rect, br);

			Graphics dcGraphics(dcPaint);

			CRect rectClient;

			m_ctlInfoBmp.GetClientRect(&rectClient);

			rect = rectClient;

			rect.left	+= 3;
			rect.top	+= 3;
			rect.right	-= 8;
			rect.bottom -= 9;

			Rect rClient(0, 0, rectClient.Width(), rectClient.Height());
			Rect r(rect.left, rect.top, rect.Width(), rect.Height());

			bool bDrawn = false;

			if (m_pRaopContext != NULL)
			{
				m_pRaopContext->Lock();

				if (m_pRaopContext->m_bmInfo != NULL)
				{
					dcGraphics.DrawImage(m_bmArtShadow, rClient);
					bDrawn = dcGraphics.DrawImage(m_pRaopContext->m_bmInfo, r) == Ok ? true : false;
				}
				m_pRaopContext->Unlock();
			}
			if (!bDrawn)
			{
				m_bAd = true;

				CBrush			brShadow;
				brShadow.CreateSolidBrush(::GetSysColor(COLOR_BTNSHADOW));

				CBrushHandle oldBrush = dcPaint.SelectBrush(brShadow);
				
				dcPaint.RoundRect(rect, CPoint(16, 16));
				dcPaint.SelectBrush(oldBrush);

				dcGraphics.DrawImage(m_bmAdShadow, rClient);

				r.Inflate(-6, -6);
				dcGraphics.DrawImage(m_bmWmc, r);
			}
			else
				m_bAd = false;
		}
	}
}

void CMainDlg::OnTimeBarBmpPaint(HDC wParam)
{
	CRect rect;

	if (m_ctlTimeBarBmp.GetUpdateRect(&rect))
	{
		CPaintDC dcPaint(m_ctlTimeBarBmp.m_hWnd);

		if (!dcPaint.IsNull())
		{
			Graphics		dcGraphics(dcPaint);
			CBrush			brFace;
			CBrush			brShadow;
			CBrush			brBar;
			CBrushHandle	oldBrush;
			CPoint			rnd(6, 6);
			CRect			rectClient;

			m_ctlTimeBarBmp.GetClientRect(&rectClient);

			Rect rClient(0, 0, rectClient.Width(), rectClient.Height());

			rect		= rectClient;
			rect.left	+= 4;
			rect.top	+= 3;
			rect.right	-= 7;
			rect.bottom -= 7;

			brFace.CreateSolidBrush(::GetSysColor(COLOR_3DFACE));
			brShadow.CreateSolidBrush(::GetSysColor(COLOR_BTNSHADOW));
			brBar.CreateSolidBrush(::GetActiveWindow() == m_hWnd ? ::GetSysColor(COLOR_GRADIENTACTIVECAPTION) : ::GetSysColor(COLOR_GRADIENTINACTIVECAPTION));
			
			CRgn rgn;

			rgn.CreateRoundRectRgn(rect.left, rect.top, rect.right+1, rect.bottom+1, rnd.x, rnd.y);

			dcPaint.SelectClipRgn(rgn, RGN_DIFF);
			dcPaint.FillRect(&rectClient, brFace);
			
			dcGraphics.DrawImage(m_bmProgressShadow, rClient);

			dcPaint.SelectClipRgn(rgn);

			CRect rectLeft(rect.TopLeft(), CSize((rect.Width() * m_nCurPos) / 100, rect.Height()));

			if (rectLeft.Width() > 2)
			{
				CRgn rgn1;

				rgn1.CreateRoundRectRgn(rectLeft.left, rectLeft.top, rectLeft.right+1, rectLeft.bottom+1, rnd.x, rnd.y);

				dcPaint.SelectClipRgn(rgn1, RGN_DIFF);

				oldBrush = dcPaint.SelectBrush(brShadow);
				dcPaint.RoundRect(rect, rnd);

				dcPaint.SelectClipRgn(rgn1);
				
				dcPaint.SelectBrush(brBar);
				dcPaint.RoundRect(rectLeft, rnd);
				
				dcPaint.SelectBrush(oldBrush);
			}
			else
			{
				oldBrush = dcPaint.SelectBrush(brShadow);
				dcPaint.RoundRect(rect, rnd);
				dcPaint.SelectBrush(oldBrush);
			}
		}
	}
}

void CMainDlg::OnActivate(UINT nState, BOOL bMinimized, CWindow wndOther)
{
	m_ctlTimeBarBmp.Invalidate();
	SetMsgHandled(FALSE);
}

BOOL CMainDlg::OnAppCommand(CWindow wndFocus, short cmd, WORD uDevice, int dwKeys)
{
	if (HasMultimediaControl())
	{
		switch(cmd)
		{
			case APPCOMMAND_MEDIA_NEXTTRACK:
			{
				if (m_ctlSkipNext.IsWindowEnabled())
				{
					OnSkipNext(0, 0, NULL);
					return TRUE;
				}
			}
			break;

			case APPCOMMAND_MEDIA_STOP:
			case APPCOMMAND_MEDIA_PAUSE:
			{
				if (m_ctlPause.IsWindowEnabled())
				{
					OnPause(0, 0, NULL);
					return TRUE;
				}
			}
			break;

			case APPCOMMAND_MEDIA_FAST_FORWARD:
			{
				if (m_ctlFFw.IsWindowEnabled())
				{
					OnForward(0, 0, NULL);
					return TRUE;
				}
			}
			break;

			case APPCOMMAND_MEDIA_PLAY:
			{
				if (m_ctlPlay.IsWindowEnabled())
				{
					OnPlay(0, 0, NULL);
					return TRUE;
				}
			}
			break;

			case APPCOMMAND_MEDIA_PLAY_PAUSE:
			{
				if (m_ctlPlay.IsWindowEnabled())
				{
					OnPlay(0, 0, NULL);
					return TRUE;
				}
				else if (m_ctlPause.IsWindowEnabled())
				{
					OnPause(0, 0, NULL);
					return TRUE;
				}
			}
			break;

			case APPCOMMAND_MEDIA_PREVIOUSTRACK:
			{
				if (m_ctlSkipPrev.IsWindowEnabled())
				{
					OnSkipPrev(0, 0, NULL);
					return TRUE;
				}
			}
			break;

			case APPCOMMAND_MEDIA_REWIND:
			{
				if (m_ctlRew.IsWindowEnabled())
				{
					OnRewind(0, 0, NULL);
					return TRUE;
				}
			}
			break;
		}
	}
	SetMsgHandled(FALSE);
	return FALSE;
}


LRESULT CMainDlg::OnPowerBroadcast(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetMsgHandled(FALSE);

	Log("OnPowerBroadcast: %d\n", wParam);

	// Prevent suspend or standby is we are streaming
	// Note that you only get to this message when system is timing out.  You
	// will not get it when a user forces a standby or suspend
	if ((wParam == PBT_APMQUERYSUSPEND || wParam == PBT_APMQUERYSTANDBY) &&
		is_streaming())
	{
		SetMsgHandled(TRUE);
		Log("OnPowerBroadcast: System wants to suspend and since we are streaming we deny the request\n");
		return BROADCAST_QUERY_DENY;
	}

	// If we did not get the query to know if we deny the standby/suspend
	// we have to close all clients because we will be suspended anyways
	// and all our connections will be closed by the OS.
	if ((wParam == PBT_APMSUSPEND || wParam == PBT_APMSTANDBY) &&
		is_streaming())
	{
		SetMsgHandled(TRUE);

		Log("OnPowerBroadcast: System is going to suspend and since we are streaming we will close the connections\n");

		stop_serving();
		Sleep(1000);
		deinit_ao();
	}

	if (wParam == PBT_APMRESUMESUSPEND ||
		wParam == PBT_APMRESUMESTANDBY)
	{
		SetMsgHandled(TRUE);
		RestartService();
	}
	return 0;
}

BOOL CMainDlg::OnSetCursor(CWindow wnd, UINT nHitTest, UINT message)
{
	if (m_bAd)
	{
		CPoint pt;

		if (GetCursorPos(&pt))
		{
			ScreenToClient(&pt);

			CWindow wnd = ::RealChildWindowFromPoint(m_hWnd, pt);

			if (wnd.m_hWnd == m_ctlInfoBmp.m_hWnd)
			{
				SetCursor(m_hHandCursor);
				return TRUE;
			}
		}
	}
	SetMsgHandled(FALSE);
	return FALSE;
}

LRESULT CMainDlg::OnInfoBitmap(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_ctlInfoBmp.Invalidate();
	return 0l;
}

void CMainDlg::OnClickedInfoPanel(UINT, int, HWND)
{
	const int nOffsetValue = 134;

	DoDataExchange(TRUE, IDC_INFOPANEL);

	PutValueToRegistry(HKEY_CURRENT_USER, "InfoPanel", m_bInfoPanel ? "yes" : "no");

	CRect rect;
	GetWindowRect(&rect);

	if ((!m_bNoMetaInfo && m_bInfoPanel) && rect.Height() < 300)
	{
		SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height()+nOffsetValue, SWP_NOZORDER | SWP_NOMOVE | SWP_FRAMECHANGED);

		m_ctlPanelFrame.GetWindowRect(&rect);
		m_ctlPanelFrame.SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height()+nOffsetValue, SWP_NOZORDER | SWP_NOMOVE | SWP_FRAMECHANGED);

		m_ctlControlFrame.GetClientRect(&rect);
		m_ctlControlFrame.MapWindowPoints(m_hWnd, &rect);
		m_ctlControlFrame.SetWindowPos(NULL, rect.left, rect.top+nOffsetValue, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_FRAMECHANGED);

		REPLACE_CONTROL(m_ctlPlay,		nOffsetValue)
		REPLACE_CONTROL(m_ctlPause,		nOffsetValue)
		REPLACE_CONTROL(m_ctlFFw,		nOffsetValue)
		REPLACE_CONTROL(m_ctlRew,		nOffsetValue)
		REPLACE_CONTROL(m_ctlSkipNext,	nOffsetValue)
		REPLACE_CONTROL(m_ctlSkipPrev,	nOffsetValue)

		m_ctlInfoBmp.ShowWindow(SW_NORMAL);
		m_ctlArtist.ShowWindow(SW_NORMAL);
		m_ctlAlbum.ShowWindow(SW_NORMAL);
		m_ctlTrack.ShowWindow(SW_NORMAL);

		m_ctlInfoBmp.Invalidate();
	}
	else if ((m_bNoMetaInfo || !m_bInfoPanel) && rect.Height() > 280)
	{
		SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height()-nOffsetValue, SWP_NOZORDER | SWP_NOMOVE | SWP_FRAMECHANGED);

		m_ctlPanelFrame.GetWindowRect(&rect);
		m_ctlPanelFrame.SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height()-nOffsetValue, SWP_NOZORDER | SWP_NOMOVE | SWP_FRAMECHANGED);

		m_ctlControlFrame.GetClientRect(&rect);
		m_ctlControlFrame.MapWindowPoints(m_hWnd, &rect);
		m_ctlControlFrame.SetWindowPos(NULL, rect.left, rect.top-nOffsetValue, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_FRAMECHANGED);

		REPLACE_CONTROL(m_ctlPlay,		-nOffsetValue)
		REPLACE_CONTROL(m_ctlPause,		-nOffsetValue)
		REPLACE_CONTROL(m_ctlFFw,		-nOffsetValue)
		REPLACE_CONTROL(m_ctlRew,		-nOffsetValue)
		REPLACE_CONTROL(m_ctlSkipNext,	-nOffsetValue)
		REPLACE_CONTROL(m_ctlSkipPrev,	-nOffsetValue)

		m_ctlInfoBmp.ShowWindow(SW_HIDE);
		m_ctlArtist.ShowWindow(SW_HIDE);
		m_ctlAlbum.ShowWindow(SW_HIDE);
		m_ctlTrack.ShowWindow(SW_HIDE);
	}
}

void CMainDlg::OnServiceResolved(const char* strHost, USHORT nPort)
{
	m_strDacpHost	= strHost;
	m_nDacpPort		= nPort;

	Log("OnServiceResolved: %s : %lu\n", strHost, (ULONG)ntohs(nPort));

	PostMessage(WM_SET_MMSTATE);
}

LRESULT CMainDlg::OnSetMultimediaState(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (m_ctlPlay.IsWindow())
	{
		if (!HasMultimediaControl())
		{
			m_ctlPlay.EnableWindow(false);
			m_ctlPause.EnableWindow(false);
			m_ctlFFw.EnableWindow(false);
			m_ctlRew.EnableWindow(false);
			m_ctlSkipNext.EnableWindow(false);
			m_ctlSkipPrev.EnableWindow(false);
		}
		else
		{
			switch(_m_nMMState)
			{
				case pause:
				{
					m_ctlPlay.EnableWindow(true);
					m_ctlPause.EnableWindow(false);
					m_ctlFFw.EnableWindow(false);
					m_ctlRew.EnableWindow(false);
					m_ctlSkipNext.EnableWindow(true);
					m_ctlSkipPrev.EnableWindow(true);
				}
				break;

				case play:
				{
					m_ctlPlay.EnableWindow(false);
					m_ctlPause.EnableWindow(true);
					m_ctlFFw.EnableWindow(true);
					m_ctlRew.EnableWindow(true);
					m_ctlSkipNext.EnableWindow(true);
					m_ctlSkipPrev.EnableWindow(true);
				}
				break;

				case ffw:
				{
					m_ctlPlay.EnableWindow(true);
					m_ctlPause.EnableWindow(true);
					m_ctlFFw.EnableWindow(false);
					m_ctlRew.EnableWindow(false);
					m_ctlSkipNext.EnableWindow(true);
					m_ctlSkipPrev.EnableWindow(true);
				}
				break;

				case rew:
				{
					m_ctlPlay.EnableWindow(true);
					m_ctlPause.EnableWindow(true);
					m_ctlFFw.EnableWindow(false);
					m_ctlRew.EnableWindow(false);
					m_ctlSkipNext.EnableWindow(true);
					m_ctlSkipPrev.EnableWindow(true);
				}
				break;
			}
		}
	}
	return 0l;
}

void CMainDlg::OnPlay(UINT, int nID, HWND)
{
	switch(m_nMMState)
	{
		case play:
		break;

		case pause:
		{
			if (SendDacpCommand("play"))
				Resume();
		}
		break;

		case ffw:
		case rew:
		{
			if (SendDacpCommand("playresume"))
				Resume();
		}
		break;
	}
}

void CMainDlg::OnSkipPrev(UINT, int nID, HWND)
{
	switch(m_nMMState)
	{
		case play:
		{
			if (SendDacpCommand("previtem"))
			{
				if (m_pRaopContext != NULL && m_pRaopContext->m_nTimeTotal > 0)
				{
					m_nMMState = skip_prev;
				}
			}
		}
		break;

		case pause:
		{
			SendDacpCommand("previtem");
		}
		break;

		case ffw:
		case rew:
		{
			if (SendDacpCommand("playresume"))
				Resume();
			if (SendDacpCommand("previtem"))
				m_nMMState = skip_prev;
		}
		break;
	}
}

void CMainDlg::OnRewind(UINT, int nID, HWND)
{
	switch(m_nMMState)
	{
		case play:
		{
			if (SendDacpCommand("beginrew"))
			{
				if (m_pRaopContext != NULL && m_pRaopContext->m_nTimeTotal > 0)
				{
					m_nMMState = rew;
				}
			}
		}
		break;

		case pause:
		{
		}
		break;

		case ffw:
		case rew:
		{
			if (SendDacpCommand("playresume"))
				Resume();
		}
		break;
	}
}

void CMainDlg::OnPause(UINT, int nID, HWND)
{
	switch(m_nMMState)
	{
		case play:
		{
			if (SendDacpCommand("pause"))
				Pause();
		}
		break;

		case pause:
		{
		}
		break;

		case ffw:
		case rew:
		{
			if (SendDacpCommand("playresume"))
				Resume();

			if (SendDacpCommand("pause"))
				Pause();
		}
		break;
	}
}

void CMainDlg::OnForward(UINT, int nID, HWND)
{
	switch(m_nMMState)
	{
		case play:
		{
			if (SendDacpCommand("beginff"))
			{
				if (m_pRaopContext != NULL && m_pRaopContext->m_nTimeTotal > 0)
					m_nMMState = ffw;
			}
		}
		break;

		case pause:
		{
		}
		break;

		case ffw:
		case rew:
		{
			if (SendDacpCommand("playresume"))
				Resume();
		}
		break;
	}
}

void CMainDlg::OnSkipNext(UINT, int nID, HWND)
{
	switch(m_nMMState)
	{
		case play:
		{
			if (SendDacpCommand("nextitem"))
			{
				if (m_pRaopContext != NULL && m_pRaopContext->m_nTimeTotal > 0)
					m_nMMState = skip_next;
			}
		}
		break;

		case pause:
		{
			SendDacpCommand("nextitem");
		}
		break;

		case ffw:
		case rew:
		{
			if (SendDacpCommand("playresume"))
				Resume();
			if (SendDacpCommand("nextitem"))
				m_nMMState = skip_next;
		}
		break;
	}
}

void CMainDlg::OnEditTrayTrackInfo(UINT, int, HWND)
{
	m_bTrayTrackInfo = !m_bTrayTrackInfo;
	UISetCheck(ID_EDIT_TRAYTRACKINFO, m_bTrayTrackInfo);

	PutValueToRegistry(HKEY_CURRENT_USER, "TrayTrackInfo", m_bTrayTrackInfo ? "yes" : "no");
}
