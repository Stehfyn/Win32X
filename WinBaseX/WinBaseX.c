/*
 * WinBaseX.c -- no-CRT static library for WinMainEx/wWinMainEx startup and
 * exefile DelegateExecute launch forwarding.
 *
 * The dual W/A client-facing layer (command-line tokenizer, show-state mapper, client call, and
 * the WinBaseXRunWide/WinBaseXRunAnsi entry bodies) is generated from a single charset-agnostic
 * template, WinBaseXText.inl, included once per character set. The DelegateExecute COM server is
 * wide by ABI and stays WCHAR. All mutable/config state lives in one heap WBX_STATE held in
 * apartment-local (TLS) storage -- no bare globals.
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

typedef struct ClassFactory
{
    const IClassFactoryVtbl *vtbl;
} ClassFactory;

/* All mutable/config state. One heap instance per process, reached via wbx_state() (TLS). */
typedef struct WBX_STATE
{
    /* client registration (loaded once) */
    GUID               clsid;
    LPCWSTR            pszFriendlyName;
    LPCWSTR            pszLaunchHistoryKey;
    ClassFactory       factory;            /* embedded singleton -> no global factory */
    WBX_PFN_WINMAINEXA pfnWinMainExA;      /* exactly one of A/W set per process */
    WBX_PFN_WINMAINEXW pfnWinMainExW;
    /* COM-server runtime */
    volatile LONG      nObjectCount;
    BOOL               fRegistrationLoaded;
    BOOL               fComServer;
    BOOL               fUseWideCallback;
    /* process identity + forward scratch */
    WCHAR              szMyPath[MAX_PATH];
    WCHAR              szCmdBuf[WBX_CMD_CCH];   /* wide forward command line (COM path) */
    CHAR               szCmdBufA[WBX_CMD_CCH];  /* ANSI client command line (W->A bridge) */
} WBX_STATE;

typedef struct CommandObject
{
    const IExecuteCommandVtbl      *vtbl_ec;
    const IObjectWithSelectionVtbl *vtbl_ows;
    IShellItemArray                *selection;
    WBX_STATE                      *pState;
    POINT                           pos;
    LONG                            ref;
    int                             show_window;
    BOOL                            show_set;
    BOOL                            pos_set;
    WCHAR                           params[WBX_PARAMS_CCH];
    WCHAR                           directory[MAX_PATH];
} CommandObject;

static DWORD s_dwTlsState = TLS_OUT_OF_INDEXES;     /* the one irreducible root */

/* ec_Execute (wide COM section) forwards a self/no-selection activation through here; the body is
   defined after the generated W/A call-client helpers exist. */
static int wbx_call_client_from_wide_startup(LPWSTR pszCmdLine, const STARTUPINFOW *psi);

/* name/identifier paste used by the WinBaseXText.inl template: WBXCAT(STARTUPINFO, W) -> STARTUPINFOW,
   WBXNAME(wbx_call_client) -> wbx_call_client<WBXSUF>. */
#define WBXCAT2(a, b)  a##b
#define WBXCAT(a, b)   WBXCAT2(a, b)
#define WBXNAME(base)  WBXCAT(base, WBXSUF)

#pragma function(memcmp)
_Check_return_
int __cdecl memcmp(_In_reads_bytes_(cb) const void *pvA, _In_reads_bytes_(cb) const void *pvB, _In_ size_t cb)
{
    const BYTE *pbA;
    const BYTE *pbB;

    pbA = (const BYTE *)pvA;
    pbB = (const BYTE *)pvB;
    while (cb)
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

static WBX_STATE *wbx_state(void)
{
    return (WBX_STATE *)TlsGetValue(s_dwTlsState);
}

BOOL WINAPI IsWinBaseXComServer(void)
{
    WBX_STATE *pState;

    pState = wbx_state();
    if (NULL == pState)
    {
        return FALSE;
    }
    return pState->fComServer;
}

/* Whole-token, case-insensitive search across the (always wide) command line. */
static BOOL wbx_isarg(LPCWSTR pszCmd, LPCWSTR pszTarget)
{
    LPCWSTR p;
    int     cchTarget;
    WCHAR   chPre;
    WCHAR   chPost;
    BOOL    fTokenStart;
    BOOL    fMatch;
    BOOL    fTokenEnd;

    cchTarget = lstrlenW(pszTarget);
    for (p = pszCmd; (*p); p++)
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
        fTokenEnd = (!chPost) || (L' ' == chPost) || (L'"' == chPost);
        if (fTokenEnd)
        {
            return TRUE;
        }
    }
    return FALSE;
}

