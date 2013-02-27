/*
 *
 *  stdafx.h
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

// Change these values to use different versions
#define WINVER			0x0500
#define _WIN32_WINNT	0x0501
#define _WIN32_IE		0x0501
#define _RICHEDIT_VER	0x0200

#pragma warning(disable:4786)
#pragma warning(disable:4996)

#include <atlbase.h>
#include <atlapp.h>

extern CAppModule _Module;

#include <atlwin.h>

#include <atlframe.h>
#include <atlctrls.h>
#include <atlmisc.h>
#include <atlCtrlx.h>
#include <atldlgs.h>
#include <atlddx.h>
#include <atlcrack.h>
#include <atlgdi.h>

#include <atlrx.h>
#include <atlsync.h>

#include <ws2tcpip.h>


#include <list>
#include <queue>
#include <deque>
#include <vector>
#include <map>
#include <string>
#include <algorithm>

using namespace std;

#include <GdiPlus.h>

using namespace Gdiplus;

#include "Networking.h"

#include "openssl/rsa.h"
#include "openssl/buffer.h"
#include "openssl/bio.h"
#include "openssl/evp.h"
#include <openssl/aes.h>
#include <openssl/md5.h>

#include <Wincodec.h>

#include "myMutex.h"

#include "utils.h"
#include "http_parser.h"
#include "RaopContext.h"


#if defined _M_IX86
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
