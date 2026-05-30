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
#define STARTF_HASSHELLDATA 0x00000400
#endif

#define WBX_DEFAULT_LIST_KEY L"Software\\WinMainEx\\Launched"
#define WBX_CLSID_PREFIX     L"Software\\Classes\\CLSID\\"
#define WBX_EXEFILE_CMD_KEY  L"Software\\Classes\\exefile\\shell\\open\\command"
#define WBX_GUID_CCH         40
#define WBX_SUBKEY_CCH       256
#define WBX_NAME_CCH         128
#define WBX_PARAMS_CCH       1024
#define WBX_CMD_CCH          2048
#define WBX_SVR_REFCOUNT     2

DEFINE_GUID(IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_IClassFactory, 0x00000001, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_IExecuteCommand, 0x7F9185B0, 0xCB92, 0x43C5, 0x80, 0xA9, 0x92, 0x27, 0x7A, 0x4F, 0x7B, 0x54);
DEFINE_GUID(IID_IObjectWithSelection, 0x1C9CD5BB, 0x98E9, 0x4491, 0xA6, 0x0F, 0x31, 0xAA, 0xCC, 0x72, 0xB8, 0x3C);

typedef struct ClassFactory
{
    const IClassFactoryVtbl* vtbl;
} ClassFactory;

/* All mutable/config state. One heap instance per process, reached via WbxState() (TLS). */
typedef struct WBX_STATE
{
    /* client registration (loaded once) */
    GUID               clsid;
    LPCWSTR            pszFriendlyName;
    LPCWSTR            pszLaunchHistoryKey;
    ClassFactory       factory;       /* embedded singleton -> no global factory */
    WBX_PFN_WINMAINEXA pfnWinMainExA; /* exactly one of A/W set per process */
    WBX_PFN_WINMAINEXW pfnWinMainExW;
    /* COM-server runtime */
    volatile LONG      nObjectCount;
    BOOL               fRegistrationLoaded;
    BOOL               fComServer;
    BOOL               fIsUnicode;
    /* process identity + forward scratch */
    WCHAR              szMyPath[MAX_PATH];
    WCHAR              szCmdBuf[WBX_CMD_CCH];  /* wide forward command line (COM path) */
    WCHAR              szFriendlyName[WBX_NAME_CCH];
    WCHAR              szHistoryKey[MAX_PATH];
    CHAR               szCmdBufA[WBX_CMD_CCH]; /* ANSI client command line (W->A bridge) */
    CHAR               szDesktopA[MAX_PATH];   /* ANSI STARTUPINFO.lpDesktop (W->A bridge) */
    CHAR               szTitleA[MAX_PATH];     /* ANSI STARTUPINFO.lpTitle   (W->A bridge) */
} WBX_STATE;

typedef struct CommandObject
{
    const IExecuteCommandVtbl*      vtbl_ec;
    const IObjectWithSelectionVtbl* vtbl_ows;
    IShellItemArray*                selection;
    WBX_STATE*                      pState;
    POINT                           pos;
    LONG                            ref;
    int                             show_window;
    BOOL                            show_set;
    BOOL                            pos_set;
    WCHAR                           params[WBX_PARAMS_CCH];
    WCHAR                           directory[MAX_PATH];
} CommandObject;

static DWORD s_dwTlsState = TLS_OUT_OF_INDEXES; /* the one irreducible root */

/* ExecuteCommand_Execute (wide COM section) forwards a self/no-selection activation through here; the body is
   defined after the generated W/A call-client helpers exist. */
static int   WbxCallClientFromStartup(LPWSTR pszCmdLine, const STARTUPINFOW* psi);

/* name/identifier paste used by the WinBaseXText.inl template: WBXCAT(STARTUPINFO, W) -> STARTUPINFOW,
   WBXNAME(WbxCallClient) -> WbxCallClient<WBXSUF>. */
