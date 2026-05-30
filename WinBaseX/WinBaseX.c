/*
 * WinBaseX.c -- no-CRT static library for WinMainEx/wWinMainEx startup and
 * exefile DelegateExecute launch forwarding.
 */

#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

#pragma comment(linker, "/NODEFAULTLIB")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif
#define INITGUID

#include "WinBaseX.h"
#include <windowsx.h>
#include <initguid.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stddef.h>

#ifndef STARTF_HASSHELLDATA
#define STARTF_HASSHELLDATA     0x00000400
#endif

#define WBX_DEFAULT_LIST_KEY    L"Software\\WinMainEx\\Launched"
#define WBX_CLSID_PREFIX        L"Software\\Classes\\CLSID\\"
#define WBX_EXEFILE_CMD_KEY     L"Software\\Classes\\exefile\\shell\\open\\command"
#define WBX_GUID_CCH            40
#define WBX_SUBKEY_CCH          256
#define WBX_PARAMS_CCH          1024
#define WBX_CMD_CCH             2048
#define WBX_SVR_REFCOUNT        2

DEFINE_GUID(IID_IUnknown,             0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_IClassFactory,        0x00000001, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_IExecuteCommand,      0x7F9185B0, 0xCB92, 0x43C5, 0x80, 0xA9, 0x92, 0x27, 0x7A, 0x4F, 0x7B, 0x54);
DEFINE_GUID(IID_IObjectWithSelection, 0x1C9CD5BB, 0x98E9, 0x4491, 0xA6, 0x0F, 0x31, 0xAA, 0xCC, 0x72, 0xB8, 0x3C);

typedef int (WINAPI *WBX_PFN_WINMAINEXA)(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nShowCmd,
    const STARTUPINFOA *lpStartupInfo
    );

typedef int (WINAPI *WBX_PFN_WINMAINEXW)(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine,
    int nShowCmd,
    const STARTUPINFOW *lpStartupInfo
    );

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
    WCHAR                           params[WBX_PARAMS_CCH];
    WCHAR                           directory[MAX_PATH];
} CommandObject;

typedef struct ClassFactory
{
    const IClassFactoryVtbl *vtbl;
} ClassFactory;

static const IExecuteCommandVtbl      g_ec_vtbl;
static const IObjectWithSelectionVtbl g_ows_vtbl;
static const IClassFactoryVtbl        g_cf_vtbl;

static volatile LONG  g_object_count;
static BOOL           g_f_com_server;
static BOOL           g_f_registration_loaded;
static BOOL           g_f_use_wide_callback;
static GUID           g_clsid;
static LPCWSTR        g_psz_friendly_name;
static LPCWSTR        g_psz_launch_history_key;
static WCHAR          g_my_path[MAX_PATH];
static WCHAR          g_cmd_buf[WBX_CMD_CCH];
static CHAR           g_cmd_buf_a[WBX_CMD_CCH];
static WBX_PFN_WINMAINEXA g_pfn_winmainexa;
static WBX_PFN_WINMAINEXW g_pfn_winmainexw;
static ClassFactory   g_class_factory = { &g_cf_vtbl };

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

BOOL WINAPI IsWinBaseXComServer(void)
{
    return g_f_com_server;
}

static int wbx_strlen(LPCWSTR psz)
{
    int cch;

    cch = 0;
    while (0 != psz[cch])
    {
        cch++;
    }
    return cch;
}

static void wbx_strcpy(LPWSTR pszDst, LPCWSTR pszSrc)
{
    while (0 != (*pszSrc))
    {
        (*pszDst) = (*pszSrc);
        pszDst++;
        pszSrc++;
    }
    (*pszDst) = 0;
}

static LPWSTR wbx_strappend(LPWSTR pszDst, LPCWSTR pszSrc)
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

