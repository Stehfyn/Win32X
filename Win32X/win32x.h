/*
 * win32x.h -- Win32X umbrella header.
 *
 * The single public entry point to the Win32X static library. A client includes this one header to
 * pull in the whole extension surface (the SDK "X" headers plus the WinBaseX startup/launch API);
 * it must not include the individual headers directly. Each constituent header carries its own
 * include guard and pulls <windows.h> itself, so the include order here is not significant --
 * windefx.h (the primitive macros) is listed first only for readability.
 */
#ifndef WIN32X_H
#define WIN32X_H

#include <windows.h>

#include "windefx.h"          /* primitive ABS/BOUND/flag/RECT macros */
#include "WinBaseX.h"         /* startup shim + exefile DelegateExecute launch broker */
#include "WinUserX.h"         /* user32 extensions (ShowWindowEx, DPI helpers, ...) */
#include "ShellScalingApiX.h" /* per-monitor DPI / scaling thunks */
#include "versionhelpersx.h"  /* CRT-free version predicates */
#include "processenvx.h"      /* command-line argument helpers */

#endif /* WIN32X_H */
