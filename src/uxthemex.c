/*
 * uxthemex.c -- shared runtime state for the header-inline theming helpers.
 */

#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Win32X/uxthemex.h"

THEME_STATE g_theme;
