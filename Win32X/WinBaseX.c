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

/* Every non-kernel32 system DLL is delay-loaded by the linking application (via /DELAYLOAD link options
   -- not honored as an embedded #pragma comment(linker) directive, only as a real link option), so it
   leaves the load-time (auto-load) import directory and binds on first call instead. The import libs
   above still supply the call stubs; delay-load just moves the descriptor to the delay-import table.
   This keeps guaranteed/colliding funcs statically referenced (no per-function thunk) while reducing the
   PE's auto-load set to kernel32. The __delayLoadHelper2 the stubs call is our own CRT-free
   implementation (delayhlpx.c), not delayimp.lib -- the latter's delayhlp.obj drags _load_config_used,
   a CRT object incompatible with /NODEFAULTLIB. */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif
#define INITGUID

#include "WinBaseX.h"
#include "windefx.h"
#include "result.h"
#include "processenvx.h"
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

#define WBX_FRIENDLY_NAME    L"WinBaseX Launch Broker"
#define WBX_LIST_KEY         L"Software\\WinBaseX\\Launched"
#define WBX_CLSID_PREFIX     L"Software\\Classes\\CLSID\\"
#define WBX_EXEFILE_CMD_KEY  L"Software\\Classes\\exefile\\shell\\open\\command"
#define WBX_GUID_CCH         40
#define WBX_SUBKEY_CCH       256
#define WBX_PARAMS_CCH       1024
#define WBX_CMD_CCH          2048
#define WBX_SVR_REFCOUNT     2

DEFINE_GUID(IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_IClassFactory, 0x00000001, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_IExecuteCommand, 0x7F9185B0, 0xCB92, 0x43C5, 0x80, 0xA9, 0x92, 0x27, 0x7A, 0x4F, 0x7B, 0x54);
DEFINE_GUID(IID_IObjectWithSelection, 0x1C9CD5BB, 0x98E9, 0x4491, 0xA6, 0x0F, 0x31, 0xAA, 0xCC, 0x72, 0xB8, 0x3C);

/* The library's own coclass: the exefile DelegateExecute launch broker. Declared extern const CLSID
   in WinBaseX.h; this is its single definition site (Variant B). */
DEFINE_GUID(CLSID_WinBaseXLaunchBroker, 0xE5F1A9C2, 0x8B7D, 0x4E3F, 0xA1, 0x5C, 0x9D, 0x2E, 0x7B, 0x6F, 0x4A, 0x83);

typedef struct ClassFactory
{
    const IClassFactoryVtbl* vtbl;
} ClassFactory;

/* All mutable/per-process state, reached via GetState() (TLS). The broker's identity (CLSID, friendly
   name, launch-history key) is compile-time library constant, not state -- see the WBX_* macros and
   CLSID_WinBaseXLaunchBroker above. */
typedef struct WBX_STATE
{
    ClassFactory       factory;       /* embedded singleton -> no global factory */
    /* COM-server runtime */
    volatile LONG      nObjectCount;
    /* process identity + forward scratch */
    TCHAR              szMyPath[MAX_PATH];    /* this build's charset; GetModuleFileName fills it generic */
    TCHAR              szCmdBuf[WBX_CMD_CCH]; /* forward command line, in this build's charset */
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
    TCHAR                           params[WBX_PARAMS_CCH];
    TCHAR                           directory[MAX_PATH];
} CommandObject;

static DWORD s_dwTlsState = TLS_OUT_OF_INDEXES; /* the one irreducible root */

/* The IExecuteCommand ABI hands strings in fixed-wide LPCWSTR (so does IShellItem::GetDisplayName).
   Read one into a generic-text buffer: GetComTextW copies wide, GetComTextA narrows to ANSI --
   concrete on each side, never a "convert to TCHAR." GetComText resolves by this build's charset;
   both leaves always defined, like CreateWindowA/W. */
static void GetComTextW(LPWSTR pszDst, LPCWSTR pszSrc, int cchDst);
static void GetComTextA(LPSTR pszDst, LPCWSTR pszSrc, int cchDst);
#ifdef UNICODE
#define GetComText GetComTextW
#else
#define GetComText GetComTextA
#endif

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
        RETURN_VALUE_IF_NOT((*pbA) == (*pbB), (int)(*pbA) - (int)(*pbB));
        pbA++;
        pbB++;
        cb--;
    }
    return 0;
}

static WBX_STATE* GetState(void)
{
    return (WBX_STATE*)TlsGetValue(s_dwTlsState);
}

