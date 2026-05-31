#ifndef WINUSERX_H
#define WINUSERX_H

#include <windows.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * MonitorFromStartupInfo -- resolve the monitor a launch targets, from its STARTUPINFO. Sibling to
 * MonitorFromWindow/MonitorFromPoint/MonitorFromRect: when the shell passes a monitor through
 * STARTF_HASSHELLDATA (hStdOutput), that monitor is returned; otherwise a STARTF_USEPOSITION launch
 * resolves through its dwX/dwY point and any other launch resolves through the origin. dwFlags is the
 * MONITOR_DEFAULTTO* fallback used for the point-based cases. psi may be NULL (treated as the origin).
 */
HMONITOR WINAPI MonitorFromStartupInfo(_In_opt_ const STARTUPINFO* psi, _In_ DWORD dwFlags);

/*
 * CalculateWindowStartupPosition -- compute where this process's top-level window should first appear
 * at launch. It reads the process STARTUPINFO itself via GetStartupInfo, resolves the target monitor
 * with MonitorFromStartupInfo, and places within that monitor's work area. The launcher wins where it
 * speaks: STARTF_USESIZE supplies the extent (dwXSize/dwYSize) and STARTF_USEPOSITION the top-left
 * (dwX/dwY). Where it is silent the extent defaults to *pDefaultSize and the position defaults to
 * centering that extent within the work area. The result is written to *prcOut.
 *
 * Returns FALSE -- leaving *prcOut untouched -- on a NULL argument or when the resolved monitor
 * yields no work area.
 */
_Success_(return != FALSE)
BOOL WINAPI CalculateWindowStartupPosition(_In_ const SIZE* pDefaultSize, _Out_ RECT* prcOut);

#ifdef __cplusplus
}
#endif

#endif /* WINUSERX_H */
