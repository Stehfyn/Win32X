// framework.h : precompiled-header include file for the WindowsProject sample.
//
// Spiritual successor to the Visual Studio "Windows Desktop Application" template's framework.h,
// but CRT-free: this app links /NODEFAULTLIB and starts through Win32X's _tWinMainEx, so the CRT
// headers the stock template pulls in (<stdlib.h>, <malloc.h>, <memory.h>, <tchar.h>) are gone.
// Strings use TEXT() from <windows.h>.
#pragma once

#include "targetver.h"

// CRT-free environment: this app links /NODEFAULTLIB. Disable the codegen that would emit calls into
// an absent C runtime -- runtime checks (/RTC under Debug), stack probing, strict GS -- for this
// translation unit and the inline helpers pulled in from the headers below.
#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <windowsx.h>                   // message crackers (HANDLE_MSG)

#include "Win32X/win32x.h"              // the library this sample exercises
#include "Win32X/uxthemex.h"            // dark-mode / theming helpers (pulls in windowsx2.h crackers)
