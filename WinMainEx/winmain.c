/*
 * winmain.c — WinMainEx, reworked as a lean COM server for the exefile
 * "open" verb via DelegateExecute.
 *
 * Why: the launch "spinny" (IDC_APPSTARTING) is armed by the shell when it
 * cold-CreateProcess'es a GUI exe, and nothing the launched process does to
 * itself reaches that decision (we proved this exhaustively). The fix is to
 * change WHO launches: register as the DelegateExecute handler for exefile so
 * a double-click routes through this resident, already-input-idle COM server,
 * which performs the CreateProcessW itself -> no cold-launch feedback cursor.
 *
 * NoCRT, custom entry. Imports kernel32, user32, ole32, advapi32.
 *
 * MODES (parsed by substring match on GetCommandLineW)
 *   /unregister    Tear down HKCU registration.
 *   -Embedding     rpcss launched us as a COM server -> run_com_server.
 *   (other)        Direct launch -> self-install on first run, show GUI.
 *
 * REGISTRATION (HKCU only)
 *   CLSID\{guid}\LocalServer32\(Default) = exe path
 *   exefile\shell\open\command\DelegateExecute = "{guid}"
 *
 * RECOVERY
 *   cmd /c "<exe>" /unregister
 */

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
#include <initguid.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <shlobj.h>

#ifndef STARTF_HASSHELLDATA
#define STARTF_HASSHELLDATA 0x00000400   /* not in the SDK headers; hStdOutput holds the shell HMONITOR */
#endif

/* GUIDs we need emitted as storage, without uuid.lib. */
DEFINE_GUID(IID_IUnknown,            0x00000000, 0x0000, 0x0000, 0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46);
DEFINE_GUID(IID_IClassFactory,       0x00000001, 0x0000, 0x0000, 0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46);
DEFINE_GUID(IID_IExecuteCommand,     0x7F9185B0, 0xCB92, 0x43C5, 0x80,0xA9,0x92,0x27,0x7A,0x4F,0x7B,0x54);
DEFINE_GUID(IID_IObjectWithSelection,0x1C9CD5BB, 0x98E9, 0x4491, 0xA6,0x0F,0x31,0xAA,0xCC,0x72,0xB8,0x3C);

DEFINE_GUID(CLSID_WinMainEx,
    0xE5F1A9C2, 0x8B7D, 0x4E3F,
    0xA1, 0x5C, 0x9D, 0x2E, 0x7B, 0x6F, 0x4A, 0x83);

/* NoCRT replacement for memcmp; IsEqualIID compiles down to memcmp.
 * `#pragma function` opts out of the /Oi intrinsic so we can supply our own. */
#pragma function(memcmp)
int __cdecl memcmp(const void *a, const void *b, size_t n) {
    const BYTE *pa = a, *pb = b;
    while (n--) { if (*pa != *pb) return (int)*pa - (int)*pb; pa++; pb++; }
    return 0;
}


static volatile LONG g_object_count;
static BOOL          g_is_com_server;   /* TRUE in run_com_server; WM_DESTROY should NOT exit */
static WCHAR         g_my_path[MAX_PATH];
/* Reusable command-line buffer for ec_Execute. STA serializes Execute()
 * calls on the main thread, so one shared buffer is safe. Lives in .bss. */
static WCHAR         g_cmd_buf[2048];

#define WMX_WND_CLASS L"WinMainEx"
/* Set of every .exe that has activated through us. Value name = full path,
 * data = L"1". Idempotent: re-recording the same path overwrites in place. */
#define WMX_LIST_KEY  L"Software\\WinMainEx\\Launched"

/* ------------------------------------------------------------------ */
/* Small inline string helpers (NoCRT, kernel32-only)                  */
/* ------------------------------------------------------------------ */

