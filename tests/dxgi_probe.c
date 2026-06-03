/* dxgi_probe.c -- verify the dwmframex flip-swapchain port plan against the LIVE system DLLs
 * (dxgi.dll / d3d11.dll). Settles three points the MSDN docs contradict themselves on:
 *   1. Does CreateSwapChainForComposition accept DXGI_SCALING_NONE? (function page says "must STRETCH";
 *      enum page says NONE is supported for composition.)
 *   2. Does IDXGISwapChain::Present accept DXGI_PRESENT_RESTART on a windowed composition swapchain?
 *      (Present-flag page says RESTART is "flip-model AND full screen".)
 *   3. Does CreateSwapChainForComposition fail on a WARP/software device? (function page says software
 *      drivers "are not supported".)
 * Also confirms the frame-latency waitable handle + SetMaximumFrameLatency contract.
 * No DComp/D2D needed: ForComposition is windowless and Present's HRESULT is independent of visual binding.
 */
#define WIN32_LEAN_AND_MEAN
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <dxgi1_3.h>
#include <d3d11.h>
#include <stdio.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

static const char* hrname(HRESULT hr)
{
    switch ((unsigned)hr) {
        case 0x00000000u: return "S_OK";
        case 0x80070057u: return "E_INVALIDARG";
        case 0x887A0001u: return "DXGI_ERROR_INVALID_CALL";
        case 0x887A0004u: return "DXGI_ERROR_UNSUPPORTED";
        case 0x887A0005u: return "DXGI_ERROR_DEVICE_REMOVED";
        case 0x80004001u: return "E_NOTIMPL";
        case 0x80004005u: return "E_FAIL";
        default:          return "(other)";
    }
}
#define SHOW(label, hr) printf("  %-46s hr=0x%08X %s\n", (label), (unsigned)(hr), hrname(hr))

static IDXGIFactory2* g_factory;

static HRESULT make_device(D3D_DRIVER_TYPE drv, ID3D11Device** ppDev)
{
    return D3D11CreateDevice(NULL, drv, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                             NULL, 0, D3D11_SDK_VERSION, ppDev, NULL, NULL);
}

static IDXGIFactory2* factory_from(ID3D11Device* dev)
{
    IDXGIDevice*  dxdev = NULL;
    IDXGIAdapter* adap  = NULL;
    IDXGIFactory2* fac  = NULL;
    if (FAILED(ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void**)&dxdev)) || !dxdev) return NULL;
    if (FAILED(IDXGIDevice_GetAdapter(dxdev, &adap)) || !adap) { IDXGIDevice_Release(dxdev); return NULL; }
    (void)IDXGIAdapter_GetParent(adap, &IID_IDXGIFactory2, (void**)&fac);
    IDXGIAdapter_Release(adap);
    IDXGIDevice_Release(dxdev);
    return fac;
}

static void desc_init(DXGI_SWAP_CHAIN_DESC1* d, DXGI_SCALING scaling, DXGI_SWAP_EFFECT eff, UINT flags)
{
    ZeroMemory(d, sizeof(*d));
    d->Width  = 3840; d->Height = 2160;          /* monitor-sized, as the plan allocates */
    d->Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d->Stereo = FALSE;
    d->SampleDesc.Count = 1; d->SampleDesc.Quality = 0;
    d->BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d->BufferCount = 2;
    d->Scaling   = scaling;
    d->SwapEffect = eff;
    d->AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    d->Flags     = flags;
}

static IDXGISwapChain1* create_comp(IDXGIFactory2* fac, ID3D11Device* dev,
                                    DXGI_SCALING scaling, DXGI_SWAP_EFFECT eff, UINT flags, HRESULT* phr)
{
    DXGI_SWAP_CHAIN_DESC1 d;
    IDXGISwapChain1* sc = NULL;
    desc_init(&d, scaling, eff, flags);
    *phr = IDXGIFactory2_CreateSwapChainForComposition(fac, (IUnknown*)dev, &d, NULL, &sc);
    return sc;
}