static void wbx_record_exe(LPCWSTR pszPath)
{
    static const WCHAR szOne[] = L"1";
    WBX_STATE         *pState;
    HKEY               hKey;
    LONG               lr;

    pState = wbx_state();
    lr     = RegCreateKeyExW(HKEY_CURRENT_USER, pState->pszLaunchHistoryKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    if (ERROR_SUCCESS == lr)
    {
        RegSetValueExW(hKey, pszPath, 0, REG_SZ, (const BYTE *)szOne, (DWORD)sizeof(szOne));
        RegCloseKey(hKey);
    }
}

static BOOL wbx_is_exe_registered(LPCWSTR pszPath)
{
    WBX_STATE *pState;
    HKEY       hKey;
    LONG       lrOpen;
    LONG       lrQuery;
    BOOL       fFound;

    pState = wbx_state();
    fFound = FALSE;
    lrOpen = RegOpenKeyExW(HKEY_CURRENT_USER, pState->pszLaunchHistoryKey, 0, KEY_QUERY_VALUE, &hKey);
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
        InterlockedDecrement(&pObj->pState->nObjectCount);
        HeapFree(GetProcessHeap(), 0, pObj);
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

    pObj = EC_OBJ(pThis);
    if (NULL != pszParams)
    {
        (void)lstrcpynW(pObj->params, pszParams, WBX_PARAMS_CCH);
    }
    else
    {
        pObj->params[0] = 0;
    }
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

    pObj = EC_OBJ(pThis);
    if (NULL != pszDir)
    {
        (void)lstrcpynW(pObj->directory, pszDir, MAX_PATH);
    }
    else
    {
        pObj->directory[0] = 0;
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ec_Execute(IExecuteCommand *pThis)
{
    CommandObject      *pObj;
    WBX_STATE          *pState;
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
    int                 cchPath;
    int                 cchParams;
    int                 cchNeeded;
    BOOL                fIsSelf;
    BOOL                fRegistered;
    BOOL                fHasParams;
    BOOL                fCreated;

    pObj   = EC_OBJ(pThis);
    pState = pObj->pState;

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

    fIsSelf = (CSTR_EQUAL == CompareStringOrdinal(pszPath, -1, pState->szMyPath, -1, TRUE));
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

    /* Build "<path>" [params] into szCmdBuf, bounded so an over-long path/params declines the
       forward rather than overflowing into adjacent state. */
    cchPath    = lstrlenW(pszPath);
    fHasParams = !!pObj->params[0];
    cchParams  = 0;
    if (fHasParams)
    {
        cchParams = lstrlenW(pObj->params);
    }
    cchNeeded = cchPath + 3;                 /* two quotes + nul */
    if (fHasParams)
    {
        cchNeeded += cchParams + 1;          /* space + params */
    }
    if (WBX_CMD_CCH < cchNeeded)
    {
        CoTaskMemFree(pszPath);
        return E_FAIL;
    }

    pszWrite    = pState->szCmdBuf;
    (*pszWrite) = L'"';
    pszWrite++;
    (void)lstrcpynW(pszWrite, pszPath, cchPath + 1);
    pszWrite   += cchPath;
    (*pszWrite) = L'"';
    pszWrite++;
    if (fHasParams)
    {
        (*pszWrite) = L' ';
        pszWrite++;
        (void)lstrcpynW(pszWrite, pObj->params, cchParams + 1);
        pszWrite += cchParams;
    }
    (*pszWrite) = 0;

    wbx_fill_startupinfo(pObj, &si, TRUE);
    if (fRegistered)
    {
        si.dwFlags |= STARTF_FORCEOFFFEEDBACK;
    }

    pszDir = NULL;
    if (pObj->directory[0])
    {
        pszDir = pObj->directory;
    }

    fCreated = CreateProcessW(NULL, pState->szCmdBuf, NULL, NULL, FALSE, 0, NULL, pszDir, &si, &pi);
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

static const IExecuteCommandVtbl s_ec_vtbl = {
    ec_QI,          ec_AddRef,        ec_Release,     ec_SetKeyState,  ec_SetParameters,
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

static const IObjectWithSelectionVtbl s_ows_vtbl = {
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
    pObj->vtbl_ec  = &s_ec_vtbl;
    pObj->vtbl_ows = &s_ows_vtbl;
    pObj->pState   = wbx_state();
    pObj->ref      = 1;
    InterlockedIncrement(&pObj->pState->nObjectCount);
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

static const IClassFactoryVtbl s_cf_vtbl = {cf_QI, cf_AddRef, cf_Release, cf_CreateInstance, cf_LockServer};

static void wbx_clsid_str(WCHAR rgClsid[WBX_GUID_CCH])
{
    int cch;

    cch = StringFromGUID2(&wbx_state()->clsid, rgClsid, WBX_GUID_CCH);
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
    cch     = lstrlenW(pszValue);
    cbValue = (DWORD)(((size_t)cch + 1u) * sizeof(WCHAR));
    lr      = RegSetValueExW(hKey, pszName, 0, REG_SZ, (const BYTE *)pszValue, cbValue);
    RegCloseKey(hKey);
    return lr;
}

static int wbx_register(void)
{
    WBX_STATE *pState;
    WCHAR      rgClsid[WBX_GUID_CCH];
    WCHAR      szSub[WBX_SUBKEY_CCH];
    LONG       lr;

    pState = wbx_state();
    wbx_clsid_str(rgClsid);

    lstrcpyW(szSub, WBX_CLSID_PREFIX);
    lstrcatW(szSub, rgClsid);
    lr = wbx_reg_set_sz(HKEY_CURRENT_USER, szSub, NULL, pState->pszFriendlyName);
    if (ERROR_SUCCESS != lr)
    {
        return 1;
    }

    lstrcpyW(szSub, WBX_CLSID_PREFIX);
    lstrcatW(szSub, rgClsid);
    lstrcatW(szSub, L"\\LocalServer32");
    lr = wbx_reg_set_sz(HKEY_CURRENT_USER, szSub, NULL, pState->szMyPath);
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
    WCHAR rgClsid[WBX_GUID_CCH];
    WCHAR szSub[WBX_SUBKEY_CCH];
    HKEY  hKey;
    LONG  lrOpen;

    wbx_clsid_str(rgClsid);
    lstrcpyW(szSub, WBX_CLSID_PREFIX);
    lstrcatW(szSub, rgClsid);
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
    WCHAR rgClsid[WBX_GUID_CCH];
    WCHAR szSub[WBX_SUBKEY_CCH];
    HKEY  hKey;
    LONG  lrOpen;

    wbx_clsid_str(rgClsid);
    lstrcpyW(szSub, WBX_CLSID_PREFIX);
    lstrcatW(szSub, rgClsid);
    lstrcatW(szSub, L"\\LocalServer32");
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
    WBX_STATE *pState;
    MSG        msg;
    HRESULT    hrInit;
    HRESULT    hrReg;
    DWORD      dwCookie;

    pState             = wbx_state();
    pState->fComServer = TRUE;
    hrInit             = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hrInit))
    {
        return 1;
    }
    dwCookie = 0;
    hrReg    = CoRegisterClassObject(&pState->clsid, (IUnknown *)&pState->factory, CLSCTX_LOCAL_SERVER,
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

    if (dwCookie)
    {
        CoRevokeClassObject(dwCookie);
    }
    CoUninitialize();
    return 0;
}

static BOOL wbx_load_registration(void)
{
    WBX_STATE                        *pState;
    WINBASEX_REGISTRATION_PROPERTIESW props;
    BOOL                              fGotProps;
    BOOL                              fValidCb;
    BOOL                              fValidFields;

    pState = wbx_state();
    if (pState->fRegistrationLoaded)
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

    pState->clsid               = *props.lpClsid;
    pState->pszFriendlyName     = props.lpFriendlyName;
    pState->pszLaunchHistoryKey = props.lpLaunchHistoryKey;
    if (NULL == pState->pszLaunchHistoryKey)
    {
        pState->pszLaunchHistoryKey = WBX_DEFAULT_LIST_KEY;
    }
    pState->fRegistrationLoaded = TRUE;
    return TRUE;
}

/* Allocate the apartment-local state once, at the very top of the entry body. */
static BOOL wbx_state_init(void)
{
    WBX_STATE *pState;

    s_dwTlsState = TlsAlloc();
    if (TLS_OUT_OF_INDEXES == s_dwTlsState)
    {
        return FALSE;
    }
    pState = (WBX_STATE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*pState));
    if (NULL == pState)
    {
        return FALSE;
    }
    TlsSetValue(s_dwTlsState, pState);
    pState->factory.vtbl = &s_cf_vtbl;
    return TRUE;
}

/* Shared mode dispatch (charset-independent). Sets (*pfProceed) when the caller should go on to
   invoke the client directly; otherwise returns the handled exit code. */
static int wbx_run_common(BOOL *pfProceed)
{
    WBX_STATE *pState;
    LPCWSTR    pszCmd;
    BOOL       fUnregister;
    BOOL       fEmbedding;

    (*pfProceed) = FALSE;
    pState       = wbx_state();

    if (!wbx_load_registration())
    {
        return 3;
    }
    GetModuleFileNameW(NULL, pState->szMyPath, MAX_PATH);

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
    (*pfProceed) = TRUE;
    return 0;
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
    WBX_STATE *pState;
    int        cch;

    pState               = wbx_state();
    pState->szCmdBufA[0] = 0;
    cch = WideCharToMultiByte(CP_ACP, 0, pszCmdLine, -1, pState->szCmdBufA, WBX_CMD_CCH, NULL, NULL);
    if (0 == cch)
    {
        pState->szCmdBufA[0] = 0;
    }
    return pState->szCmdBufA;
}

/* --- generate the wide (W) client-facing layer --- */
#define WBXSUF          W
#define WBXSTR          LPWSTR
#define WBXTEXT(x)      L##x
#define WBX_STARTUPINFO STARTUPINFOW
#define WBX_GETCMDLINE  GetCommandLineW
#define WBX_GETSTARTUP  GetStartupInfoW
#define WBX_RUN         WinBaseXRunWide
#define WBX_USE_WIDE    TRUE
#include "WinBaseXText.inl"
#undef WBXSUF
#undef WBXSTR
#undef WBXTEXT
#undef WBX_STARTUPINFO
#undef WBX_GETCMDLINE
#undef WBX_GETSTARTUP
#undef WBX_RUN
#undef WBX_USE_WIDE

/* --- generate the ANSI (A) client-facing layer --- */
#define WBXSUF          A
#define WBXSTR          LPSTR
#define WBXTEXT(x)      x
#define WBX_STARTUPINFO STARTUPINFOA
#define WBX_GETCMDLINE  GetCommandLineA
#define WBX_GETSTARTUP  GetStartupInfoA
#define WBX_RUN         WinBaseXRunAnsi
#define WBX_USE_WIDE    FALSE
#include "WinBaseXText.inl"
#undef WBXSUF
#undef WBXSTR
#undef WBXTEXT
#undef WBX_STARTUPINFO
#undef WBX_GETCMDLINE
#undef WBX_GETSTARTUP
#undef WBX_RUN
#undef WBX_USE_WIDE

/* COM (always wide) self/no-selection forward: call the wide client, or bridge to the ANSI one. */
static int wbx_call_client_from_wide_startup(LPWSTR pszCmdLine, const STARTUPINFOW *psi)
{
    STARTUPINFOA siA;
    LPSTR        pszCmdLineA;

    if (wbx_state()->fUseWideCallback)
    {
        return wbx_call_clientW(pszCmdLine, psi);
    }
    wbx_startupinfo_w_to_a(psi, &siA);
    pszCmdLineA = wbx_command_line_w_to_a(pszCmdLine);
    return wbx_call_clientA(pszCmdLineA, &siA);
}