static void RecordExe(LPCWSTR pszPath)
{
    static const WCHAR szOne[] = L"1";
    HKEY               hKey;
    LONG               lr;

    lr = RegCreateKeyExW(HKEY_CURRENT_USER, WBX_LIST_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    if (ERROR_SUCCESS == lr)
    {
        RegSetValueExW(hKey, pszPath, 0, REG_SZ, (const BYTE*)szOne, (DWORD)sizeof(szOne));
        RegCloseKey(hKey);
    }
}

static BOOL IsExeRegistered(LPCWSTR pszPath)
{
    HKEY hKey;
    LONG lrOpen;
    LONG lrQuery;
    BOOL fFound;

    fFound = FALSE;
    lrOpen = RegOpenKeyExW(HKEY_CURRENT_USER, WBX_LIST_KEY, 0, KEY_QUERY_VALUE, &hKey);
    if (ERROR_SUCCESS == lrOpen)
    {
        lrQuery = RegQueryValueExW(hKey, pszPath, NULL, NULL, NULL, NULL);
        fFound  = (ERROR_SUCCESS == lrQuery);
        RegCloseKey(hKey);
    }
    return fFound;
}

static void FillStartupInfoW(const CommandObject* pObj, STARTUPINFOW* psi, BOOL fForwardChild)
{
    SecureZeroMemory(psi, sizeof((*psi)));
    psi->cb = (DWORD)sizeof((*psi));
    if (pObj->show_set)
    {
        SetFlag(psi->dwFlags, STARTF_USESHOWWINDOW);
        psi->wShowWindow = (WORD)pObj->show_window;
    }
    if (fForwardChild && pObj->pos_set)
    {
        SetFlag(psi->dwFlags, STARTF_USEPOSITION);
        psi->dwX = (DWORD)pObj->pos.x;
        psi->dwY = (DWORD)pObj->pos.y;
    }
}

static void FillStartupInfoA(const CommandObject* pObj, STARTUPINFOA* psi, BOOL fForwardChild)
{
    SecureZeroMemory(psi, sizeof((*psi)));
    psi->cb = (DWORD)sizeof((*psi));
    if (pObj->show_set)
    {
        SetFlag(psi->dwFlags, STARTF_USESHOWWINDOW);
        psi->wShowWindow = (WORD)pObj->show_window;
    }
    if (fForwardChild && pObj->pos_set)
    {
        SetFlag(psi->dwFlags, STARTF_USEPOSITION);
        psi->dwX = (DWORD)pObj->pos.x;
        psi->dwY = (DWORD)pObj->pos.y;
    }
}

#ifdef UNICODE
#define FillStartupInfo FillStartupInfoW
#else
#define FillStartupInfo FillStartupInfoA
#endif

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
        if (pObj->selection)
        {
            IShellItemArray_Release(pObj->selection);
        }
        /* Last broker object gone: the transient launch is done, so end RunComServer's pump. Release
           arrives on the STA pump thread, so PostQuitMessage targets the right queue. */
        if (0 == InterlockedDecrement(&pObj->pState->nObjectCount))
        {
            PostQuitMessage(0);
        }
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
    if (pszParams)
    {
        GetComText(pObj->params, pszParams, WBX_PARAMS_CCH);
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
        GetComText(pObj->directory, pszDir, MAX_PATH);
    }
    else
    {
        pObj->directory[0] = 0;
    }
    return S_OK;
}

/* Build "<path>" [params] into pState->szCmdBuf, bounded so an over-long path/params declines the
   launch rather than overflowing into adjacent state. */
static BOOL BuildLaunchCommand(WBX_STATE* pState, LPCTSTR pszPath, LPCTSTR pszParams)
{
    LPTSTR pszWrite;
    int    cchPath;
    int    cchParams;
    int    cchNeeded;
    BOOL   fHasParams;

    cchPath    = lstrlen(pszPath);
    fHasParams = !!(pszParams && pszParams[0]);
    cchParams  = 0;
    if (fHasParams)
    {
        cchParams = lstrlen(pszParams);
    }
    cchNeeded = cchPath + 3; /* two quotes + nul */
    if (fHasParams)
    {
        cchNeeded += cchParams + 1; /* space + params */
    }
    RETURN_FALSE_IF(WBX_CMD_CCH < cchNeeded);

    pszWrite    = pState->szCmdBuf;
    (*pszWrite) = '"';
    pszWrite++;
    (void)lstrcpyn(pszWrite, pszPath, cchPath + 1);
    pszWrite    += cchPath;
    (*pszWrite)  = '"';
    pszWrite++;
    if (fHasParams)
    {
        (*pszWrite) = ' ';
        pszWrite++;
        (void)lstrcpyn(pszWrite, pszParams, cchParams + 1);
        pszWrite += cchParams;
    }
    (*pszWrite) = 0;
    return TRUE;
}