int main(void)
{
    ID3D11Device* dev = NULL;
    HRESULT hr;
    const UINT WAIT = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    printf("== dwmframex flip-swapchain port: LIVE SYSTEM VERIFICATION ==\n");

    hr = make_device(D3D_DRIVER_TYPE_HARDWARE, &dev);
    SHOW("D3D11CreateDevice(HARDWARE, BGRA)", hr);
    if (!dev) { printf("no hardware device; aborting\n"); return 1; }

    g_factory = factory_from(dev);
    printf("  IDXGIFactory2 obtained: %s\n", g_factory ? "yes" : "NO");
    if (!g_factory) return 1;

    printf("\n[1] CreateSwapChainForComposition: SCALING contradiction\n");
    {
        IDXGISwapChain1* a = create_comp(g_factory, dev, DXGI_SCALING_STRETCH, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, WAIT, &hr);
        SHOW("FLIP_SEQUENTIAL + SCALING_STRETCH + WAITABLE", hr); if (a) IDXGISwapChain1_Release(a);
        IDXGISwapChain1* b = create_comp(g_factory, dev, DXGI_SCALING_NONE, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, WAIT, &hr);
        SHOW("FLIP_SEQUENTIAL + SCALING_NONE    + WAITABLE", hr); if (b) IDXGISwapChain1_Release(b);
        IDXGISwapChain1* c = create_comp(g_factory, dev, DXGI_SCALING_STRETCH, DXGI_SWAP_EFFECT_FLIP_DISCARD, WAIT, &hr);
        SHOW("FLIP_DISCARD    + SCALING_STRETCH + WAITABLE", hr); if (c) IDXGISwapChain1_Release(c);
        IDXGISwapChain1* e = create_comp(g_factory, dev, DXGI_SCALING_NONE, DXGI_SWAP_EFFECT_FLIP_DISCARD, WAIT, &hr);
        SHOW("FLIP_DISCARD    + SCALING_NONE    + WAITABLE", hr); if (e) IDXGISwapChain1_Release(e);
    }

    printf("\n[2] Frame-latency waitable + SetMaximumFrameLatency + Present(RESTART) contradiction\n");
    {
        IDXGISwapChain1* sc1 = create_comp(g_factory, dev, DXGI_SCALING_STRETCH, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, WAIT, &hr);
        SHOW("create (STRETCH/SEQUENTIAL/WAITABLE)", hr);
        if (sc1) {
            IDXGISwapChain2* sc2 = NULL;
            (void)IDXGISwapChain1_QueryInterface(sc1, &IID_IDXGISwapChain2, (void**)&sc2);
            printf("  QI IDXGISwapChain2: %s\n", sc2 ? "yes" : "NO");
            if (sc2) {
                HANDLE w = IDXGISwapChain2_GetFrameLatencyWaitableObject(sc2);
                printf("  GetFrameLatencyWaitableObject: %s\n", w ? "non-NULL (good)" : "NULL");
                hr = IDXGISwapChain2_SetMaximumFrameLatency(sc2, 1);
                SHOW("SetMaximumFrameLatency(1)", hr);
                hr = IDXGISwapChain2_SetSourceSize(sc2, 1280, 720);
                SHOW("SetSourceSize(1280,720)  [<= buffer]", hr);
                hr = IDXGISwapChain2_SetSourceSize(sc2, 5000, 720);
                SHOW("SetSourceSize(5000,720)  [> buffer width]", hr);
                if (w) (void)WaitForSingleObject(w, 1000);     /* doc: wait before first present */
                IDXGISwapChain2_Release(sc2);
            }
            hr = IDXGISwapChain1_Present(sc1, 0, 0);                       SHOW("Present(0, 0)", hr);
            hr = IDXGISwapChain1_Present(sc1, 1, 0);                       SHOW("Present(1, 0)  [vsync]", hr);
            hr = IDXGISwapChain1_Present(sc1, 1, DXGI_PRESENT_RESTART);    SHOW("Present(1, DXGI_PRESENT_RESTART)", hr);
            hr = IDXGISwapChain1_Present(sc1, 0, DXGI_PRESENT_RESTART);    SHOW("Present(0, DXGI_PRESENT_RESTART)", hr);
            hr = IDXGISwapChain1_Present(sc1, 1, DXGI_PRESENT_DO_NOT_SEQUENCE); SHOW("Present(1, DO_NOT_SEQUENCE)", hr);
            hr = IDXGISwapChain1_Present(sc1, 0, DXGI_PRESENT_ALLOW_TEARING);   SHOW("Present(0, ALLOW_TEARING) [no tearing flag set]", hr);
            IDXGISwapChain1_Release(sc1);
        }
    }

    printf("\n[3] WARP (software) device + CreateSwapChainForComposition\n");
    {
        ID3D11Device* warp = NULL;
        hr = make_device(D3D_DRIVER_TYPE_WARP, &warp);
        SHOW("D3D11CreateDevice(WARP, BGRA)", hr);
        if (warp) {
            IDXGIFactory2* wf = factory_from(warp);
            if (wf) {
                IDXGISwapChain1* ws = create_comp(wf, warp, DXGI_SCALING_STRETCH, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, WAIT, &hr);
                SHOW("WARP CreateSwapChainForComposition", hr);
                if (ws) IDXGISwapChain1_Release(ws);
                IDXGIFactory2_Release(wf);
            }
            ID3D11Device_Release(warp);
        }
    }

    IDXGIFactory2_Release(g_factory);
    ID3D11Device_Release(dev);
    printf("\n== done ==\n");
    return 0;
}
