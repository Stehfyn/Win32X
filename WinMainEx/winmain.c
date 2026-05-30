/*
 * winmain.c -- WinMainEx, a lean COM server for the exefile "open" verb via DelegateExecute.
 *
 * Why: the launch "spinny" (IDC_APPSTARTING) is armed by the shell when it cold-CreateProcess'es
 * a GUI exe, and nothing the launched process does to itself reaches that decision. The fix is to
 * change WHO launches: register as the DelegateExecute handler for exefile so a double-click routes
 * through this resident, already-input-idle COM server, which performs the CreateProcessW itself --
 * so the shell never arms the cold-launch feedback cursor.
 *
 * NoCRT, custom entry. Imports kernel32, user32, ole32, advapi32, shell32.
 *
 * MODES (parsed by whole-token match on GetCommandLineW)
 *   /unregister    Tear down HKCU registration.
 *   -Embedding     rpcss launched us as a COM server -> run_com_server.
 *   (other)        Direct launch -> self-install on first run, show GUI.
 *
 * REGISTRATION (HKCU only)
 *   CLSID\{guid}\LocalServer32\(Default)        = exe path
 *   exefile\shell\open\command\DelegateExecute  = "{guid}"
 *
 * RECOVERY
 *   cmd /c "<exe>" /unregister
 */

/* NoCRT: disable per-function instrumentation that emits CRT helper calls (_RTC_* under Debug
   /RTC, stack probes, /GS cookie checks) -- none of those support functions exist under
   /NODEFAULTLIB. These are codegen toggles, not warning suppressions. */
#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "/ENTRY:mainCRTStartup")
#pragma comment(linker, "/NODEFAULTLIB")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define INITGUID

#include <windows.h>
#include <windowsx.h>
#include <initguid.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <shlobj.h>

#ifndef STARTF_HASSHELLDATA
#define STARTF_HASSHELLDATA  0x00000400 /* not in the SDK headers; hStdOutput holds the shell HMONITOR */
#endif

#define WMX_WND_CLASS        L"WinMainEx"
#define WMX_WND_TITLE        L"WinMainEx"
#define WMX_FRIENDLY_NAME    L"WinMainEx"
/* Set of every .exe that has activated through us. Value name = full path, data = L"1". */
#define WMX_LIST_KEY         L"Software\\WinMainEx\\Launched"
#define WMX_CLSID_PREFIX     L"Software\\Classes\\CLSID\\"
#define WMX_EXEFILE_CMD_KEY  L"Software\\Classes\\exefile\\shell\\open\\command"
#define WMX_WND_PCT          50   /* window size as a percent of the work area (keeps its aspect ratio) */
#define WMX_PCT_DENOM        100
#define WMX_GUID_CCH         40   /* {8-4-4-4-12} GUID string + brace + nul, per StringFromGUID2 */
#define WMX_SUBKEY_CCH       256
#define WMX_PARAMS_CCH       1024
#define WMX_CMD_CCH          2048
#define WMX_SVR_REFCOUNT     2    /* static class-factory ref: never freed, never zero */

/* GUIDs emitted as storage without uuid.lib. */
DEFINE_GUID(IID_IUnknown,             0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_IClassFactory,        0x00000001, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_IExecuteCommand,      0x7F9185B0, 0xCB92, 0x43C5, 0x80, 0xA9, 0x92, 0x27, 0x7A, 0x4F, 0x7B, 0x54);
DEFINE_GUID(IID_IObjectWithSelection, 0x1C9CD5BB, 0x98E9, 0x4491, 0xA6, 0x0F, 0x31, 0xAA, 0xCC, 0x72, 0xB8, 0x3C);
DEFINE_GUID(CLSID_WinMainEx,          0xE5F1A9C2, 0x8B7D, 0x4E3F, 0xA1, 0x5C, 0x9D, 0x2E, 0x7B, 0x6F, 0x4A, 0x83);

/* CommandObject implements IExecuteCommand + IObjectWithSelection. The two vtbl pointers lead so
   EC_OBJ/OWS_OBJ container_of works; remaining members are ordered largest-alignment-first so the
   layout carries no interior padding. */