static __forceinline int gnc_strlen(LPCWSTR s) {
    int n = 0; while (s[n]) n++; return n;
}
static __forceinline void gnc_strcpy(LPWSTR dst, LPCWSTR src) {
    while ((*dst++ = *src++) != 0) {}
}
static __forceinline LPWSTR gnc_append(LPWSTR dst, LPCWSTR src) {
    while ((*dst = *src) != 0) { dst++; src++; }
    return dst;
}
static __forceinline BOOL gnc_iarg(LPCWSTR cmd, LPCWSTR target) {
    /* Whole-token case-insensitive substring search across cmd. */
    int tlen = gnc_strlen(target);
    for (LPCWSTR p = cmd; *p; p++) {
        WCHAR pre = (p == cmd) ? L' ' : p[-1];
        if (pre != L' ' && pre != L'"') continue;
        if (CompareStringOrdinal(p, tlen, target, tlen, TRUE) != CSTR_EQUAL) continue;
        WCHAR post = p[tlen];
        if (post == 0 || post == L' ' || post == L'"') return TRUE;
    }
    return FALSE;
}

/* Add an activated exe path to the WMX_LIST_KEY set. One create+set call,
 * idempotent, errors ignored — recording must never affect the launch. */
static void record_exe(LPCWSTR path)
{
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, WMX_LIST_KEY, 0, NULL, 0,
                        KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        static const WCHAR one[] = L"1";
        RegSetValueExW(hKey, path, 0, REG_SZ, (const BYTE *)one, (DWORD)sizeof(one));
        RegCloseKey(hKey);
    }
}

/* Is this exe already in the launched-exe set (i.e. opted in to our handling)? */
static BOOL is_exe_registered(LPCWSTR path)
{
    HKEY hKey;
    BOOL found = FALSE;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, WMX_LIST_KEY, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, path, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) found = TRUE;
        RegCloseKey(hKey);
    }
    return found;
}

/* ------------------------------------------------------------------ */
/* CommandObject: IExecuteCommand + IObjectWithSelection                */
/* ------------------------------------------------------------------ */

typedef struct CommandObject {
    const IExecuteCommandVtbl      *vtbl_ec;
    const IObjectWithSelectionVtbl *vtbl_ows;
    LONG                            ref;
    IShellItemArray                *selection;
    WCHAR                           params[1024];
    WCHAR                           directory[MAX_PATH];
    int                             show_window;
    BOOL                            show_set;
    POINT                           pos;
    BOOL                            pos_set;
} CommandObject;

static const IExecuteCommandVtbl      g_ec_vtbl;
static const IObjectWithSelectionVtbl g_ows_vtbl;

static void show_gui(const STARTUPINFOW *si);

static HRESULT cmd_QI(CommandObject *obj, REFIID iid, void **ppv) {
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IExecuteCommand))
        *ppv = (void *)&obj->vtbl_ec;
    else if (IsEqualIID(iid, &IID_IObjectWithSelection))
        *ppv = (void *)&obj->vtbl_ows;
    else { *ppv = NULL; return E_NOINTERFACE; }
    InterlockedIncrement(&obj->ref);
    return S_OK;
}
static ULONG cmd_AddRef(CommandObject *obj) {
    return (ULONG)InterlockedIncrement(&obj->ref);
}
static ULONG cmd_Release(CommandObject *obj) {
    LONG r = InterlockedDecrement(&obj->ref);
    if (r == 0) {
        if (obj->selection) IShellItemArray_Release(obj->selection);
        HeapFree(GetProcessHeap(), 0, obj);
        InterlockedDecrement(&g_object_count);
    }
    return (ULONG)r;
}

#define EC_OBJ(p)  ((CommandObject *)((BYTE *)(p) - offsetof(CommandObject, vtbl_ec)))
#define OWS_OBJ(p) ((CommandObject *)((BYTE *)(p) - offsetof(CommandObject, vtbl_ows)))

