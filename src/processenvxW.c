#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

#pragma comment(lib, "kernel32.lib")

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Win32X/processenvx.h"
#include "Win32X/windefx.h"
#include "result.h"
#include "processenvxText.inl" /* UNICODE defined for this TU -> GetCommandLineArguments*W */