static BOOL wbx_isarg(LPCWSTR pszCmd, LPCWSTR pszTarget)
{
    LPCWSTR p;
    int     cchTarget;
    WCHAR   chPre;
    WCHAR   chPost;
    BOOL    fTokenStart;
    BOOL    fMatch;
    BOOL    fTokenEnd;

    cchTarget = wbx_strlen(pszTarget);
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

static LPWSTR wbx_winmain_command_line_w(void)
{
    LPWSTR pszCmd;

    pszCmd = GetCommandLineW();
    if (L'"' == (*pszCmd))
    {
        pszCmd++;
        while ((0 != (*pszCmd)) && (L'"' != (*pszCmd)))
        {
            pszCmd++;
        }
        if (L'"' == (*pszCmd))
        {
            pszCmd++;
        }
    }
    else
    {
        while ((0 != (*pszCmd)) && (L' ' < (*pszCmd)))
        {
            pszCmd++;
        }
    }
    while ((L' ' == (*pszCmd)) || (L'\t' == (*pszCmd)))
    {
        pszCmd++;
    }
    return pszCmd;
}

static LPSTR wbx_winmain_command_line_a(void)
{
    LPSTR pszCmd;

    pszCmd = GetCommandLineA();
    if ('"' == (*pszCmd))
    {
        pszCmd++;
        while ((0 != (*pszCmd)) && ('"' != (*pszCmd)))
        {
            pszCmd++;
        }
        if ('"' == (*pszCmd))
        {
            pszCmd++;
        }
    }
    else
    {
        while ((0 != (*pszCmd)) && (' ' < (*pszCmd)))
        {
            pszCmd++;
        }
    }
    while ((' ' == (*pszCmd)) || ('\t' == (*pszCmd)))
    {
        pszCmd++;
    }
    return pszCmd;
}

static int wbx_show_window_from_startup_w(const STARTUPINFOW *psi)
{
    BOOL fUseShow;

    fUseShow = !!(STARTF_USESHOWWINDOW & psi->dwFlags);
    if (fUseShow)
    {
        return (int)psi->wShowWindow;
    }
    return SW_SHOWDEFAULT;
}

static int wbx_show_window_from_startup_a(const STARTUPINFOA *psi)
{
    BOOL fUseShow;

    fUseShow = !!(STARTF_USESHOWWINDOW & psi->dwFlags);
    if (fUseShow)
    {
        return (int)psi->wShowWindow;
    }
    return SW_SHOWDEFAULT;
}

static void wbx_startupinfo_w_to_a(const STARTUPINFOW *psiW, STARTUPINFOA *psiA)
{
    SecureZeroMemory(psiA, sizeof((*psiA)));
    psiA->cb              = (DWORD)sizeof((*psiA));
    psiA->dwX             = psiW->dwX;
    psiA->dwY             = psiW->dwY;
    psiA->dwXSize         = psiW->dwXSize;
    psiA->dwYSize         = psiW->dwYSize;
    psiA->dwXCountChars   = psiW->dwXCountChars;
    psiA->dwYCountChars   = psiW->dwYCountChars;
    psiA->dwFillAttribute = psiW->dwFillAttribute;
    psiA->dwFlags         = psiW->dwFlags;
    psiA->wShowWindow     = psiW->wShowWindow;
    psiA->cbReserved2     = psiW->cbReserved2;
    psiA->lpReserved2     = psiW->lpReserved2;
    psiA->hStdInput       = psiW->hStdInput;
    psiA->hStdOutput      = psiW->hStdOutput;
    psiA->hStdError       = psiW->hStdError;
}

static LPSTR wbx_command_line_w_to_a(LPCWSTR pszCmdLine)
{
    int cch;

    g_cmd_buf_a[0] = 0;
    cch = WideCharToMultiByte(CP_ACP, 0, pszCmdLine, -1, g_cmd_buf_a, WBX_CMD_CCH, NULL, NULL);
    if (0 == cch)
    {
        g_cmd_buf_a[0] = 0;
    }
    return g_cmd_buf_a;
}

static BOOL wbx_load_registration(void)
{
    WINBASEX_REGISTRATION_PROPERTIESW props;
    BOOL                              fGotProps;
    BOOL                              fValidCb;
    BOOL                              fValidFields;

    if (g_f_registration_loaded)
    {
        return TRUE;
    }

    SecureZeroMemory(&props, sizeof(props));
    props.cb  = (DWORD)sizeof(props);
    fGotProps = GetWinBaseXRegistrationProperties(&props);
    if (!fGotProps)
    {
        return FALSE;
    }

    fValidCb     = (sizeof(props) <= props.cb);
    fValidFields = (NULL != props.lpClsid) && (NULL != props.lpFriendlyName) && (0 == props.dwFlags);
    if (!fValidCb || !fValidFields)
    {
        return FALSE;
    }

    g_clsid                  = *props.lpClsid;
    g_psz_friendly_name      = props.lpFriendlyName;
    g_psz_launch_history_key = props.lpLaunchHistoryKey;
    if (NULL == g_psz_launch_history_key)
    {
        g_psz_launch_history_key = WBX_DEFAULT_LIST_KEY;
    }
    g_f_registration_loaded = TRUE;
    return TRUE;
}

static int wbx_call_client_w(LPWSTR pszCmdLine, const STARTUPINFOW *psi)
{
    HINSTANCE hInstance;
    int       nShowCmd;

    hInstance = GetModuleHandleW(NULL);
    nShowCmd  = wbx_show_window_from_startup_w(psi);
    return g_pfn_winmainexw(hInstance, NULL, pszCmdLine, nShowCmd, psi);
}

static int wbx_call_client_a(LPSTR pszCmdLine, const STARTUPINFOA *psi)
{
    HINSTANCE hInstance;
    int       nShowCmd;

    hInstance = GetModuleHandleW(NULL);
    nShowCmd  = wbx_show_window_from_startup_a(psi);
    return g_pfn_winmainexa(hInstance, NULL, pszCmdLine, nShowCmd, psi);
}

static int wbx_call_client_from_wide_startup(LPWSTR pszCmdLine, const STARTUPINFOW *psi)
{
    STARTUPINFOA siA;
    LPSTR        pszCmdLineA;

    if (g_f_use_wide_callback)
    {
        return wbx_call_client_w(pszCmdLine, psi);
    }
    wbx_startupinfo_w_to_a(psi, &siA);
    pszCmdLineA = wbx_command_line_w_to_a(pszCmdLine);
    return wbx_call_client_a(pszCmdLineA, &siA);
}

static void wbx_record_exe(LPCWSTR pszPath)
{
    static const WCHAR szOne[] = L"1";
    HKEY               hKey;
    LONG               lr;

    lr = RegCreateKeyExW(HKEY_CURRENT_USER, g_psz_launch_history_key, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    if (ERROR_SUCCESS == lr)
    {
        RegSetValueExW(hKey, pszPath, 0, REG_SZ, (const BYTE *)szOne, (DWORD)sizeof(szOne));
        RegCloseKey(hKey);
    }
}

static BOOL wbx_is_exe_registered(LPCWSTR pszPath)
{
    HKEY hKey;
    LONG lrOpen;
    LONG lrQuery;
    BOOL fFound;

    fFound = FALSE;
    lrOpen = RegOpenKeyExW(HKEY_CURRENT_USER, g_psz_launch_history_key, 0, KEY_QUERY_VALUE, &hKey);
    if (ERROR_SUCCESS == lrOpen)
    {
        lrQuery = RegQueryValueExW(hKey, pszPath, NULL, NULL, NULL, NULL);
        fFound  = (ERROR_SUCCESS == lrQuery);
        RegCloseKey(hKey);
    }
    return fFound;
}

static void wbx_fill_startupinfo(const CommandObject *pObj, STARTUPINFOW *psi, BOOL fForwardChild)
{
    SecureZeroMemory(psi, sizeof((*psi)));
    psi->cb = (DWORD)sizeof((*psi));
    if (pObj->show_set)
    {
        psi->dwFlags |= STARTF_USESHOWWINDOW;
        psi->wShowWindow = (WORD)pObj->show_window;
    }
    if (fForwardChild && pObj->pos_set)
    {
        psi->dwFlags |= STARTF_USEPOSITION;
        psi->dwX = (DWORD)pObj->pos.x;
        psi->dwY = (DWORD)pObj->pos.y;
    }
}

#define EC_OBJ(p)  ((CommandObject *)((BYTE *)(p) - offsetof(CommandObject, vtbl_ec)))
#define OWS_OBJ(p) ((CommandObject *)((BYTE *)(p) - offsetof(CommandObject, vtbl_ows)))

static HRESULT wbx_cmd_QI(CommandObject *pObj, REFIID riid, void **ppv)
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

static ULONG wbx_cmd_AddRef(CommandObject *pObj)
{
    return (ULONG)InterlockedIncrement(&pObj->ref);
}

static ULONG wbx_cmd_Release(CommandObject *pObj)
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

static HRESULT STDMETHODCALLTYPE ec_QI(IExecuteCommand *pThis, REFIID riid, void **ppv)
{
    return wbx_cmd_QI(EC_OBJ(pThis), riid, ppv);
}

static ULONG STDMETHODCALLTYPE ec_AddRef(IExecuteCommand *pThis)
{
    return wbx_cmd_AddRef(EC_OBJ(pThis));
}

static ULONG STDMETHODCALLTYPE ec_Release(IExecuteCommand *pThis)
{
    return wbx_cmd_Release(EC_OBJ(pThis));
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
        while ((0 != pszParams[cch]) && ((WBX_PARAMS_CCH - 1) > cch))
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
        wbx_fill_startupinfo(pObj, &si, FALSE);
        wbx_call_client_from_wide_startup(pObj->params, &si);
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

    fIsSelf = (CSTR_EQUAL == CompareStringOrdinal(pszPath, -1, g_my_path, -1, TRUE));
    if (fIsSelf)
    {
        CoTaskMemFree(pszPath);
        wbx_fill_startupinfo(pObj, &si, FALSE);
        wbx_call_client_from_wide_startup(pObj->params, &si);
        return S_OK;
    }

    fRegistered = wbx_is_exe_registered(pszPath);
    if (!fRegistered)
    {
        wbx_record_exe(pszPath);
    }

    pszWrite     = g_cmd_buf;
    (*pszWrite)  = L'"';
    pszWrite++;
    pszWrite     = wbx_strappend(pszWrite, pszPath);
    (*pszWrite)  = L'"';
    pszWrite++;
    fHasParams = (0 != pObj->params[0]);
    if (fHasParams)
    {
        (*pszWrite) = L' ';
        pszWrite++;
        pszWrite = wbx_strappend(pszWrite, pObj->params);
    }
    (*pszWrite) = 0;

    wbx_fill_startupinfo(pObj, &si, TRUE);
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
            SecureZeroMemory(&sei, sizeof(sei));
            sei.cbSize      = (DWORD)sizeof(sei);
            sei.fMask       = SEE_MASK_FLAG_NO_UI;
            sei.lpVerb      = L"runas";
            sei.lpFile      = pszPath;
            sei.lpDirectory = pszDir;
            sei.nShow       = SW_SHOWNORMAL;
            if (0 != pObj->params[0])
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
    ec_QI,          ec_AddRef,       ec_Release,    ec_SetKeyState, ec_SetParameters,
    ec_SetPosition, ec_SetShowWindow, ec_SetNoShowUI, ec_SetDirectory, ec_Execute};

static HRESULT STDMETHODCALLTYPE ows_QI(IObjectWithSelection *pThis, REFIID riid, void **ppv)
{
    return wbx_cmd_QI(OWS_OBJ(pThis), riid, ppv);
}

static ULONG STDMETHODCALLTYPE ows_AddRef(IObjectWithSelection *pThis)
{
    return wbx_cmd_AddRef(OWS_OBJ(pThis));
}

static ULONG STDMETHODCALLTYPE ows_Release(IObjectWithSelection *pThis)
{
    return wbx_cmd_Release(OWS_OBJ(pThis));
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
    return WBX_SVR_REFCOUNT;
}

static ULONG STDMETHODCALLTYPE cf_Release(IClassFactory *pThis)
{
    UNREFERENCED_PARAMETER(pThis);
    return WBX_SVR_REFCOUNT - 1;
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
    hr = wbx_cmd_QI(pObj, riid, ppv);
    wbx_cmd_Release(pObj);
    return hr;
}

static HRESULT STDMETHODCALLTYPE cf_LockServer(IClassFactory *pThis, BOOL fLock)
{
    UNREFERENCED_PARAMETER(pThis);
    UNREFERENCED_PARAMETER(fLock);
    return S_OK;
}

static const IClassFactoryVtbl g_cf_vtbl = {cf_QI, cf_AddRef, cf_Release, cf_CreateInstance, cf_LockServer};

static void wbx_clsid_str(WCHAR rgClsid[WBX_GUID_CCH])
{
    int cch;

    cch = StringFromGUID2(&g_clsid, rgClsid, WBX_GUID_CCH);
    if (0 == cch)
    {
        rgClsid[0] = 0;
    }
}

static LONG wbx_reg_set_sz(HKEY hParent, LPCWSTR pszSubKey, LPCWSTR pszName, LPCWSTR pszValue)
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
    cch     = wbx_strlen(pszValue);
    cbValue = (DWORD)(((size_t)cch + 1u) * sizeof(WCHAR));
    lr      = RegSetValueExW(hKey, pszName, 0, REG_SZ, (const BYTE *)pszValue, cbValue);
    RegCloseKey(hKey);
    return lr;
}

static int wbx_register(void)
{
    WCHAR  rgClsid[WBX_GUID_CCH];
    WCHAR  szSub[WBX_SUBKEY_CCH];
    LPWSTR pszWrite;
    LONG   lr;

    wbx_clsid_str(rgClsid);

    pszWrite = wbx_strappend(szSub, WBX_CLSID_PREFIX);
    wbx_strcpy(pszWrite, rgClsid);
    lr = wbx_reg_set_sz(HKEY_CURRENT_USER, szSub, NULL, g_psz_friendly_name);
    if (ERROR_SUCCESS != lr)
    {
        return 1;
    }

    pszWrite = wbx_strappend(szSub, WBX_CLSID_PREFIX);
    pszWrite = wbx_strappend(pszWrite, rgClsid);
    wbx_strcpy(pszWrite, L"\\LocalServer32");
    lr = wbx_reg_set_sz(HKEY_CURRENT_USER, szSub, NULL, g_my_path);
    if (ERROR_SUCCESS != lr)
    {
        return 1;
    }

    lr = wbx_reg_set_sz(HKEY_CURRENT_USER, WBX_EXEFILE_CMD_KEY, NULL, L"\"%1\" %*");
    if (ERROR_SUCCESS != lr)
    {
        return 1;
    }
    lr = wbx_reg_set_sz(HKEY_CURRENT_USER, WBX_EXEFILE_CMD_KEY, L"DelegateExecute", rgClsid);
    if (ERROR_SUCCESS != lr)
    {
        return 1;
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return 0;
}

static int wbx_unregister(void)
{
    WCHAR  rgClsid[WBX_GUID_CCH];
    WCHAR  szSub[WBX_SUBKEY_CCH];
    LPWSTR pszWrite;
    HKEY   hKey;
    LONG   lrOpen;

    wbx_clsid_str(rgClsid);
    pszWrite = wbx_strappend(szSub, WBX_CLSID_PREFIX);
    wbx_strcpy(pszWrite, rgClsid);
    RegDeleteTreeW(HKEY_CURRENT_USER, szSub);

    lrOpen = RegOpenKeyExW(HKEY_CURRENT_USER, WBX_EXEFILE_CMD_KEY, 0, KEY_SET_VALUE, &hKey);
    if (ERROR_SUCCESS == lrOpen)
    {
        RegDeleteValueW(hKey, L"DelegateExecute");
        RegDeleteValueW(hKey, NULL);
        RegCloseKey(hKey);
    }
    RegDeleteKeyExW(HKEY_CURRENT_USER, WBX_EXEFILE_CMD_KEY, 0, 0);
    RegDeleteKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\exefile\\shell\\open", 0, 0);
    RegDeleteKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\exefile\\shell", 0, 0);
    RegDeleteKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\exefile", 0, 0);
    return 0;
}

static BOOL wbx_is_registered(void)
{
    WCHAR  rgClsid[WBX_GUID_CCH];
    WCHAR  szSub[WBX_SUBKEY_CCH];
    LPWSTR pszWrite;
    HKEY   hKey;
    LONG   lrOpen;

    wbx_clsid_str(rgClsid);
    pszWrite = wbx_strappend(szSub, WBX_CLSID_PREFIX);
    pszWrite = wbx_strappend(pszWrite, rgClsid);
    wbx_strcpy(pszWrite, L"\\LocalServer32");
    lrOpen = RegOpenKeyExW(HKEY_CURRENT_USER, szSub, 0, KEY_READ, &hKey);
    if (ERROR_SUCCESS != lrOpen)
    {
        return FALSE;
    }
    RegCloseKey(hKey);
    return TRUE;
}

static int wbx_run_com_server(void)
{
    MSG     msg;
    HRESULT hrInit;
    HRESULT hrReg;
    DWORD   dwCookie;

    g_f_com_server = TRUE;
    hrInit         = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hrInit))
    {
        return 1;
    }
    dwCookie = 0;
    hrReg    = CoRegisterClassObject(&g_clsid, (IUnknown *)&g_class_factory, CLSCTX_LOCAL_SERVER,
                                      REGCLS_MULTIPLEUSE, &dwCookie);
    if (FAILED(hrReg))
    {
        CoUninitialize();
        return 2;
    }

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

int __cdecl WinBaseXRunWide(WBX_PFN_WINMAINEXW pfnWinMainEx)
{
    STARTUPINFOW si;
    LPCWSTR      pszCmd;
    BOOL         fUnregister;
    BOOL         fEmbedding;

    g_f_use_wide_callback = TRUE;
    g_pfn_winmainexw      = pfnWinMainEx;

    if (!wbx_load_registration())
    {
        return 3;
    }
    GetModuleFileNameW(NULL, g_my_path, MAX_PATH);

    pszCmd      = GetCommandLineW();
    fUnregister = wbx_isarg(pszCmd, L"/unregister");
    fEmbedding  = wbx_isarg(pszCmd, L"-Embedding") || wbx_isarg(pszCmd, L"/Embedding");
    if (fUnregister)
    {
        return wbx_unregister();
    }
    if (fEmbedding)
    {
        return wbx_run_com_server();
    }
    if (!wbx_is_registered())
    {
        wbx_register();
    }
    GetStartupInfoW(&si);
    return wbx_call_client_w(wbx_winmain_command_line_w(), &si);
}

int __cdecl WinBaseXRunAnsi(WBX_PFN_WINMAINEXA pfnWinMainEx)
{
    STARTUPINFOA si;
    LPCWSTR      pszCmd;
    BOOL         fUnregister;
    BOOL         fEmbedding;

    g_f_use_wide_callback = FALSE;
    g_pfn_winmainexa      = pfnWinMainEx;

    if (!wbx_load_registration())
    {
        return 3;
    }
    GetModuleFileNameW(NULL, g_my_path, MAX_PATH);

    pszCmd      = GetCommandLineW();
    fUnregister = wbx_isarg(pszCmd, L"/unregister");
    fEmbedding  = wbx_isarg(pszCmd, L"-Embedding") || wbx_isarg(pszCmd, L"/Embedding");
    if (fUnregister)
    {
        return wbx_unregister();
    }
    if (fEmbedding)
    {
        return wbx_run_com_server();
    }
    if (!wbx_is_registered())
    {
        wbx_register();
    }
    GetStartupInfoA(&si);
    return wbx_call_client_a(wbx_winmain_command_line_a(), &si);
}