/* IExecuteCommand */
static HRESULT STDMETHODCALLTYPE ec_QI(IExecuteCommand *s, REFIID i, void **p)        { return cmd_QI(EC_OBJ(s), i, p); }
static ULONG   STDMETHODCALLTYPE ec_AddRef(IExecuteCommand *s)                         { return cmd_AddRef(EC_OBJ(s)); }
static ULONG   STDMETHODCALLTYPE ec_Release(IExecuteCommand *s)                        { return cmd_Release(EC_OBJ(s)); }
static HRESULT STDMETHODCALLTYPE ec_SetKeyState(IExecuteCommand *s, DWORD k)           { (void)s; (void)k; return S_OK; }
static HRESULT STDMETHODCALLTYPE ec_SetParameters(IExecuteCommand *s, LPCWSTR p) {
    CommandObject *o = EC_OBJ(s);
    if (p) { int i; for (i = 0; p[i] && i < 1023; i++) o->params[i] = p[i]; o->params[i] = 0; }
    else o->params[0] = 0;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE ec_SetPosition(IExecuteCommand *s, POINT pt) {
    CommandObject *o = EC_OBJ(s);
    o->pos = pt;
    o->pos_set = TRUE;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE ec_SetShowWindow(IExecuteCommand *s, int n) {
    CommandObject *o = EC_OBJ(s);
    o->show_window = n;
    o->show_set    = TRUE;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE ec_SetNoShowUI(IExecuteCommand *s, BOOL b)            { (void)s; (void)b; return S_OK; }
static HRESULT STDMETHODCALLTYPE ec_SetDirectory(IExecuteCommand *s, LPCWSTR d) {
    CommandObject *o = EC_OBJ(s);
    if (d) { int i; for (i = 0; d[i] && i < MAX_PATH - 1; i++) o->directory[i] = d[i]; o->directory[i] = 0; }
    else o->directory[0] = 0;
    return S_OK;
}

/* --- THE HOT PATH ------------------------------------------------- */
static HRESULT STDMETHODCALLTYPE ec_Execute(IExecuteCommand *self)
{
    CommandObject *o = EC_OBJ(self);

    if (!o->selection) { STARTUPINFOW s0 = { sizeof(s0) }; show_gui(&s0); return S_OK; }

    IShellItem *item;
    if (FAILED(IShellItemArray_GetItemAt(o->selection, 0, &item))) return E_FAIL;

    LPWSTR path = NULL;
    HRESULT hr = IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &path);
    IShellItem_Release(item);
    if (FAILED(hr) || !path) return hr;

    /* Single ordinal full-path compare against cached g_my_path. */
    if (CompareStringOrdinal(path, -1, g_my_path, -1, TRUE) == CSTR_EQUAL) {
        /* Synthesize a STARTUPINFO from the shell's IExecuteCommand intent so our
         * window honors the requested show state + position. */
        STARTUPINFOW s2 = { sizeof(s2) };
        if (o->show_set) { s2.dwFlags |= STARTF_USESHOWWINDOW; s2.wShowWindow = (WORD)o->show_window; }
        if (o->pos_set)  { s2.dwFlags |= STARTF_USEPOSITION;   s2.dwX = (DWORD)o->pos.x; s2.dwY = (DWORD)o->pos.y; }
        CoTaskMemFree(path);
        show_gui(&s2);
        return S_OK;
    }

    /* Non-self exe. Only exes already in our set get the cursor-free treatment.
     * A first-seen exe is recorded (the one-call bootstrap) but its launch this
     * time is left UNMOLESTED -- faithful pass-through, normal feedback. */
    BOOL registered = is_exe_registered(path);
    if (!registered) record_exe(path);

    /* Forward: build "<path>" + params into the static g_cmd_buf. STA,
     * sequential Execute() calls -> safe to reuse. Zero alloc per launch.
     * The CreateProcessW happens from this resident, input-idle server,
     * so the shell never arms IDC_APPSTARTING -> no spinny. */
    LPWSTR w = g_cmd_buf;
    *w++ = L'"';
    w = gnc_append(w, path);
    *w++ = L'"';
    if (o->params[0]) {
        *w++ = L' ';
        w = gnc_append(w, o->params);
    }
    *w = 0;

    STARTUPINFOW si = { sizeof(si) };
    /* Only opted-in exes get cursor suppression; non-registered launches are
     * left exactly as the shell would have done them (no flag = normal feedback). */
    si.dwFlags = registered ? STARTF_FORCEOFFFEEDBACK : 0;
    if (o->show_set) { si.dwFlags |= STARTF_USESHOWWINDOW; si.wShowWindow = (WORD)o->show_window; }
    if (o->pos_set)  { si.dwFlags |= STARTF_USEPOSITION;   si.dwX = (DWORD)o->pos.x; si.dwY = (DWORD)o->pos.y; }
    PROCESS_INFORMATION pi;
    HRESULT rc;
    if (CreateProcessW(NULL, g_cmd_buf, NULL, NULL, FALSE, 0, NULL,
            o->directory[0] ? o->directory : NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        rc = S_OK;
    } else if (GetLastError() == ERROR_ELEVATION_REQUIRED) {
        /* Retain correct behavior: target requires admin. Route through the
         * 'runas' verb (exefile\shell\runas, NOT our 'open' DelegateExecute,
         * so no re-entry) to get the normal UAC consent prompt. */
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask        = SEE_MASK_FLAG_NO_UI;
        sei.lpVerb       = L"runas";
        sei.lpFile       = path;
        sei.lpParameters = o->params[0]    ? o->params    : NULL;
        sei.lpDirectory  = o->directory[0] ? o->directory : NULL;
        sei.nShow        = o->show_set ? o->show_window : SW_SHOWNORMAL;
        rc = ShellExecuteExW(&sei) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
    } else {
        rc = HRESULT_FROM_WIN32(GetLastError());
    }
    CoTaskMemFree(path);
    return rc;
}

static const IExecuteCommandVtbl g_ec_vtbl = {
    ec_QI, ec_AddRef, ec_Release,
    ec_SetKeyState, ec_SetParameters, ec_SetPosition,
    ec_SetShowWindow, ec_SetNoShowUI, ec_SetDirectory, ec_Execute
};

/* IObjectWithSelection */
static HRESULT STDMETHODCALLTYPE ows_QI(IObjectWithSelection *s, REFIID i, void **p) { return cmd_QI(OWS_OBJ(s), i, p); }
static ULONG   STDMETHODCALLTYPE ows_AddRef(IObjectWithSelection *s)                  { return cmd_AddRef(OWS_OBJ(s)); }
static ULONG   STDMETHODCALLTYPE ows_Release(IObjectWithSelection *s)                 { return cmd_Release(OWS_OBJ(s)); }
static HRESULT STDMETHODCALLTYPE ows_SetSelection(IObjectWithSelection *s, IShellItemArray *psia) {
    CommandObject *o = OWS_OBJ(s);
    if (o->selection) IShellItemArray_Release(o->selection);
    o->selection = psia;
    if (psia) IShellItemArray_AddRef(psia);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE ows_GetSelection(IObjectWithSelection *s, REFIID riid, void **ppv) {
    CommandObject *o = OWS_OBJ(s);
    if (!o->selection) { *ppv = NULL; return E_FAIL; }
    return IShellItemArray_QueryInterface(o->selection, riid, ppv);
}
static const IObjectWithSelectionVtbl g_ows_vtbl = {
    ows_QI, ows_AddRef, ows_Release, ows_SetSelection, ows_GetSelection
};

/* ------------------------------------------------------------------ */
/* IClassFactory singleton                                              */
/* ------------------------------------------------------------------ */

typedef struct ClassFactory { const IClassFactoryVtbl *vtbl; } ClassFactory;
static HRESULT STDMETHODCALLTYPE cf_QI(IClassFactory *s, REFIID i, void **p) {
    if (IsEqualIID(i, &IID_IUnknown) || IsEqualIID(i, &IID_IClassFactory)) { *p = s; return S_OK; }
    *p = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE cf_AddRef(IClassFactory *s)  { (void)s; return 2; }
static ULONG STDMETHODCALLTYPE cf_Release(IClassFactory *s) { (void)s; return 1; }
static HRESULT STDMETHODCALLTYPE cf_CreateInstance(IClassFactory *s, IUnknown *outer, REFIID iid, void **ppv) {
    (void)s;
    *ppv = NULL;
    if (outer) return CLASS_E_NOAGGREGATION;
    CommandObject *obj = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CommandObject));
    if (!obj) return E_OUTOFMEMORY;
    obj->vtbl_ec  = &g_ec_vtbl;
    obj->vtbl_ows = &g_ows_vtbl;
    obj->ref      = 1;
    InterlockedIncrement(&g_object_count);
    HRESULT hr = cmd_QI(obj, iid, ppv);
    cmd_Release(obj);
    return hr;
}
static HRESULT STDMETHODCALLTYPE cf_LockServer(IClassFactory *s, BOOL b) { (void)s; (void)b; return S_OK; }
static const IClassFactoryVtbl g_cf_vtbl = {
    cf_QI, cf_AddRef, cf_Release, cf_CreateInstance, cf_LockServer
};
static ClassFactory g_class_factory = { &g_cf_vtbl };

/* ------------------------------------------------------------------ */
/* GUI (slim) — one overlapped window per show_gui call.                */
/* ------------------------------------------------------------------ */
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_DESTROY) {
        /* Direct-run mode: the window IS the process -> exit. COM-server
         * mode: windows come and go; the server stays alive. */
        if (!g_is_com_server) PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}
/* Center hwnd in the work area of the given monitor. */
static void CenterOnMonitor(HWND hwnd, HMONITOR mon)
{
    MONITORINFO mi;
    RECT wr;
    int w, h, x, y;
    mi.cbSize = (DWORD)sizeof(mi);
    if (!GetMonitorInfoW(mon, &mi)) return;
    GetWindowRect(hwnd, &wr);
    w = (int)(wr.right - wr.left);
    h = (int)(wr.bottom - wr.top);
    x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - w) / 2;
    y = mi.rcWork.top  + ((mi.rcWork.bottom - mi.rcWork.top) - h) / 2;
    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/* Show our window honoring the launcher's STARTUPINFO (show state + placement).
 *   show state: STARTF_USESHOWWINDOW -> wShowWindow, else SW_SHOWDEFAULT.
 *   placement:  STARTF_USEPOSITION  -> create at dwX/dwY (leave it).
 *               STARTF_HASSHELLDATA -> hStdOutput is the shell's HMONITOR; center there.
 *               otherwise           -> center on the monitor we landed on.
 * For the COM path `si` is synthesized from IExecuteCommand (SetShowWindow/SetPosition);
 * for the direct path it is the real GetStartupInfoW. */
static void show_gui(const STARTUPINFOW *si)
{
    static BOOL class_registered = FALSE;
    HINSTANCE hi = GetModuleHandleW(NULL);
    int  nCmdShow = (si->dwFlags & STARTF_USESHOWWINDOW) ? (int)si->wShowWindow : SW_SHOWDEFAULT;
    BOOL usePos   = (si->dwFlags & STARTF_USEPOSITION) != 0;
    int  x = usePos ? (int)si->dwX : CW_USEDEFAULT;
    int  y = usePos ? (int)si->dwY : CW_USEDEFAULT;
    HWND hwnd;
    if (!class_registered) {
        WNDCLASSW wc = {0};
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = hi;
        wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = WMX_WND_CLASS;
        RegisterClassW(&wc);
        class_registered = TRUE;
    }
    /* Created hidden so we can place it before honoring the show state. */
    hwnd = CreateWindowExW(0, WMX_WND_CLASS, L"WinMainEx",
        WS_OVERLAPPEDWINDOW, x, y, 700, 220, NULL, NULL, hi, NULL);
    if (!hwnd) return;
    if (!usePos) {
        HMONITOR mon = (si->dwFlags & STARTF_HASSHELLDATA)
            ? (HMONITOR)si->hStdOutput
            : MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        CenterOnMonitor(hwnd, mon);
    }
    ShowWindow(hwnd, nCmdShow);
    SetForegroundWindow(hwnd);
}

/* ------------------------------------------------------------------ */
/* Registry install / uninstall (HKCU)                                  */
/* ------------------------------------------------------------------ */

static void clsid_str(WCHAR out[40]) {
    StringFromGUID2(&CLSID_WinMainEx, out, 40);
}
static LONG reg_set_sz(HKEY parent, LPCWSTR sub, LPCWSTR name, LPCWSTR val) {
    HKEY hKey;
    LONG e = RegCreateKeyExW(parent, sub, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    if (e) return e;
    int n = gnc_strlen(val);
    e = RegSetValueExW(hKey, name, 0, REG_SZ, (const BYTE *)val, (DWORD)((n + 1) * sizeof(WCHAR)));
    RegCloseKey(hKey);
    return e;
}

static int do_register(void)
{
    WCHAR clsid[40]; clsid_str(clsid);
    WCHAR sub[256];
    /* sub = "Software\\Classes\\CLSID\\{guid}" */
    LPWSTR w = sub;
    w = gnc_append(w, L"Software\\Classes\\CLSID\\");
    gnc_strcpy(w, clsid);
    if (reg_set_sz(HKEY_CURRENT_USER, sub, NULL, L"WinMainEx")) return 1;

    /* sub = "Software\\Classes\\CLSID\\{guid}\\LocalServer32" */
    w = gnc_append(sub, L"Software\\Classes\\CLSID\\");
    w = gnc_append(w, clsid);
    gnc_strcpy(w, L"\\LocalServer32");
    if (reg_set_sz(HKEY_CURRENT_USER, sub, NULL, g_my_path)) return 1;

    /* exefile open\command DelegateExecute = our CLSID */
    if (reg_set_sz(HKEY_CURRENT_USER,
            L"Software\\Classes\\exefile\\shell\\open\\command",
            NULL, L"\"%1\" %*")) return 1;
    if (reg_set_sz(HKEY_CURRENT_USER,
            L"Software\\Classes\\exefile\\shell\\open\\command",
            L"DelegateExecute", clsid)) return 1;
    /* Tell the shell the exefile association changed, else Explorer keeps using
     * its cached (default cold-launch) association and never calls our handler. */
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return 0;
}

static int do_unregister(void)
{
    WCHAR clsid[40]; clsid_str(clsid);
    WCHAR sub[256];
    LPWSTR w = gnc_append(sub, L"Software\\Classes\\CLSID\\");
    gnc_strcpy(w, clsid);
    RegDeleteTreeW(HKEY_CURRENT_USER, sub);

    HKEY hKey;
    if (ERROR_SUCCESS == RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Classes\\exefile\\shell\\open\\command",
            0, KEY_SET_VALUE, &hKey)) {
        RegDeleteValueW(hKey, L"DelegateExecute");
        RegDeleteValueW(hKey, NULL);
        RegCloseKey(hKey);
    }
    RegDeleteKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\exefile\\shell\\open\\command", 0, 0);
    RegDeleteKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\exefile\\shell\\open",         0, 0);
    RegDeleteKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\exefile\\shell",               0, 0);
    RegDeleteKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\exefile",                       0, 0);
    return 0;
}

static BOOL is_registered(void)
{
    WCHAR clsid[40]; clsid_str(clsid);
    WCHAR sub[256];
    LPWSTR w = gnc_append(sub, L"Software\\Classes\\CLSID\\");
    w = gnc_append(w, clsid);
    gnc_strcpy(w, L"\\LocalServer32");
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, sub, 0, KEY_READ, &hKey)) return FALSE;
    RegCloseKey(hKey);
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Modes                                                                */
/* ------------------------------------------------------------------ */

static int run_com_server(void)
{
    g_is_com_server = TRUE;
    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
        return 1;
    DWORD cookie = 0;
    if (FAILED(CoRegisterClassObject(&CLSID_WinMainEx,
            (IUnknown *)&g_class_factory,
            CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &cookie))) {
        CoUninitialize();
        return 2;
    }
    /* Standard STA pump: dispatches COM RPC (ole32's hidden STA window)
     * and any show_gui window messages. WM_QUIT from PostQuitMessage in
     * WndProc on WM_DESTROY, or from the OS at logoff. */
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (cookie) CoRevokeClassObject(cookie);
    CoUninitialize();
    return 0;
}

static int run_direct(void)
{
    STARTUPINFOW si;
    if (!is_registered()) do_register();
    GetStartupInfoW(&si);   /* direct launch: honor the real launcher's STARTUPINFO */
    show_gui(&si);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Entry — NoCRT custom entry                                           */
/* ------------------------------------------------------------------ */

void __cdecl mainCRTStartup(void)
{
    GetModuleFileNameW(NULL, g_my_path, MAX_PATH);

    LPCWSTR cmd = GetCommandLineW();
    int rc;
    if (gnc_iarg(cmd, L"/unregister"))
        rc = do_unregister();
    else if (gnc_iarg(cmd, L"-Embedding") || gnc_iarg(cmd, L"/Embedding"))
        rc = run_com_server();
    else
        rc = run_direct();

    ExitProcess((UINT)rc);
}