#define WBXCAT2(a, b) a##b
#define WBXCAT(a, b)  WBXCAT2(a, b)
#define WBXNAME(base) WBXCAT(base, WBXSUF)

#pragma function(memcmp)
_Check_return_ int __cdecl memcmp(_In_reads_bytes_(cb) const void* pvA,
                                  _In_reads_bytes_(cb) const void* pvB,
                                  _In_ size_t                      cb)
{
    const BYTE* pbA;
    const BYTE* pbB;

    pbA = (const BYTE*)pvA;
    pbB = (const BYTE*)pvB;
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

static WBX_STATE* WbxState(void)
{
    return (WBX_STATE*)TlsGetValue(s_dwTlsState);
}

BOOL WINAPI IsWinBaseXComServer(void)
{
    WBX_STATE* pState;

    pState = WbxState();
    if (NULL == pState)
    {
        return FALSE;
    }
    return pState->fComServer;
}

/* Whole-token, case-insensitive search across the (always wide) command line. */
static BOOL WbxIsArg(LPCWSTR pszCmd, LPCWSTR pszTarget)
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

static void WbxRecordExe(LPCWSTR pszPath)
{
    static const WCHAR szOne[] = L"1";
    WBX_STATE*         pState;
    HKEY               hKey;
    LONG               lr;

    pState = WbxState();
    lr     = RegCreateKeyExW(HKEY_CURRENT_USER, pState->pszLaunchHistoryKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    if (ERROR_SUCCESS == lr)
    {
        RegSetValueExW(hKey, pszPath, 0, REG_SZ, (const BYTE*)szOne, (DWORD)sizeof(szOne));
        RegCloseKey(hKey);
    }
}

static BOOL WbxIsExeRegistered(LPCWSTR pszPath)
{
    WBX_STATE* pState;
    HKEY       hKey;
    LONG       lrOpen;
    LONG       lrQuery;
    BOOL       fFound;

    pState = WbxState();
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

static void WbxFillStartupInfo(const CommandObject* pObj, STARTUPINFOW* psi, BOOL fForwardChild)
{
    SecureZeroMemory(psi, sizeof((*psi)));
    psi->cb = (DWORD)sizeof((*psi));
    if (pObj->show_set)
    {
        psi->dwFlags     |= STARTF_USESHOWWINDOW;
        psi->wShowWindow  = (WORD)pObj->show_window;
    }
    if (fForwardChild && pObj->pos_set)
    {
        psi->dwFlags |= STARTF_USEPOSITION;
        psi->dwX      = (DWORD)pObj->pos.x;
        psi->dwY      = (DWORD)pObj->pos.y;
    }
}

#define EC_OBJ(p)  ((CommandObject*)((BYTE*)(p) - offsetof(CommandObject, vtbl_ec)))
#define OWS_OBJ(p) ((CommandObject*)((BYTE*)(p) - offsetof(CommandObject, vtbl_ows)))

static HRESULT Command_QueryInterface(CommandObject* pObj, REFIID riid, void** ppv)
{
    BOOL fUnknown;
    BOOL fExecute;
    BOOL fSelection;

    fUnknown   = IsEqualIID(riid, &IID_IUnknown);
    fExecute   = IsEqualIID(riid, &IID_IExecuteCommand);
    fSelection = IsEqualIID(riid, &IID_IObjectWithSelection);
    if (fUnknown || fExecute)
    {
        (*ppv) = (void*)&pObj->vtbl_ec;
    }
    else if (fSelection)
    {
        (*ppv) = (void*)&pObj->vtbl_ows;
    }
    else
    {
        (*ppv) = NULL;
        return E_NOINTERFACE;
    }
    InterlockedIncrement(&pObj->ref);
    return S_OK;
}

static ULONG Command_AddRef(CommandObject* pObj)
{
    return (ULONG)InterlockedIncrement(&pObj->ref);
}

static ULONG Command_Release(CommandObject* pObj)
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

static HRESULT STDMETHODCALLTYPE ExecuteCommand_QueryInterface(IExecuteCommand* pThis, REFIID riid, void** ppv)
{
    return Command_QueryInterface(EC_OBJ(pThis), riid, ppv);
}

static ULONG STDMETHODCALLTYPE ExecuteCommand_AddRef(IExecuteCommand* pThis)
{
    return Command_AddRef(EC_OBJ(pThis));
}

static ULONG STDMETHODCALLTYPE ExecuteCommand_Release(IExecuteCommand* pThis)
{
    return Command_Release(EC_OBJ(pThis));
}

static HRESULT STDMETHODCALLTYPE ExecuteCommand_SetKeyState(IExecuteCommand* pThis, DWORD dwKeyState)
{
    UNREFERENCED_PARAMETER(pThis);
    UNREFERENCED_PARAMETER(dwKeyState);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ExecuteCommand_SetParameters(IExecuteCommand* pThis, LPCWSTR pszParams)
{
    CommandObject* pObj;

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

static HRESULT STDMETHODCALLTYPE ExecuteCommand_SetPosition(IExecuteCommand* pThis, POINT pt)
{
    CommandObject* pObj;

    pObj          = EC_OBJ(pThis);
    pObj->pos     = pt;
    pObj->pos_set = TRUE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ExecuteCommand_SetShowWindow(IExecuteCommand* pThis, int nShow)
{
    CommandObject* pObj;

    pObj              = EC_OBJ(pThis);
    pObj->show_window = nShow;
    pObj->show_set    = TRUE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ExecuteCommand_SetNoShowUI(IExecuteCommand* pThis, BOOL fNoShowUI)
{
    UNREFERENCED_PARAMETER(pThis);
    UNREFERENCED_PARAMETER(fNoShowUI);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ExecuteCommand_SetDirectory(IExecuteCommand* pThis, LPCWSTR pszDir)
{
    CommandObject* pObj;

    pObj = EC_OBJ(pThis);
    if (pszDir)
    {
        (void)lstrcpynW(pObj->directory, pszDir, MAX_PATH);
    }
    else
    {
        pObj->directory[0] = 0;
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ExecuteCommand_Execute(IExecuteCommand* pThis)
{
    CommandObject*      pObj;
    WBX_STATE*          pState;
    IShellItem*         pItem;
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
        WbxFillStartupInfo(pObj, &si, FALSE);
        WbxCallClientFromStartup(pObj->params, &si);
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
        WbxFillStartupInfo(pObj, &si, FALSE);
        WbxCallClientFromStartup(pObj->params, &si);
        return S_OK;
    }

    fRegistered = WbxIsExeRegistered(pszPath);
    if (!fRegistered)
    {
        WbxRecordExe(pszPath);
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
    cchNeeded = cchPath + 3; /* two quotes + nul */
    if (fHasParams)
    {
        cchNeeded += cchParams + 1; /* space + params */
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
    pszWrite    += cchPath;
    (*pszWrite)  = L'"';
    pszWrite++;
    if (fHasParams)
    {
        (*pszWrite) = L' ';
        pszWrite++;
        (void)lstrcpynW(pszWrite, pObj->params, cchParams + 1);
        pszWrite += cchParams;
    }
    (*pszWrite) = 0;

    WbxFillStartupInfo(pObj, &si, TRUE);
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

static const IExecuteCommandVtbl s_ExecuteCommandVtbl = { ExecuteCommand_QueryInterface, ExecuteCommand_AddRef,
                                                          ExecuteCommand_Release,        ExecuteCommand_SetKeyState,
                                                          ExecuteCommand_SetParameters,  ExecuteCommand_SetPosition,
                                                          ExecuteCommand_SetShowWindow,  ExecuteCommand_SetNoShowUI,
                                                          ExecuteCommand_SetDirectory,   ExecuteCommand_Execute };

static HRESULT STDMETHODCALLTYPE ObjectWithSelection_QueryInterface(IObjectWithSelection* pThis,
                                                                    REFIID                riid,
                                                                    void**                ppv)
{
    return Command_QueryInterface(OWS_OBJ(pThis), riid, ppv);
}

static ULONG STDMETHODCALLTYPE ObjectWithSelection_AddRef(IObjectWithSelection* pThis)
{
    return Command_AddRef(OWS_OBJ(pThis));
}

static ULONG STDMETHODCALLTYPE ObjectWithSelection_Release(IObjectWithSelection* pThis)
{
    return Command_Release(OWS_OBJ(pThis));
}

static HRESULT STDMETHODCALLTYPE ObjectWithSelection_SetSelection(IObjectWithSelection* pThis, IShellItemArray* psia)
{
    CommandObject* pObj;

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

static HRESULT STDMETHODCALLTYPE ObjectWithSelection_GetSelection(IObjectWithSelection* pThis, REFIID riid, void** ppv)
{
    CommandObject* pObj;

    pObj = OWS_OBJ(pThis);
    if (NULL == pObj->selection)
    {
        (*ppv) = NULL;
        return E_FAIL;
    }
    return IShellItemArray_QueryInterface(pObj->selection, riid, ppv);
}

static const IObjectWithSelectionVtbl s_ObjectWithSelectionVtbl = { ObjectWithSelection_QueryInterface,
                                                                    ObjectWithSelection_AddRef,
                                                                    ObjectWithSelection_Release,
                                                                    ObjectWithSelection_SetSelection,
                                                                    ObjectWithSelection_GetSelection };

static HRESULT STDMETHODCALLTYPE      ClassFactory_QueryInterface(IClassFactory* pThis, REFIID riid, void** ppv)
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

static ULONG STDMETHODCALLTYPE ClassFactory_AddRef(IClassFactory* pThis)
{
    UNREFERENCED_PARAMETER(pThis);
    return WBX_SVR_REFCOUNT;
}

static ULONG STDMETHODCALLTYPE ClassFactory_Release(IClassFactory* pThis)
{
    UNREFERENCED_PARAMETER(pThis);
    return WBX_SVR_REFCOUNT - 1;
}

static HRESULT STDMETHODCALLTYPE ClassFactory_CreateInstance(IClassFactory* pThis,
                                                             IUnknown*      pOuter,
                                                             REFIID         riid,
                                                             void**         ppv)
{
    CommandObject* pObj;
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
    pObj->vtbl_ec  = &s_ExecuteCommandVtbl;
    pObj->vtbl_ows = &s_ObjectWithSelectionVtbl;
    pObj->pState   = WbxState();
    pObj->ref      = 1;
    InterlockedIncrement(&pObj->pState->nObjectCount);
    hr = Command_QueryInterface(pObj, riid, ppv);
    Command_Release(pObj);
    return hr;
}

static HRESULT STDMETHODCALLTYPE ClassFactory_LockServer(IClassFactory* pThis, BOOL fLock)
{
    UNREFERENCED_PARAMETER(pThis);
    UNREFERENCED_PARAMETER(fLock);
    return S_OK;
}

static const IClassFactoryVtbl s_ClassFactoryVtbl = { ClassFactory_QueryInterface,
                                                      ClassFactory_AddRef,
                                                      ClassFactory_Release,
                                                      ClassFactory_CreateInstance,
                                                      ClassFactory_LockServer };

static void                    WbxClsidString(WCHAR rgClsid[WBX_GUID_CCH])
{
    int cch;

    cch = StringFromGUID2(&WbxState()->clsid, rgClsid, WBX_GUID_CCH);
    if (0 == cch)
    {
        rgClsid[0] = 0;
    }
}

LONG WbxRegSetStringW(HKEY hParent, LPCWSTR pszSubKey, LPCWSTR pszName, LPCWSTR pszValue)
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
    lr      = RegSetValueExW(hKey, pszName, 0, REG_SZ, (const BYTE*)pszValue, cbValue);
    RegCloseKey(hKey);
    return lr;
}

/* A variant: convert the ANSI subkey/name/value up to wide, then call the W variant. */
LONG WbxRegSetStringA(HKEY hParent, LPCSTR pszSubKey, LPCSTR pszName, LPCSTR pszValue)
{
    WCHAR   szSubKey[WBX_SUBKEY_CCH];
    WCHAR   szName[WBX_SUBKEY_CCH];
    WCHAR   szValue[MAX_PATH];
    LPCWSTR pszNameW;

    szSubKey[0] = 0;
    szValue[0]  = 0;
    (void)MultiByteToWideChar(CP_ACP, 0, pszSubKey, -1, szSubKey, WBX_SUBKEY_CCH);
    (void)MultiByteToWideChar(CP_ACP, 0, pszValue, -1, szValue, MAX_PATH);
    pszNameW = NULL;
    if (NULL != pszName)
    {
        szName[0] = 0;
        (void)MultiByteToWideChar(CP_ACP, 0, pszName, -1, szName, WBX_SUBKEY_CCH);
        pszNameW = szName;
    }
    return WbxRegSetStringW(hParent, szSubKey, pszNameW, szValue);
}

static int WbxRegister(void)
{
    WBX_STATE* pState;
    WCHAR      rgClsid[WBX_GUID_CCH];
    WCHAR      szSub[WBX_SUBKEY_CCH];
    LONG       lr;

    pState = WbxState();
    WbxClsidString(rgClsid);

    lstrcpyW(szSub, WBX_CLSID_PREFIX);
    lstrcatW(szSub, rgClsid);
    lr = WbxRegSetStringW(HKEY_CURRENT_USER, szSub, NULL, pState->pszFriendlyName);
    if (ERROR_SUCCESS != lr)
    {
        return 1;
    }

    lstrcpyW(szSub, WBX_CLSID_PREFIX);
    lstrcatW(szSub, rgClsid);
    lstrcatW(szSub, L"\\LocalServer32");
    lr = WbxRegSetStringW(HKEY_CURRENT_USER, szSub, NULL, pState->szMyPath);
    if (ERROR_SUCCESS != lr)
    {
        return 1;
    }

    lr = WbxRegSetStringW(HKEY_CURRENT_USER, WBX_EXEFILE_CMD_KEY, NULL, L"\"%1\" %*");
    if (ERROR_SUCCESS != lr)
    {
        return 1;
    }
    lr = WbxRegSetStringW(HKEY_CURRENT_USER, WBX_EXEFILE_CMD_KEY, L"DelegateExecute", rgClsid);
    if (ERROR_SUCCESS != lr)
    {
        return 1;
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return 0;
}

static int WbxUnregister(void)
{
    WCHAR rgClsid[WBX_GUID_CCH];
    WCHAR szSub[WBX_SUBKEY_CCH];
    HKEY  hKey;
    LONG  lrOpen;

    WbxClsidString(rgClsid);
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

static BOOL WbxIsRegistered(void)
{
    WCHAR rgClsid[WBX_GUID_CCH];
    WCHAR szSub[WBX_SUBKEY_CCH];
    HKEY  hKey;
    LONG  lrOpen;

    WbxClsidString(rgClsid);
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

static int WbxRunComServer(void)
{
    WBX_STATE* pState;
    MSG        msg;
    HRESULT    hrInit;
    HRESULT    hrReg;
    DWORD      dwCookie;

    pState             = WbxState();
    pState->fComServer = TRUE;
    hrInit             = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hrInit))
    {
        return 1;
    }
    dwCookie = 0;
    hrReg    = CoRegisterClassObject(
        &pState->clsid, (IUnknown*)&pState->factory, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &dwCookie);
    if (FAILED(hrReg))
    {
        CoUninitialize();
        return 2;
    }

    while (0 < GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (dwCookie)
    {
        CoRevokeClassObject(dwCookie);
    }
    CoUninitialize();
    return 0;
}

/* Allocate the apartment-local state once, at the very top of the entry body. */
BOOL WbxStateInit(void)
{
    WBX_STATE* pState;

    s_dwTlsState = TlsAlloc();
    if (TLS_OUT_OF_INDEXES == s_dwTlsState)
    {
        return FALSE;
    }
    pState = (WBX_STATE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*pState));
    if (NULL == pState)
    {
        return FALSE;
    }
    TlsSetValue(s_dwTlsState, pState);
    pState->factory.vtbl = &s_ClassFactoryVtbl;
    return TRUE;
}

/* W variant: the worker -- validate the (wide) registration parameter and store it into state. */
BOOL WbxLoadRegistrationW(const WINBASEX_REGISTRATION_PROPERTIESW* pReg)
{
    WBX_STATE* pState;

    pState = WbxState();
    if (NULL == pReg)
    {
        return FALSE;
    }
    if ((sizeof((*pReg)) > pReg->cb) || (NULL == pReg->lpClsid) || (NULL == pReg->lpFriendlyName) ||
        (0 != pReg->dwFlags))
    {
        return FALSE;
    }
    pState->clsid           = *pReg->lpClsid;
    pState->pszFriendlyName = pReg->lpFriendlyName;
    if (NULL == pReg->lpLaunchHistoryKey)
    {
        pState->pszLaunchHistoryKey = WBX_DEFAULT_LIST_KEY;
    }
    else
    {
        pState->pszLaunchHistoryKey = pReg->lpLaunchHistoryKey;
    }
    pState->fRegistrationLoaded = TRUE;
    return TRUE;
}

/* A variant: convert the ANSI registration parameter up to wide, then call the W variant. */
BOOL WbxLoadRegistrationA(const WINBASEX_REGISTRATION_PROPERTIESA* pReg)
{
    WBX_STATE*                        pState;
    WINBASEX_REGISTRATION_PROPERTIESW regW;

    if (NULL == pReg)
    {
        return FALSE;
    }
    pState = WbxState();
    SecureZeroMemory(&regW, sizeof(regW));
    regW.cb      = (DWORD)sizeof(regW);
    regW.lpClsid = pReg->lpClsid;
    regW.dwFlags = pReg->dwFlags;
    if (NULL != pReg->lpFriendlyName)
    {
        (void)MultiByteToWideChar(CP_ACP, 0, pReg->lpFriendlyName, -1, pState->szFriendlyName, WBX_NAME_CCH);
        regW.lpFriendlyName = pState->szFriendlyName;
    }
    if (NULL != pReg->lpLaunchHistoryKey)
    {
        (void)MultiByteToWideChar(CP_ACP, 0, pReg->lpLaunchHistoryKey, -1, pState->szHistoryKey, MAX_PATH);
        regW.lpLaunchHistoryKey = pState->szHistoryKey;
    }
    return WbxLoadRegistrationW(&regW);
}

/* Shared mode dispatch (charset-independent). Sets (*pfProceed) when the caller should go on to
   invoke the client directly; otherwise returns the handled exit code. */
int WbxRunCommon(BOOL* pfProceed)
{
    WBX_STATE* pState;
    LPCWSTR    pszCmd;
    BOOL       fUnregister;
    BOOL       fEmbedding;

    (*pfProceed) = FALSE;
    pState       = WbxState();

    GetModuleFileName(NULL, pState->szMyPath, MAX_PATH);

    pszCmd      = GetCommandLine();
    fUnregister = WbxIsArg(pszCmd, L"/unregister");
    fEmbedding  = WbxIsArg(pszCmd, L"-Embedding") || WbxIsArg(pszCmd, L"/Embedding");
    if (fUnregister)
    {
        return WbxUnregister();
    }
    if (fEmbedding)
    {
        return WbxRunComServer();
    }
    if (!WbxIsRegistered())
    {
        WbxRegister();
    }
    (*pfProceed) = TRUE;
    return 0;
}

/* Convert a wide string to ANSI into pszBufA; returns the buffer, or NULL for a NULL/unconvertible
   source (NULL is the correct STARTUPINFO default for an absent lpDesktop/lpTitle). */
static LPSTR WbxWideToAnsi(LPCWSTR pszW, LPSTR pszBufA, int cchBufA)
{
    if (NULL == pszW)
    {
        return NULL;
    }
    if (0 == WideCharToMultiByte(CP_ACP, 0, pszW, -1, pszBufA, cchBufA, NULL, NULL))
    {
        return NULL;
    }
    return pszBufA;
}

/* STARTUPINFOA and STARTUPINFOW share an identical binary layout; the only members that genuinely
   differ are the LPWSTR strings, which must be charset-converted. Scalars/handles copy across, the
   CRT lpReserved2 block is charset-neutral, and lpReserved stays NULL (reserved). */
static void WbxStartupInfoWToA(const STARTUPINFOW* psiW, STARTUPINFOA* psiA)
{
    WBX_STATE* pState;

    pState = WbxState();
    SecureZeroMemory(psiA, sizeof((*psiA)));
    psiA->cb              = (DWORD)sizeof((*psiA));
    psiA->lpReserved      = NULL;
    psiA->lpDesktop       = WbxWideToAnsi(psiW->lpDesktop, pState->szDesktopA, ARRAYSIZE(pState->szDesktopA));
    psiA->lpTitle         = WbxWideToAnsi(psiW->lpTitle, pState->szTitleA, ARRAYSIZE(pState->szTitleA));
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

static LPSTR WbxCommandLineWToA(LPCWSTR pszCmdLine)
{
    WBX_STATE* pState;
    int        cch;

    pState               = WbxState();
    pState->szCmdBufA[0] = 0;
    cch                  = WideCharToMultiByte(CP_ACP, 0, pszCmdLine, -1, pState->szCmdBufA, WBX_CMD_CCH, NULL, NULL);
    if (0 == cch)
    {
        pState->szCmdBufA[0] = 0;
    }
    return pState->szCmdBufA;
}

/* COM self-activation is always wide: call the wide client directly, or convert the wide command
   line + STARTUPINFO down to ANSI for an ANSI client. */
static int WbxCallClientFromStartup(LPWSTR pszCmdLine, const STARTUPINFOW* psi)
{
    WBX_STATE*   pState;
    HINSTANCE    hInstance;
    int          nShowCmd;
    BOOL         fUseShow;
    STARTUPINFOA siA;
    LPSTR        pszCmdLineA;

    pState    = WbxState();
    hInstance = GetModuleHandleW(NULL);
    fUseShow  = !!(STARTF_USESHOWWINDOW & psi->dwFlags);
    nShowCmd  = SW_SHOWDEFAULT;
    if (fUseShow)
    {
        nShowCmd = (int)psi->wShowWindow;
    }
    if (pState->fIsUnicode)
    {
        return pState->pfnWinMainExW(hInstance, NULL, pszCmdLine, nShowCmd, psi);
    }
    WbxStartupInfoWToA(psi, &siA);
    pszCmdLineA = WbxCommandLineWToA(pszCmdLine);
    return pState->pfnWinMainExA(hInstance, NULL, pszCmdLineA, nShowCmd, &siA);
}

/* The generic-text WinBaseXRun (WinBaseXText.inl) stores the client it was handed; W or A is chosen
   by the charset that TU was compiled as. */
void WbxStoreClientW(WBX_PFN_WINMAINEXW pfnWinMainEx)
{
    WBX_STATE* pState;

    pState                = WbxState();
    pState->pfnWinMainExW = pfnWinMainEx;
    pState->fIsUnicode    = TRUE;
}

void WbxStoreClientA(WBX_PFN_WINMAINEXA pfnWinMainEx)
{
    WBX_STATE* pState;

    pState                = WbxState();
    pState->pfnWinMainExA = pfnWinMainEx;
    pState->fIsUnicode    = FALSE;
}