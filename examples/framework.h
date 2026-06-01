// framework.h : precompiled-header include file for the WindowsProject sample.
//
// Spiritual successor to the Visual Studio "Windows Desktop Application" template's framework.h,
// but CRT-free: this app links /NODEFAULTLIB and starts through Win32X's _tWinMainEx, so the CRT
// headers the stock template pulls in (<stdlib.h>, <malloc.h>, <memory.h>, <tchar.h>) are gone.
// Strings use TEXT() from <windows.h>.
#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <windowsx.h>                   // message crackers (HANDLE_MSG)

#include "Win32X/win32x.h"              // the library this sample exercises