typedef struct CommandObject
{
    const IExecuteCommandVtbl      *vtbl_ec;
    const IObjectWithSelectionVtbl *vtbl_ows;
    IShellItemArray                *selection;
    POINT                           pos;
    LONG                            ref;
    int                             show_window;
    BOOL                            show_set;
    BOOL                            pos_set;
    WCHAR                           params[WMX_PARAMS_CCH];
    WCHAR                           directory[MAX_PATH];
} CommandObject;

typedef struct ClassFactory
{
    const IClassFactoryVtbl *vtbl;
} ClassFactory;

static volatile LONG g_object_count;
static BOOL          g_is_com_server; /* TRUE in run_com_server; WM_DESTROY must NOT exit then */
static WCHAR         g_my_path[MAX_PATH];
/* Reusable command-line buffer for ec_Execute. The STA serializes Execute() calls on one thread,
   so a single shared buffer is safe. Lives in .bss (zero-initialized by the loader). */
static WCHAR         g_cmd_buf[WMX_CMD_CCH];

/* NoCRT replacement for memcmp; IsEqualIID lowers to a memcmp. #pragma function opts out of the
   /Oi intrinsic so this definition is the one that links. SAL matches the CRT declaration so the
   redefinition is annotation-consistent (C28251). */
#pragma function(memcmp)
_Check_return_
int __cdecl memcmp(_In_reads_bytes_(cb) const void *pvA, _In_reads_bytes_(cb) const void *pvB, _In_ size_t cb)
{
    const BYTE *pbA;
    const BYTE *pbB;

    pbA = (const BYTE *)pvA;
    pbB = (const BYTE *)pvB;
    while (0u != cb)
    {
        if ((*pbA) != (*pbB))
        {
            return (int)(*pbA) - (int)(*pbB);
        }
        pbA++;
        pbB++;
        cb--;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Small inline string helpers (NoCRT, kernel32-only)                  */
/* ------------------------------------------------------------------ */

static int wmx_strlen(LPCWSTR psz)
{
    int cch;

    cch = 0;
    while (0 != psz[cch])
    {
        cch++;
    }
    return cch;
}

static void wmx_strcpy(LPWSTR pszDst, LPCWSTR pszSrc)
{
    while (0 != (*pszSrc))
    {
        (*pszDst) = (*pszSrc);
        pszDst++;
        pszSrc++;
    }
    (*pszDst) = 0;
}

/* Append pszSrc onto pszDst; return a pointer to the new terminating nul so callers can chain. */
static LPWSTR wmx_strappend(LPWSTR pszDst, LPCWSTR pszSrc)
{
    while (0 != (*pszSrc))
    {
        (*pszDst) = (*pszSrc);
        pszDst++;
        pszSrc++;
    }
    (*pszDst) = 0;
    return pszDst;
}

/* Whole-token, case-insensitive search for pszTarget across the command line pszCmd. */
static BOOL wmx_isarg(LPCWSTR pszCmd, LPCWSTR pszTarget)
{
    LPCWSTR p;
    int     cchTarget;
    WCHAR   chPre;
    WCHAR   chPost;
    BOOL    fTokenStart;
    BOOL    fMatch;
    BOOL    fTokenEnd;

    cchTarget = wmx_strlen(pszTarget);
    for (p = pszCmd; 0 != (*p); p++)
    {
        if (p == pszCmd)
        {
            chPre = L' ';
        }
        else
        {
            chPre = p[-1];
        }
        fTokenStart = (L' ' == chPre) || (L'"' == chPre);
        if (!fTokenStart)
        {
            continue;
        }
        fMatch = (CSTR_EQUAL == CompareStringOrdinal(p, cchTarget, pszTarget, cchTarget, TRUE));
        if (!fMatch)
        {
            continue;
        }
        chPost    = p[cchTarget];
        fTokenEnd = (0 == chPost) || (L' ' == chPost) || (L'"' == chPost);
        if (fTokenEnd)
        {
            return TRUE;
        }
    }
    return FALSE;
}

/* ------------------------------------------------------------------ */
/* Launched-exe set (HKCU)                                             */
/* ------------------------------------------------------------------ */

/* Add an activated exe path to the set. One create+set, idempotent, errors ignored -- recording
   must never affect the launch. */
static void record_exe(LPCWSTR pszPath)
{
    static const WCHAR szOne[] = L"1";
    HKEY               hKey;
    LONG               lr;

    lr = RegCreateKeyExW(HKEY_CURRENT_USER, WMX_LIST_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    if (ERROR_SUCCESS == lr)
    {
        RegSetValueExW(hKey, pszPath, 0, REG_SZ, (const BYTE *)szOne, (DWORD)sizeof(szOne));
        RegCloseKey(hKey);
    }
}

/* Is this exe already in the set (i.e. opted in to our handling)? */
static BOOL is_exe_registered(LPCWSTR pszPath)
{
    HKEY hKey;
    LONG lrOpen;
    LONG lrQuery;
    BOOL fFound;

    fFound = FALSE;
    lrOpen = RegOpenKeyExW(HKEY_CURRENT_USER, WMX_LIST_KEY, 0, KEY_QUERY_VALUE, &hKey);
    if (ERROR_SUCCESS == lrOpen)
    {
        lrQuery = RegQueryValueExW(hKey, pszPath, NULL, NULL, NULL, NULL);
        fFound  = (ERROR_SUCCESS == lrQuery);
        RegCloseKey(hKey);
    }
    return fFound;
}

/* ------------------------------------------------------------------ */
/* GUI: one overlapped window per show_gui call                        */
/* ------------------------------------------------------------------ */

/* void Cls_OnDestroy(HWND hwnd) */
static void WinMainEx_OnDestroy(HWND hwnd)
{
    UNREFERENCED_PARAMETER(hwnd);

    /* Direct-run mode: the window IS the process -> quit. COM-server mode: windows come and go,
       the server stays alive. */
    if (!g_is_com_server)
    {
        PostQuitMessage(0);
    }
}

static LRESULT CALLBACK WinMainEx_WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_DESTROY, WinMainEx_OnDestroy);
    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}

/* Size hwnd to WMX_WND_PCT% of the monitor work area (which preserves the work-area aspect ratio)
   and center it there. */
static void CenterOnMonitor(HWND hwnd, HMONITOR hMon)
{
    MONITORINFO mi;
    BOOL        fGotInfo;
    int         nWorkWidth;
    int         nWorkHeight;
    int         nWidth;
    int         nHeight;
    int         nX;
    int         nY;

    SecureZeroMemory(&mi, sizeof(mi));
    mi.cbSize = (DWORD)sizeof(mi);
    fGotInfo  = GetMonitorInfoW(hMon, &mi);
    if (!fGotInfo)
    {
        return;
    }
    nWorkWidth  = (int)(mi.rcWork.right - mi.rcWork.left);
    nWorkHeight = (int)(mi.rcWork.bottom - mi.rcWork.top);
    nWidth      = nWorkWidth * WMX_WND_PCT / WMX_PCT_DENOM;
    nHeight     = nWorkHeight * WMX_WND_PCT / WMX_PCT_DENOM;
    nX          = mi.rcWork.left + (nWorkWidth - nWidth) / 2;
    nY          = mi.rcWork.top + (nWorkHeight - nHeight) / 2;
    SetWindowPos(hwnd, NULL, nX, nY, nWidth, nHeight, SWP_NOZORDER | SWP_NOACTIVATE);
}

/* Show our window honoring the launcher's STARTUPINFO show state, then size+center it. For the COM
   path psi is synthesized from the shell's IExecuteCommand intent; for the direct path it is the
   real GetStartupInfoW. We always center our own window (the shell's SetPosition is an icon-click
   point, not a window origin); position pass-through still applies to FORWARDED child launches. */
static void show_gui(const STARTUPINFOW *psi)
{
    static BOOL s_fClassRegistered = FALSE;
    WNDCLASSW   wc;
    HINSTANCE   hInstance;
    HMONITOR    hMon;
    HWND        hwnd;
    int         nCmdShow;
    BOOL        fUseShow;
    BOOL        fHasShellData;

    hInstance = GetModuleHandleW(NULL);
    fUseShow  = !!(STARTF_USESHOWWINDOW & psi->dwFlags);
    if (fUseShow)
    {
        nCmdShow = (int)psi->wShowWindow;
    }
    else
    {
        nCmdShow = SW_SHOWDEFAULT;
    }

    if (!s_fClassRegistered)
    {
        SecureZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc   = WinMainEx_WndProc;
        wc.hInstance     = hInstance;
        wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = WMX_WND_CLASS;
        RegisterClassW(&wc);
        s_fClassRegistered = TRUE;
    }

    /* Created hidden at default size; CenterOnMonitor then sizes + positions it before we honor
       the requested show state. */
    hwnd = CreateWindowExW(0, WMX_WND_CLASS, WMX_WND_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                           CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);
    if (NULL == hwnd)
    {
        return;
    }

    fHasShellData = !!(STARTF_HASSHELLDATA & psi->dwFlags);
    if (fHasShellData)
    {
        hMon = (HMONITOR)psi->hStdOutput;
    }
    else
    {
        hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    }
    CenterOnMonitor(hwnd, hMon);
    ShowWindow(hwnd, nCmdShow);
    SetForegroundWindow(hwnd);
}

/* ------------------------------------------------------------------ */
/* CommandObject: IExecuteCommand + IObjectWithSelection                */
/* ------------------------------------------------------------------ */

#define EC_OBJ(p)  ((CommandObject *)((BYTE *)(p) - offsetof(CommandObject, vtbl_ec)))
#define OWS_OBJ(p) ((CommandObject *)((BYTE *)(p) - offsetof(CommandObject, vtbl_ows)))

static HRESULT cmd_QI(CommandObject *pObj, REFIID riid, void **ppv)
{
    BOOL fUnknown;
    BOOL fExecute;
    BOOL fSelection;

    fUnknown   = IsEqualIID(riid, &IID_IUnknown);
    fExecute   = IsEqualIID(riid, &IID_IExecuteCommand);
    fSelection = IsEqualIID(riid, &IID_IObjectWithSelection);
    if (fUnknown || fExecute)
    {
        (*ppv) = (void *)&pObj->vtbl_ec;
    }
    else if (fSelection)
    {
        (*ppv) = (void *)&pObj->vtbl_ows;
    }
    else
    {
        (*ppv) = NULL;
        return E_NOINTERFACE;
    }
    InterlockedIncrement(&pObj->ref);
    return S_OK;
}

static ULONG cmd_AddRef(CommandObject *pObj)
{
    return (ULONG)InterlockedIncrement(&pObj->ref);
}

static ULONG cmd_Release(CommandObject *pObj)
{
    LONG lRef;

    lRef = InterlockedDecrement(&pObj->ref);
    if (0 == lRef)
    {
        if (NULL != pObj->selection)
        {
            IShellItemArray_Release(pObj->selection);
        }
        HeapFree(GetProcessHeap(), 0, pObj);
        InterlockedDecrement(&g_object_count);
    }
    return (ULONG)lRef;
}

/* IExecuteCommand */
static HRESULT STDMETHODCALLTYPE ec_QI(IExecuteCommand *pThis, REFIID riid, void **ppv)
{
    return cmd_QI(EC_OBJ(pThis), riid, ppv);
}

static ULONG STDMETHODCALLTYPE ec_AddRef(IExecuteCommand *pThis)
{
    return cmd_AddRef(EC_OBJ(pThis));
}

static ULONG STDMETHODCALLTYPE ec_Release(IExecuteCommand *pThis)
{
    return cmd_Release(EC_OBJ(pThis));
}

static HRESULT STDMETHODCALLTYPE ec_SetKeyState(IExecuteCommand *pThis, DWORD dwKeyState)
{
    UNREFERENCED_PARAMETER(pThis);
    UNREFERENCED_PARAMETER(dwKeyState);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ec_SetParameters(IExecuteCommand *pThis, LPCWSTR pszParams)
{
    CommandObject *pObj;
    int            cch;

    pObj = EC_OBJ(pThis);
    cch  = 0;
    if (NULL != pszParams)
    {
        while ((0 != pszParams[cch]) && ((WMX_PARAMS_CCH - 1) > cch))
        {
            pObj->params[cch] = pszParams[cch];
            cch++;
        }
    }
    pObj->params[cch] = 0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ec_SetPosition(IExecuteCommand *pThis, POINT pt)
{
    CommandObject *pObj;

    pObj          = EC_OBJ(pThis);
    pObj->pos     = pt;
    pObj->pos_set = TRUE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ec_SetShowWindow(IExecuteCommand *pThis, int nShow)
{
    CommandObject *pObj;

    pObj              = EC_OBJ(pThis);
    pObj->show_window = nShow;
    pObj->show_set    = TRUE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ec_SetNoShowUI(IExecuteCommand *pThis, BOOL fNoShowUI)
{
    UNREFERENCED_PARAMETER(pThis);
    UNREFERENCED_PARAMETER(fNoShowUI);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ec_SetDirectory(IExecuteCommand *pThis, LPCWSTR pszDir)
{
    CommandObject *pObj;
    int            cch;

    pObj = EC_OBJ(pThis);
    cch  = 0;
    if (NULL != pszDir)
    {
        while ((0 != pszDir[cch]) && ((MAX_PATH - 1) > cch))
        {
            pObj->directory[cch] = pszDir[cch];
            cch++;
        }
    }
    pObj->directory[cch] = 0;
    return S_OK;
}

/* Build a STARTUPINFO that carries the shell's show state (and, for child launches, position). */
static void wmx_fill_startupinfo(const CommandObject *pObj, STARTUPINFOW *psi, BOOL fForwardChild)
{
    SecureZeroMemory(psi, sizeof(*psi));
    psi->cb = (DWORD)sizeof(*psi);
    if (pObj->show_set)
    {
        psi->dwFlags |= STARTF_USESHOWWINDOW;
        psi->wShowWindow = (WORD)pObj->show_window;
    }
    /* Position pass-through only for forwarded children; our own window always centers. */
    if (fForwardChild && pObj->pos_set)
    {
        psi->dwFlags |= STARTF_USEPOSITION;
        psi->dwX = (DWORD)pObj->pos.x;
        psi->dwY = (DWORD)pObj->pos.y;
    }
}

/* --- THE HOT PATH ------------------------------------------------- */
static HRESULT STDMETHODCALLTYPE ec_Execute(IExecuteCommand *pThis)
{
    CommandObject      *pObj;
    IShellItem         *pItem;
    LPWSTR              pszPath;
    LPWSTR              pszWrite;
    LPCWSTR             pszDir;
    STARTUPINFOW        si;
    PROCESS_INFORMATION pi;
    SHELLEXECUTEINFOW   sei;
    HRESULT             hr;
    HRESULT             rc;
    DWORD               dwErr;
    BOOL                fIsSelf;
    BOOL                fRegistered;
    BOOL                fHasParams;
    BOOL                fCreated;

    pObj = EC_OBJ(pThis);

    if (NULL == pObj->selection)
    {
        wmx_fill_startupinfo(pObj, &si, FALSE);
        show_gui(&si);
        return S_OK;
    }

    hr = IShellItemArray_GetItemAt(pObj->selection, 0, &pItem);
    if (FAILED(hr))
    {
        return E_FAIL;
    }
    pszPath = NULL;
    hr      = IShellItem_GetDisplayName(pItem, SIGDN_FILESYSPATH, &pszPath);
    IShellItem_Release(pItem);
    if (FAILED(hr) || (NULL == pszPath))
    {
        return hr;
    }

    /* Single ordinal full-path compare against the cached g_my_path. */
    fIsSelf = (CSTR_EQUAL == CompareStringOrdinal(pszPath, -1, g_my_path, -1, TRUE));
    if (fIsSelf)
    {
        CoTaskMemFree(pszPath);
        wmx_fill_startupinfo(pObj, &si, FALSE);
        show_gui(&si);
        return S_OK;
    }

    /* Non-self exe. Only exes already in the set get the cursor-free treatment; a first-seen exe is
       recorded (the one-call bootstrap) and launched UNMOLESTED this time (normal feedback). */
    fRegistered = is_exe_registered(pszPath);
    if (!fRegistered)
    {
        record_exe(pszPath);
    }

    /* Forward: build "<path>" + params into g_cmd_buf. The CreateProcessW runs from this resident,
       input-idle server, so the shell never arms IDC_APPSTARTING. */
    pszWrite     = g_cmd_buf;
    (*pszWrite)  = L'"';
    pszWrite++;
    pszWrite     = wmx_strappend(pszWrite, pszPath);
    (*pszWrite)  = L'"';
    pszWrite++;
    fHasParams = (0 != pObj->params[0]);
    if (fHasParams)
    {
        (*pszWrite) = L' ';
        pszWrite++;
        pszWrite = wmx_strappend(pszWrite, pObj->params);
    }
    (*pszWrite) = 0;

    wmx_fill_startupinfo(pObj, &si, TRUE);
    /* Only opted-in exes get cursor suppression; non-registered launches stay exactly as the shell
       would have done them (no flag -> normal feedback). */
    if (fRegistered)
    {
        si.dwFlags |= STARTF_FORCEOFFFEEDBACK;
    }

    pszDir = NULL;
    if (0 != pObj->directory[0])
    {
        pszDir = pObj->directory;
    }

    fCreated = CreateProcessW(NULL, g_cmd_buf, NULL, NULL, FALSE, 0, NULL, pszDir, &si, &pi);
    if (fCreated)
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        rc = S_OK;
    }
    else
    {
        dwErr = GetLastError();
        if (ERROR_ELEVATION_REQUIRED == dwErr)
        {
            /* Target requires admin: route through the 'runas' verb (exefile\shell\runas, NOT our
               'open' DelegateExecute, so no re-entry) for the normal UAC consent prompt. */
            SecureZeroMemory(&sei, sizeof(sei));
            sei.cbSize       = (DWORD)sizeof(sei);
            sei.fMask        = SEE_MASK_FLAG_NO_UI;
            sei.lpVerb       = L"runas";
            sei.lpFile       = pszPath;
            sei.lpDirectory  = pszDir;
            sei.nShow        = SW_SHOWNORMAL;
            if (pObj->params[0])
            {
                sei.lpParameters = pObj->params;
            }
            if (pObj->show_set)
            {
                sei.nShow = pObj->show_window;
            }
            if (ShellExecuteExW(&sei))
            {
                rc = S_OK;
            }
            else
            {
                rc = HRESULT_FROM_WIN32(GetLastError());
            }
        }
        else
        {
            rc = HRESULT_FROM_WIN32(dwErr);
        }
    }
    CoTaskMemFree(pszPath);
    return rc;
}

static const IExecuteCommandVtbl g_ec_vtbl = {
    ec_QI,        ec_AddRef,       ec_Release,     ec_SetKeyState, ec_SetParameters,
    ec_SetPosition, ec_SetShowWindow, ec_SetNoShowUI, ec_SetDirectory, ec_Execute};

/* IObjectWithSelection */
static HRESULT STDMETHODCALLTYPE ows_QI(IObjectWithSelection *pThis, REFIID riid, void **ppv)
{
    return cmd_QI(OWS_OBJ(pThis), riid, ppv);
}

static ULONG STDMETHODCALLTYPE ows_AddRef(IObjectWithSelection *pThis)
{
    return cmd_AddRef(OWS_OBJ(pThis));
}

static ULONG STDMETHODCALLTYPE ows_Release(IObjectWithSelection *pThis)
{
    return cmd_Release(OWS_OBJ(pThis));
}

static HRESULT STDMETHODCALLTYPE ows_SetSelection(IObjectWithSelection *pThis, IShellItemArray *psia)
{
    CommandObject *pObj;

    pObj = OWS_OBJ(pThis);
    if (NULL != pObj->selection)
    {
        IShellItemArray_Release(pObj->selection);
    }
    pObj->selection = psia;
    if (NULL != psia)
    {
        IShellItemArray_AddRef(psia);
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ows_GetSelection(IObjectWithSelection *pThis, REFIID riid, void **ppv)
{
    CommandObject *pObj;

    pObj = OWS_OBJ(pThis);
    if (NULL == pObj->selection)
    {
        (*ppv) = NULL;
        return E_FAIL;
    }
    return IShellItemArray_QueryInterface(pObj->selection, riid, ppv);
}

static const IObjectWithSelectionVtbl g_ows_vtbl = {
    ows_QI, ows_AddRef, ows_Release, ows_SetSelection, ows_GetSelection};

/* ------------------------------------------------------------------ */
/* IClassFactory singleton                                             */
/* ------------------------------------------------------------------ */

static HRESULT STDMETHODCALLTYPE cf_QI(IClassFactory *pThis, REFIID riid, void **ppv)
{
    BOOL fUnknown;
    BOOL fFactory;

    fUnknown = IsEqualIID(riid, &IID_IUnknown);
    fFactory = IsEqualIID(riid, &IID_IClassFactory);
    if (fUnknown || fFactory)
    {
        (*ppv) = pThis;
        return S_OK;
    }
    (*ppv) = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE cf_AddRef(IClassFactory *pThis)
{
    UNREFERENCED_PARAMETER(pThis);
    return WMX_SVR_REFCOUNT;
}

static ULONG STDMETHODCALLTYPE cf_Release(IClassFactory *pThis)
{
    UNREFERENCED_PARAMETER(pThis);
    return WMX_SVR_REFCOUNT - 1;
}

static HRESULT STDMETHODCALLTYPE cf_CreateInstance(IClassFactory *pThis, IUnknown *pOuter, REFIID riid, void **ppv)
{
    CommandObject *pObj;
    HRESULT        hr;

    UNREFERENCED_PARAMETER(pThis);

    (*ppv) = NULL;
    if (NULL != pOuter)
    {
        return CLASS_E_NOAGGREGATION;
    }
    pObj = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CommandObject));
    if (NULL == pObj)
    {
        return E_OUTOFMEMORY;
    }
    pObj->vtbl_ec  = &g_ec_vtbl;
    pObj->vtbl_ows = &g_ows_vtbl;
    pObj->ref      = 1;
    InterlockedIncrement(&g_object_count);
    hr = cmd_QI(pObj, riid, ppv);
    cmd_Release(pObj);
    return hr;
}

static HRESULT STDMETHODCALLTYPE cf_LockServer(IClassFactory *pThis, BOOL fLock)
{
    UNREFERENCED_PARAMETER(pThis);
    UNREFERENCED_PARAMETER(fLock);
    return S_OK;
}

static const IClassFactoryVtbl g_cf_vtbl = {cf_QI, cf_AddRef, cf_Release, cf_CreateInstance, cf_LockServer};
static ClassFactory            g_class_factory = {&g_cf_vtbl};

/* ------------------------------------------------------------------ */
/* Registry install / uninstall (HKCU)                                 */
/* ------------------------------------------------------------------ */

static void clsid_str(WCHAR rgClsid[WMX_GUID_CCH])
{
    int cch;

    /* WMX_GUID_CCH is provably large enough for a GUID string, but check the count rather than
       discard it (C6031); fall back to an empty string on the impossible failure. */
    cch = StringFromGUID2(&CLSID_WinMainEx, rgClsid, WMX_GUID_CCH);
    if (0 == cch)
    {
        rgClsid[0] = 0;
    }
}

static LONG reg_set_sz(HKEY hParent, LPCWSTR pszSubKey, LPCWSTR pszName, LPCWSTR pszValue)
{
    HKEY  hKey;
    LONG  lr;
    int   cch;
    DWORD cbValue;

    lr = RegCreateKeyExW(hParent, pszSubKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    if (ERROR_SUCCESS != lr)
    {
        return lr;
    }
    cch     = wmx_strlen(pszValue);
    cbValue = (DWORD)(((size_t)cch + 1) * sizeof(WCHAR));
    lr      = RegSetValueExW(hKey, pszName, 0, REG_SZ, (const BYTE *)pszValue, cbValue);
    RegCloseKey(hKey);
    return lr;
}

static int do_register(void)
{
    WCHAR  rgClsid[WMX_GUID_CCH];
    WCHAR  szSub[WMX_SUBKEY_CCH];
    LPWSTR pszWrite;
    LONG   lr;

    clsid_str(rgClsid);

    /* CLSID\{guid} (Default) = friendly name */
    pszWrite = wmx_strappend(szSub, WMX_CLSID_PREFIX);
    wmx_strcpy(pszWrite, rgClsid);
    lr = reg_set_sz(HKEY_CURRENT_USER, szSub, NULL, WMX_FRIENDLY_NAME);
    if (ERROR_SUCCESS != lr)
    {
        return 1;
    }

    /* CLSID\{guid}\LocalServer32 (Default) = our exe path */
    pszWrite = wmx_strappend(szSub, WMX_CLSID_PREFIX);
    pszWrite = wmx_strappend(pszWrite, rgClsid);
    wmx_strcpy(pszWrite, L"\\LocalServer32");
    lr = reg_set_sz(HKEY_CURRENT_USER, szSub, NULL, g_my_path);
    if (ERROR_SUCCESS != lr)
    {
        return 1;
    }

    /* exefile open command default + DelegateExecute = our CLSID */
    lr = reg_set_sz(HKEY_CURRENT_USER, WMX_EXEFILE_CMD_KEY, NULL, L"\"%1\" %*");
    if (ERROR_SUCCESS != lr)
    {
        return 1;
    }
    lr = reg_set_sz(HKEY_CURRENT_USER, WMX_EXEFILE_CMD_KEY, L"DelegateExecute", rgClsid);
    if (ERROR_SUCCESS != lr)
    {
        return 1;
    }

    /* Tell the shell the exefile association changed, else Explorer keeps its cached (cold-launch)
       association and never calls our handler. */
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return 0;
}

static int do_unregister(void)
{
    WCHAR  rgClsid[WMX_GUID_CCH];
    WCHAR  szSub[WMX_SUBKEY_CCH];
    LPWSTR pszWrite;
    HKEY   hKey;
    LONG   lrOpen;

    clsid_str(rgClsid);
    pszWrite = wmx_strappend(szSub, WMX_CLSID_PREFIX);
    wmx_strcpy(pszWrite, rgClsid);
    RegDeleteTreeW(HKEY_CURRENT_USER, szSub);

    lrOpen = RegOpenKeyExW(HKEY_CURRENT_USER, WMX_EXEFILE_CMD_KEY, 0, KEY_SET_VALUE, &hKey);
    if (ERROR_SUCCESS == lrOpen)
    {
        RegDeleteValueW(hKey, L"DelegateExecute");
        RegDeleteValueW(hKey, NULL);
        RegCloseKey(hKey);
    }
    RegDeleteKeyExW(HKEY_CURRENT_USER, WMX_EXEFILE_CMD_KEY, 0, 0);
    RegDeleteKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\exefile\\shell\\open", 0, 0);
    RegDeleteKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\exefile\\shell", 0, 0);
    RegDeleteKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\exefile", 0, 0);
    return 0;
}

static BOOL is_registered(void)
{
    WCHAR  rgClsid[WMX_GUID_CCH];
    WCHAR  szSub[WMX_SUBKEY_CCH];
    LPWSTR pszWrite;
    HKEY   hKey;
    LONG   lrOpen;

    clsid_str(rgClsid);
    pszWrite = wmx_strappend(szSub, WMX_CLSID_PREFIX);
    pszWrite = wmx_strappend(pszWrite, rgClsid);
    wmx_strcpy(pszWrite, L"\\LocalServer32");
    lrOpen = RegOpenKeyExW(HKEY_CURRENT_USER, szSub, 0, KEY_READ, &hKey);
    if (ERROR_SUCCESS != lrOpen)
    {
        return FALSE;
    }
    RegCloseKey(hKey);
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Modes                                                               */
/* ------------------------------------------------------------------ */

static int run_com_server(void)
{
    MSG     msg;
    HRESULT hrInit;
    HRESULT hrReg;
    DWORD   dwCookie;

    g_is_com_server = TRUE;
    hrInit          = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hrInit))
    {
        return 1;
    }
    dwCookie = 0;
    hrReg    = CoRegisterClassObject(&CLSID_WinMainEx, (IUnknown *)&g_class_factory,
                                     CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &dwCookie);
    if (FAILED(hrReg))
    {
        CoUninitialize();
        return 2;
    }

    /* Standard STA pump: dispatches COM RPC (ole32's hidden STA window) and any show_gui window
       messages. WM_QUIT comes from PostQuitMessage in WinMainEx_OnDestroy, or from the OS at logoff. */
    while (0 < GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (0 != dwCookie)
    {
        CoRevokeClassObject(dwCookie);
    }
    CoUninitialize();
    return 0;
}

static int run_direct(void)
{
    STARTUPINFOW si;
    MSG          msg;

    if (!is_registered())
    {
        do_register();
    }
    /* Direct launch: honor the real launcher's STARTUPINFO. GetStartupInfoW fills it completely. */
    GetStartupInfoW(&si);
    show_gui(&si);
    while (0 < GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Entry -- NoCRT custom entry                                         */
/* ------------------------------------------------------------------ */

void __cdecl mainCRTStartup(void)
{
    LPCWSTR cmd;
    int     rc;
    BOOL    fUnregister;
    BOOL    fEmbedding;

    GetModuleFileNameW(NULL, g_my_path, MAX_PATH);

    cmd         = GetCommandLineW();
    fUnregister = wmx_isarg(cmd, L"/unregister");
    fEmbedding  = wmx_isarg(cmd, L"-Embedding") || wmx_isarg(cmd, L"/Embedding");
    if (fUnregister)
    {
        rc = do_unregister();
    }
    else if (fEmbedding)
    {
        rc = run_com_server();
    }
    else
    {
        rc = run_direct();
    }
    ExitProcess((UINT)rc);
}