/* Re-launch this exe as a fresh, normal (non-embedded) process. CreateProcess on the image path
   bypasses the exefile DelegateExecute hook, so the spawned instance re-enters WinBaseXRun on the
   direct path and runs its own _tWinMainEx pump -- the broker never hosts the window itself. */
static HRESULT LaunchSelf(WBX_STATE* pState, const CommandObject* pObj)
{
    STARTUPINFO         si;
    PROCESS_INFORMATION pi;
    LPCTSTR             pszDir;
    BOOL                fCreated;

    RETURN_VALUE_IF_NOT(BuildLaunchCommand(pState, pState->szMyPath, pObj->params), E_FAIL);
    FillStartupInfo(pObj, &si, FALSE);
    SetFlag(si.dwFlags, STARTF_FORCEOFFFEEDBACK);

    pszDir = NULL;
    if (pObj->directory[0])
    {
        pszDir = pObj->directory;
    }
    fCreated = CreateProcess(NULL, pState->szCmdBuf, NULL, NULL, FALSE, 0, NULL, pszDir, &si, &pi);
    RETURN_VALUE_IF_NOT(fCreated, HRESULT_FROM_WIN32(GetLastError()));
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ExecuteCommand_Execute(IExecuteCommand* pThis)
{
    CommandObject*      pObj;
    WBX_STATE*          pState;
    IShellItem*         pItem;
    LPWSTR              pszPath;
    LPCTSTR             pszDir;
    TCHAR               szPath[MAX_PATH];
    STARTUPINFO         si;
    PROCESS_INFORMATION pi;
    SHELLEXECUTEINFO    sei;
    HRESULT             hr;
    HRESULT             rc;
    DWORD               dwErr;
    BOOL                fIsSelf;
    BOOL                fRegistered;

    pObj   = EC_OBJ(pThis);
    pState = pObj->pState;

    if (!pObj->selection)
    {
        return LaunchSelf(pState, pObj);
    }

    hr = IShellItemArray_GetItemAt(pObj->selection, 0, &pItem);
    RETURN_VALUE_IF(FAILED(hr), E_FAIL);
    pszPath = NULL;
    hr      = IShellItem_GetDisplayName(pItem, SIGDN_FILESYSPATH, &pszPath);
    IShellItem_Release(pItem);
    RETURN_VALUE_IF(FAILED(hr), hr);
    RETURN_VALUE_IF_NULL(pszPath, hr);

    /* IShellItem hands back a wide display name; narrow it to this build's charset once, then work
       generic from here. */
    GetComText(szPath, pszPath, MAX_PATH);

    fIsSelf = (0 == lstrcmpi(szPath, pState->szMyPath));
    if (fIsSelf)
    {
        CoTaskMemFree(pszPath);
        return LaunchSelf(pState, pObj);
    }

    fRegistered = IsExeRegistered(pszPath);
    if (!fRegistered)
    {
        RecordExe(pszPath);
    }
    CoTaskMemFree(pszPath);

    RETURN_VALUE_IF_NOT(BuildLaunchCommand(pState, szPath, pObj->params), E_FAIL);

    FillStartupInfo(pObj, &si, TRUE);
    if (fRegistered)
    {
        SetFlag(si.dwFlags, STARTF_FORCEOFFFEEDBACK);
    }

    pszDir = NULL;
    if (pObj->directory[0])
    {
        pszDir = pObj->directory;
    }

    if (CreateProcess(NULL, pState->szCmdBuf, NULL, NULL, FALSE, 0, NULL, pszDir, &si, &pi))
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return S_OK;
    }

    dwErr = GetLastError();
    RETURN_VALUE_IF(ERROR_ELEVATION_REQUIRED != dwErr, HRESULT_FROM_WIN32(dwErr));

    SecureZeroMemory(&sei, sizeof(sei));
    sei.cbSize      = (DWORD)sizeof(sei);
    sei.fMask       = SEE_MASK_FLAG_NO_UI;
    sei.lpVerb      = TEXT("runas");
    sei.lpFile      = szPath;
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
    rc = ShellExecuteEx(&sei) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
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
    if (pObj->selection)
    {
        IShellItemArray_Release(pObj->selection);
    }
    pObj->selection = psia;
    if (psia)
    {
        IShellItemArray_AddRef(psia);
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ObjectWithSelection_GetSelection(IObjectWithSelection* pThis, REFIID riid, void** ppv)
{
    CommandObject* pObj;

    pObj = OWS_OBJ(pThis);
    if (!pObj->selection)
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

static HRESULT STDMETHODCALLTYPE ClassFactory_QueryInterface(IClassFactory* pThis, REFIID riid, void** ppv)
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
    RETURN_VALUE_IF_NOT(!pOuter, CLASS_E_NOAGGREGATION);
    pObj = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CommandObject));
    RETURN_VALUE_IF_NULL(pObj, E_OUTOFMEMORY);
    pObj->vtbl_ec  = &s_ExecuteCommandVtbl;
    pObj->vtbl_ows = &s_ObjectWithSelectionVtbl;
    pObj->pState   = GetState();
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

static void ClsidString(WCHAR rgClsid[WBX_GUID_CCH])
{
    int cch;

    cch = StringFromGUID2(&CLSID_WinBaseXLaunchBroker, rgClsid, WBX_GUID_CCH);
    if (0 == cch)
    {
        rgClsid[0] = 0;
    }
}

LONG RegSetStringW(HKEY hParent, LPCWSTR pszSubKey, LPCWSTR pszName, LPCWSTR pszValue)
{
    HKEY  hKey;
    LONG  lr;
    int   cch;
    DWORD cbValue;

    lr = RegCreateKeyExW(hParent, pszSubKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    RETURN_VALUE_IF_NOT(ERROR_SUCCESS == lr, lr);
    cch     = lstrlenW(pszValue);
    cbValue = (DWORD)(((size_t)cch + 1u) * sizeof(WCHAR));
    lr      = RegSetValueExW(hKey, pszName, 0, REG_SZ, (const BYTE*)pszValue, cbValue);
    RegCloseKey(hKey);
    return lr;
}

/* A variant: convert the ANSI subkey/name/value up to wide, then call the W variant. */
LONG RegSetStringA(HKEY hParent, LPCSTR pszSubKey, LPCSTR pszName, LPCSTR pszValue)
{
    WCHAR   szSubKey[WBX_SUBKEY_CCH];
    WCHAR   szName[WBX_SUBKEY_CCH];
    WCHAR   szValue[MAX_PATH];
    LPCWSTR pszNameW;

    szSubKey[0] = 0;
    szValue[0]  = 0;
    (void)SafeMultiByteToWideChar(pszSubKey, szSubKey, WBX_SUBKEY_CCH);
    (void)SafeMultiByteToWideChar(pszValue, szValue, MAX_PATH);
    pszNameW = NULL;
    if (pszName)
    {
        szName[0] = 0;
        pszNameW = SafeMultiByteToWideChar(pszName, szName, WBX_SUBKEY_CCH);
    }
    return RegSetStringW(hParent, szSubKey, pszNameW, szValue);
}

static int Register(void)
{
    WBX_STATE* pState;
    WCHAR      rgClsid[WBX_GUID_CCH];
    WCHAR      szSub[WBX_SUBKEY_CCH];
    LONG       lr;

    pState = GetState();
    ClsidString(rgClsid);

    lstrcpyW(szSub, WBX_CLSID_PREFIX);
    lstrcatW(szSub, rgClsid);
    lr = RegSetStringW(HKEY_CURRENT_USER, szSub, NULL, WBX_FRIENDLY_NAME);
    RETURN_VALUE_IF_NOT(ERROR_SUCCESS == lr, 1);

    lstrcpyW(szSub, WBX_CLSID_PREFIX);
    lstrcatW(szSub, rgClsid);
    lstrcatW(szSub, L"\\LocalServer32");
    lr = RegSetStringW(HKEY_CURRENT_USER, szSub, NULL, pState->szMyPath);
    RETURN_VALUE_IF_NOT(ERROR_SUCCESS == lr, 1);

    lr = RegSetStringW(HKEY_CURRENT_USER, WBX_EXEFILE_CMD_KEY, NULL, L"\"%1\" %*");
    RETURN_VALUE_IF_NOT(ERROR_SUCCESS == lr, 1);
    lr = RegSetStringW(HKEY_CURRENT_USER, WBX_EXEFILE_CMD_KEY, L"DelegateExecute", rgClsid);
    RETURN_VALUE_IF_NOT(ERROR_SUCCESS == lr, 1);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return 0;
}

static int Unregister(void)
{
    WCHAR rgClsid[WBX_GUID_CCH];
    WCHAR szSub[WBX_SUBKEY_CCH];
    HKEY  hKey;
    LONG  lrOpen;

    ClsidString(rgClsid);
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

static BOOL IsRegistered(void)
{
    WCHAR rgClsid[WBX_GUID_CCH];
    WCHAR szSub[WBX_SUBKEY_CCH];
    HKEY  hKey;
    LONG  lrOpen;

    ClsidString(rgClsid);
    lstrcpyW(szSub, WBX_CLSID_PREFIX);
    lstrcatW(szSub, rgClsid);
    lstrcatW(szSub, L"\\LocalServer32");
    lrOpen = RegOpenKeyExW(HKEY_CURRENT_USER, szSub, 0, KEY_READ, &hKey);
    RETURN_FALSE_IF_NOT(ERROR_SUCCESS == lrOpen);
    RegCloseKey(hKey);
    return TRUE;
}

static int RunComServer(void)
{
    WBX_STATE* pState;
    MSG        msg;
    HRESULT    hrInit;
    HRESULT    hrReg;
    DWORD      dwCookie;

    pState = GetState();
    hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    RETURN_VALUE_IF(FAILED(hrInit), 1);
    dwCookie = 0;
    hrReg    = CoRegisterClassObject(&CLSID_WinBaseXLaunchBroker,
                                     (IUnknown*)&pState->factory,
                                     CLSCTX_LOCAL_SERVER,
                                     REGCLS_MULTIPLEUSE,
                                     &dwCookie);
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
BOOL StateInit(void)
{
    WBX_STATE* pState;

    s_dwTlsState = TlsAlloc();
    RETURN_FALSE_IF(TLS_OUT_OF_INDEXES == s_dwTlsState);
    pState = (WBX_STATE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*pState));
    RETURN_FALSE_IF_NULL(pState);
    TlsSetValue(s_dwTlsState, pState);
    pState->factory.vtbl = &s_ClassFactoryVtbl;
    return TRUE;
}

/* Shared mode dispatch (charset-independent). Sets (*pfProceed) when the caller should go on to
   invoke the client directly; otherwise returns the handled exit code. */
int RunCommon(BOOL* pfProceed)
{
    WBX_STATE* pState;
    BOOL       fUnregister;
    BOOL       fEmbedding;

    (*pfProceed) = FALSE;
    pState       = GetState();

    GetModuleFileName(NULL, pState->szMyPath, MAX_PATH);

    fUnregister = !!GetCommandLineArgument(TEXT("/unregister"));
    fEmbedding  = !!GetCommandLineArgument(TEXT("-Embedding")) || !!GetCommandLineArgument(TEXT("/Embedding"));
    if (fUnregister)
    {
        return Unregister();
    }
    if (fEmbedding)
    {
        return RunComServer();
    }
    if (!IsRegistered())
    {
        Register();
    }
    (*pfProceed) = TRUE;
    return 0;
}

/* Bounded wide->ANSI conversion into pszBufA; returns the buffer, or NULL for a NULL/unconvertible
   source (NULL is the correct STARTUPINFO default for an absent lpDesktop/lpTitle). Public so clients
   sharing the W->A bridge can reuse it. */
_Success_(return != NULL)
LPSTR SafeWideCharToMultiByte(_In_opt_ LPCWSTR pszW, _Out_writes_(cchBufA) LPSTR pszBufA, _In_ int cchBufA)
{
    int cchWritten;

    RETURN_NULL_IF_NULL(pszW);
    cchWritten = WideCharToMultiByte(CP_ACP, 0, pszW, -1, pszBufA, cchBufA, NULL, NULL);
    RETURN_NULL_IF_ZERO(cchWritten);
    return pszBufA;
}

/* Bounded ANSI->wide conversion into pszBufW; returns the buffer, or NULL for a NULL/unconvertible
   source. Symmetric with SafeWideCharToMultiByte; public so clients can reuse it. */
_Success_(return != NULL)
LPWSTR SafeMultiByteToWideChar(_In_opt_ LPCSTR pszA, _Out_writes_(cchBufW) LPWSTR pszBufW, _In_ int cchBufW)
{
    int cchWritten;

    RETURN_NULL_IF_NULL(pszA);
    cchWritten = MultiByteToWideChar(CP_ACP, 0, pszA, -1, pszBufW, cchBufW);
    RETURN_NULL_IF_ZERO(cchWritten);
    return pszBufW;
}

static void GetComTextW(LPWSTR pszDst, LPCWSTR pszSrc, int cchDst)
{
    (void)lstrcpynW(pszDst, pszSrc, cchDst);
}

static void GetComTextA(LPSTR pszDst, LPCWSTR pszSrc, int cchDst)
{
    if (!SafeWideCharToMultiByte(pszSrc, pszDst, cchDst))
    {
        pszDst[0] = 0;
    }
}

