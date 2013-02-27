/*
 *
 *  ExtOptsDlg.cpp 
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

#include "StdAfx.h"
#include "ExtOptsDlg.h"
#include "Mmdeviceapi.h"
#include "FunctionDiscoveryKeys_devpkey.h"

void GetFullDeviceName(wstring& strDevname)
{
	if (strDevname.empty())
		return;

	size_t nLength = strDevname.length();

	wstring strBestMatch;

	IMMDeviceEnumerator* pEnumerator = NULL;

	if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,__uuidof(IMMDeviceEnumerator), (void**)&pEnumerator)))
	{
		IMMDeviceCollection* pCollection = NULL;

		if (SUCCEEDED(pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE | DEVICE_STATE_UNPLUGGED, &pCollection)))
		{
			UINT count = 0;
			
			pCollection->GetCount(&count);

			for (UINT i = 0; i < count; ++i)
			{
				IMMDevice* pEndPoint = NULL;
				
				if (SUCCEEDED(pCollection->Item(i, &pEndPoint)))
				{
					IPropertyStore* pProps = NULL;
					
					if (SUCCEEDED(pEndPoint->OpenPropertyStore(STGM_READ, &pProps)))
					{
						PROPVARIANT varName;
						PropVariantInit(&varName);
						
						if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName)))
						{
							if (nLength <= wcslen(varName.pwszVal))
							{
								if (wcsnicmp(varName.pwszVal, strDevname.c_str(), nLength) == 0)
								{
									if (wcslen(varName.pwszVal) > strBestMatch.length())
										strBestMatch = varName.pwszVal;
								}
							}
							PropVariantClear(&varName);
						}
						pProps->Release();
					}
					pEndPoint->Release();
				}
			}
			pCollection->Release();
		}
		pEnumerator->Release();
	}
	if (!strBestMatch.empty())
		strDevname = strBestMatch;
}

CExtOptsDlg::CExtOptsDlg(BOOL bLogToFile, BOOL bNoMetaInfo, BOOL bNoMediaControls, CString strSoundRedirection, int nPos, int nSoundcardId /*= 0*/) :
	m_bLogToFile(bLogToFile), m_bNoMetaInfo(bNoMetaInfo), m_bNoMediaControls(bNoMediaControls), m_nPos(nPos), m_strSoundRedirection(strSoundRedirection), m_nSoundcardId(nSoundcardId) 
{
}

CExtOptsDlg::~CExtOptsDlg(void)
{
}


LRESULT CExtOptsDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CenterWindow();

	// set icons
	HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	SetIcon(hIcon, TRUE);
	m_hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(m_hIconSmall, FALSE);

	DoDataExchange(FALSE);

	m_ctlSoundRedirection.Attach(GetDlgItem(IDC_REDIRECTION));

	m_ctlSoundRedirection.AddString(_T("no redirection"));
	m_ctlSoundRedirection.AddString(_T("Sp4w_out.au"));
	m_ctlSoundRedirection.AddString(_T("lame.exe -b 192 -h -r - Sp4w_out.mp3"));

	if (m_ctlSoundRedirection.FindStringExact(0, m_strSoundRedirection) == CB_ERR)
		m_ctlSoundRedirection.AddString(m_strSoundRedirection);

	UINT n = 0;

	while(true)
	{
		char	buf[64];
		string	str;

		sprintf_s(buf, 64, "RecentRedir%ld", n++);
			
		if (GetValueFromRegistry(HKEY_CURRENT_USER, buf, str))
		{
			USES_CONVERSION;

			LPCTSTR _str = A2CT(str.c_str());

			if (m_ctlSoundRedirection.FindStringExact(0, _str) == CB_ERR)
				m_ctlSoundRedirection.AddString(_str);
		}
		else
			break;
	}
	m_ctlSoundcard.Attach(GetDlgItem(IDC_SOUNDCARD));

	m_ctlSoundcard.AddString(_T("default"));

	WAVEOUTCAPSW caps;

	n = waveOutGetNumDevs();

	for (UINT i = 0; i < n; ++i)
	{
		USES_CONVERSION;

		if (waveOutGetDevCapsW(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
			break;
		
		wstring strDevname(caps.szPname);
		GetFullDeviceName(strDevname);

		m_ctlSoundcard.AddString(W2CT(strDevname.c_str()));
	}
	m_ctlSoundcard.SetCurSel(m_nSoundcardId);

	m_ctlBuffering.Attach(GetDlgItem(IDC_BUFFERING));
	m_ctlBufferingLabel.Attach(GetDlgItem(IDC_STATIC_BUFFERING));

	m_ctlBuffering.SetRange(MIN_FILL, MAX_FILL, TRUE);
	m_ctlBuffering.SetPos(m_nPos);

	OnHScroll(0, 0, m_ctlBuffering.m_hWnd);

	return TRUE;
}

CString CExtOptsDlg::getSoundRedirection()
{
	return m_strSoundRedirection;
}

BOOL CExtOptsDlg::getLogToFile()
{
	return m_bLogToFile;
}

BOOL CExtOptsDlg::getNoMetaInfo()
{
	return m_bNoMetaInfo;
}

BOOL CExtOptsDlg::getNoMediaControls()
{
	return m_bNoMediaControls;
}

int CExtOptsDlg::getPos()
{
	return m_nPos;
}

LRESULT CExtOptsDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	m_nSoundcardId = m_ctlSoundcard.GetCurSel();

	if (m_nSoundcardId == CB_ERR)
		m_nSoundcardId = 0;

	m_nPos = m_ctlBuffering.GetPos();

	DoDataExchange(TRUE);
	EndDialog(IDOK);
	return 0;
}

LRESULT CExtOptsDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(IDCANCEL);
	return 0;
}

void CExtOptsDlg::OnHScroll(int, short, HWND)
{
	WCHAR buf[256];

	m_nPos = m_ctlBuffering.GetPos();

	swprintf_s(buf, 256, L"Buffering (%ld Frames)", m_nPos);
	m_ctlBufferingLabel.SetWindowTextW(buf);
}
