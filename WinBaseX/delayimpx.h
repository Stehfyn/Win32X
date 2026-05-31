#ifndef DELAYIMPX_H
#define DELAYIMPX_H

#include <windows.h>

/*
 * delayimpx.h -- internal delay-load thunk generators (spiritual successor to the toolset's
 * delayimp.h). Each macro emits a forwarding function that resolves its target export once, caches
 * it, and calls through it; a miss returns a caller-supplied value rather than faulting.
 * Self-contained: only LoadLibrary / GetModuleHandle / GetProcAddress, no CRT.
 */

/*
 * DELAYLOAD -- define a transparent delay-bound wrapper named _FuncName, signature _Args1, calling
 * convention _CallConv, returning _RetType. _hInst is a caller-owned HMODULE lvalue (the cached
 * library handle, shared across wrappers for the same DLL); _DllName is its module name; _Args2 is
 * the forwarded argument list; _ErrVal is returned if the library or the export is missing.
 */
#define DELAYLOAD(_hInst, _DllName, _CallConv, _FuncName, _Args1, _Args2, _RetType, _ErrVal) \
    _RetType _CallConv _FuncName _Args1                                                      \
    {                                                                                       \
        static FARPROC pfn;                                                                 \
        BOOL           fNeedResolve;                                                        \
                                                                                            \
        fNeedResolve = (NULL == pfn) || (NULL == (_hInst));                                 \
        if (fNeedResolve)                                                                   \
        {                                                                                   \
            if (NULL == (_hInst))                                                           \
            {                                                                               \
                (_hInst) = LoadLibrary(_DllName);                                           \
            }                                                                               \
            if (NULL == (_hInst))                                                           \
            {                                                                               \
                return (_RetType)(_ErrVal);                                                 \
            }                                                                               \
            pfn = GetProcAddress((_hInst), #_FuncName);                                     \
            if (NULL == pfn)                                                                \
            {                                                                               \
                return (_RetType)(_ErrVal);                                                 \
            }                                                                               \
        }                                                                                   \
        return ((_RetType(_CallConv*)_Args1)(pfn)) _Args2;                                  \
    }

/*
 * DECLARE_DLL_THUNK -- define a static Thunk_##name forwarding to dll!name when that module is
 * already loaded, returning fallback otherwise.
 */
#define DECLARE_DLL_THUNK(ret, dll, name, args, callargs, fallback)  \
    typedef ret (WINAPI *PFN_##name) args;                           \
    static ret WINAPI Thunk_##name args                              \
    {                                                                \
        static PFN_##name pfn;                                       \
        HMODULE h;                                                   \
        if (!pfn) {                                                  \
            h = GetModuleHandle(dll);                                \
            pfn = h ? (PFN_##name)GetProcAddress(h, #name) : NULL;   \
        }                                                            \
        if (pfn) return pfn callargs;                                \
        return fallback;                                             \
    }

#endif /* DELAYIMPX_H */
