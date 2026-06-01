/* Minimal packaged COM ExeServer implementing a do-nothing IWaitCursorManager
 * (IID/CLSID fc992f1f-debb-4596-b355-50c7a6dd1222). Registered in AppxManifest so the
 * package vends it; its Start/Restore/Stop are no-ops (suppress the wait cursor). */
#include <windows.h>

static const GUID CLSID_WaitCursorManager =
    {0xfc992f1f,0xdebb,0x4596,{0xb3,0x55,0x50,0xc7,0xa6,0xdd,0x12,0x22}};

/* --- IWaitCursorManager: IUnknown + Start(int) + Restore() + Stop(int) --- */
typedef struct { void* lpVtbl; LONG ref; } Obj;

static HRESULT STDMETHODCALLTYPE WCM_QI(void* p, REFIID riid, void** o) {
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &CLSID_WaitCursorManager)) {
        ((Obj*)p)->ref++; *o = p; return S_OK;
    }
    *o = 0; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE WCM_AddRef(void* p) { return (ULONG)InterlockedIncrement(&((Obj*)p)->ref); }
static ULONG STDMETHODCALLTYPE WCM_Release(void* p) {
    LONG r = InterlockedDecrement(&((Obj*)p)->ref);
    if (!r) HeapFree(GetProcessHeap(), 0, p);
    return (ULONG)r;
}
static HRESULT STDMETHODCALLTYPE WCM_Start(void* p, int id)   { (void)p;(void)id; return S_OK; }
static HRESULT STDMETHODCALLTYPE WCM_Restore(void* p)         { (void)p; return S_OK; }
static HRESULT STDMETHODCALLTYPE WCM_Stop(void* p, int id)    { (void)p;(void)id; return S_OK; }

typedef struct { HRESULT(STDMETHODCALLTYPE*QueryInterface)(void*,REFIID,void**); ULONG(STDMETHODCALLTYPE*AddRef)(void*);
    ULONG(STDMETHODCALLTYPE*Release)(void*); HRESULT(STDMETHODCALLTYPE*Start)(void*,int);
    HRESULT(STDMETHODCALLTYPE*Restore)(void*); HRESULT(STDMETHODCALLTYPE*Stop)(void*,int); } WCMVtbl;
static const WCMVtbl g_wcmVtbl = { WCM_QI, WCM_AddRef, WCM_Release, WCM_Start, WCM_Restore, WCM_Stop };

/* --- IClassFactory --- */
static HRESULT STDMETHODCALLTYPE CF_QI(IClassFactory* p, REFIID riid, void** o) {
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IClassFactory)) { *o = p; return S_OK; }
    *o = 0; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE CF_AddRef(IClassFactory* p)  { (void)p; return 2; }
static ULONG STDMETHODCALLTYPE CF_Release(IClassFactory* p) { (void)p; return 1; }
static HRESULT STDMETHODCALLTYPE CF_CreateInstance(IClassFactory* p, IUnknown* outer, REFIID riid, void** o) {
    Obj* obj; HRESULT hr; (void)p;
    if (outer) return CLASS_E_NOAGGREGATION;
    obj = (Obj*)HeapAlloc(GetProcessHeap(), 0, sizeof(Obj));
    if (!obj) return E_OUTOFMEMORY;
    obj->lpVtbl = (void*)&g_wcmVtbl; obj->ref = 1;
    hr = WCM_QI(obj, riid, o);
    WCM_Release(obj);
    return hr;
}
static HRESULT STDMETHODCALLTYPE CF_LockServer(IClassFactory* p, BOOL f) { (void)p;(void)f; return S_OK; }
static IClassFactoryVtbl g_cfVtbl = { CF_QI, CF_AddRef, CF_Release, CF_CreateInstance, CF_LockServer };
static IClassFactory g_cf = { &g_cfVtbl };

int WINAPI wWinMain(HINSTANCE h, HINSTANCE hp, LPWSTR cl, int sc)
{
    DWORD cookie; MSG msg;
    (void)h;(void)hp;(void)cl;(void)sc;
    CoInitializeEx(0, COINIT_MULTITHREADED);
    if (CoRegisterClassObject(&CLSID_WaitCursorManager, (IUnknown*)&g_cf,
            CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &cookie) == S_OK)
    {
        while (GetMessageW(&msg, 0, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        CoRevokeClassObject(cookie);
    }
    CoUninitialize();
    return 0;
}
