#ifndef WINUSERX_H
#define WINUSERX_H

#include <windows.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * SWX_SHOWSTARTUP -- a ShowWindowEx command (kept clear of the SW_* range so plain SW_* values pass
 * through unchanged): place the window per the launch policy, then show and foreground it.
 */
#define SWX_SHOWSTARTUP 0x00010000

/*
 * GetStartupMonitor -- resolve the monitor this process's launch targets, read from its own
 * STARTUPINFO via GetStartupInfo. When the shell passes a monitor through STARTF_HASSHELLDATA
 * (hStdOutput), that monitor is returned; otherwise a STARTF_USEPOSITION launch resolves through its
 * dwX/dwY point and any other launch through the origin. dwFlags is the MONITOR_DEFAULTTO* fallback
 * for the point-based cases. Unlike the SDK MonitorFrom* family it takes no charset-bearing argument
 * -- the STARTUPINFO is fetched internally and only its charset-neutral fields are read -- so it needs
 * no A/W split.
 */
HMONITOR WINAPI GetStartupMonitor(_In_ DWORD dwFlags);

/*
 * CalculateWindowStartupPosition -- compute where this process's top-level window should first appear
 * at launch. It reads the process STARTUPINFO itself via GetStartupInfo, resolves the target monitor
 * with GetStartupMonitor, and places within that monitor's work area. The launcher wins where it
 * speaks: STARTF_USESIZE supplies the extent (dwXSize/dwYSize) and STARTF_USEPOSITION the top-left
 * (dwX/dwY). Where it is silent the extent defaults to *pDefaultSize and the position defaults to
 * centering that extent within the work area. The result is written to *prcOut.
 *
 * Returns FALSE -- leaving *prcOut untouched -- on a NULL argument or when the resolved monitor
 * yields no work area.
 */
_Success_(return != FALSE)
BOOL WINAPI CalculateWindowStartupPosition(_In_ const SIZE* pDefaultSize, _Out_ RECT* prcOut);

/*
 * ShowWindowEx -- ShowWindow extended with startup-aware commands. SWX_SHOWSTARTUP runs the full
 * first-show sequence: size to a default fraction of the work area, position via
 * CalculateWindowStartupPosition (honoring STARTUPINFO), and show with an activating command
 * (SWP_SHOWWINDOW when placed, else SW_SHOWNORMAL) -- which foregrounds a foreground-entitled launch
 * without forcing (no SetForegroundWindow focus-steal). Any other nShowEx is forwarded to ShowWindow
 * unchanged -- so a caller wanting the
 * STARTUPINFO show state passes SW_SHOWDEFAULT directly, the way CW_USEDEFAULT is opted into.
 * Returns the show result (TRUE when the startup sequence ran).
 */
BOOL WINAPI ShowWindowEx(_In_ HWND hwnd, _In_ int nShowEx);

#ifdef __cplusplus
}
#endif

#endif /* WINUSERX_H */
