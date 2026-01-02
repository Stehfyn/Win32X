#pragma check_stack(off)
#pragma strict_gs_check(off)
#pragma runtime_checks("", off)

#pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#pragma comment(linker, "/STACK:1024,1024")
//#pragma comment(linker, "/HEAP:0,0")
#pragma comment(linker, "/ALIGN:256")
#pragma comment(linker, "/NODEFAULTLIB")
#pragma comment(linker, "/ENTRY:mainCRTStartup")
#pragma comment(linker, "/INCLUDE:CreateProcessW")
#pragma comment(linker, "/INCLUDE:ExitProcess")
#pragma comment(linker, "/INCLUDE:MessageBoxW")
#pragma comment(linker, "/INCLUDE:FreeConsole")
#pragma comment(linker, "/INCLUDE:DuplicateHandle")
#pragma comment(linker, "/INCLUDE:InitializeProcThreadAttributeList")
#pragma comment(linker, "/INCLUDE:UpdateProcThreadAttribute")
#pragma comment(linker, "/INCLUDE:MessageBoxA")
#pragma comment(linker, "/INCREMENTAL:NO") // Needed to ensure debuggability in default debug configuration
#pragma comment(linker, "/MERGE:.pdata=.rdata")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


static __forceinline const void* NtCurrentImage(void) // cheese and rice it's jason bourne
{
    return (unsigned short*)*(unsigned char**)((unsigned short*)(*(unsigned char**)((unsigned char*)__readgsqword(0x60) + 0x20) + 0x60) + 0x04);
}

static __forceinline unsigned short NtStartupReserved2(void)
{
    return *(unsigned short*)((unsigned char*)NtCurrentImage() - 0x05A4);
}

typedef void* HANDLE;
typedef unsigned __int64 ULONG_PTR, * PULONG_PTR;
typedef ULONG_PTR SIZE_T;
typedef struct _PROC_THREAD_ATTRIBUTE_LIST* PPROC_THREAD_ATTRIBUTE_LIST, * LPPROC_THREAD_ATTRIBUTE_LIST;
#define CREATE_SUSPENDED                  0x00000004
#define CREATE_NO_WINDOW                  0x08000000
#define CREATE_NEW_PROCESS_GROUP          0x00000200
#define EXTENDED_STARTUPINFO_PRESENT      0x00080000

#define PROCESS_QUERY_LIMITED_INFORMATION  (0x1000)  
#define SYNCHRONIZE                      (0x00100000L)
#define PROCESS_TERMINATE                  (0x0001)  
#define HEAP_ZERO_MEMORY                0x00000008

#define PROC_THREAD_ATTRIBUTE_NUMBER    0x0000FFFF
#define PROC_THREAD_ATTRIBUTE_THREAD    0x00010000  // Attribute may be used with thread creation
#define PROC_THREAD_ATTRIBUTE_INPUT     0x00020000  // Attribute is input only
#define PROC_THREAD_ATTRIBUTE_ADDITIVE  0x00040000  // Attribute may be "accumulated," e.g. bitmasks, counters, etc.

#define ProcThreadAttributeHandleList 2
#define ProcThreadAttributeValue(Number, Thread, Input, Additive) \
    (((Number) & PROC_THREAD_ATTRIBUTE_NUMBER) | \
     ((Thread != 0) ? PROC_THREAD_ATTRIBUTE_THREAD : 0) | \
     ((Input != 0) ? PROC_THREAD_ATTRIBUTE_INPUT : 0) | \
     ((Additive != 0) ? PROC_THREAD_ATTRIBUTE_ADDITIVE : 0))
#define PROC_THREAD_ATTRIBUTE_HANDLE_LIST \
    ProcThreadAttributeValue (ProcThreadAttributeHandleList, 0, 1, 0)
typedef unsigned long       DWORD;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef unsigned short WCHAR;    // wc,   16-bit UNICODE character
#define CONST               const
#define far

typedef WCHAR* PWCHAR, * LPWCH, * PWCH;
typedef CONST WCHAR* LPCWCH, * PCWCH;
typedef BYTE far* LPBYTE;

typedef WCHAR* NWPSTR, * LPWSTR, * PWSTR;
typedef struct _STARTUPINFOW {
  DWORD   cb;
  LPWSTR  lpReserved;
  LPWSTR  lpDesktop;
  LPWSTR  lpTitle;
  DWORD   dwX;
  DWORD   dwY;
  DWORD   dwXSize;
  DWORD   dwYSize;
  DWORD   dwXCountChars;
  DWORD   dwYCountChars;
  DWORD   dwFillAttribute;
  DWORD   dwFlags;
  WORD    wShowWindow;
  WORD    cbReserved2;
  LPBYTE  lpReserved2;
  HANDLE  hStdInput;
  HANDLE  hStdOutput;
  HANDLE  hStdError;
} STARTUPINFOW, * LPSTARTUPINFOW;
typedef struct _STARTUPINFOEXW {
  STARTUPINFOW StartupInfo;
  LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList;
} STARTUPINFOEXW, * LPSTARTUPINFOEXW;
typedef struct _PROCESS_INFORMATION {
  HANDLE hProcess;
  HANDLE hThread;
  DWORD dwProcessId;
  DWORD dwThreadId;
} PROCESS_INFORMATION, * PPROCESS_INFORMATION, * LPPROCESS_INFORMATION;

#pragma pack(1)
typedef struct _SHARED_STARTUP_INFO_W {
  STARTUPINFOEXW StartupInfoEx;
  PROCESS_INFORMATION ProcessInfo;
  unsigned char attributeList[48];
} SHARED_STARTUP_INFO_W, * PSHARED_STARTUP_INFO_W;
#pragma pack()

#pragma section(".shared", read, write)
__declspec(allocate(".shared") selectany) SHARED_STARTUP_INFO_W _ = { 0x00 };

void __stdcall mainCRTStartup()
{
    //MessageBoxA(0, "", "Hello, World!", 0);
    if (0x00 == NtStartupReserved2())
    {
        HANDLE hParentProcess;
        HANDLE hProcessHeap = GetProcessHeap();
        DuplicateHandle((HANDLE)-1, (HANDLE)-1, (HANDLE)-1, &hParentProcess,
          PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE | PROCESS_TERMINATE, 1, 0);

        SIZE_T attributeListSize = 0;
        InitializeProcThreadAttributeList(0, 1, 0, &attributeListSize);

        PPROC_THREAD_ATTRIBUTE_LIST attributeList = (PPROC_THREAD_ATTRIBUTE_LIST)&_.attributeList[0];
        if (!InitializeProcThreadAttributeList(attributeList, 1, 0, &attributeListSize))
        {
          return 0;
        }

        if (!UpdateProcThreadAttribute(attributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &hParentProcess, sizeof(HANDLE), 0, 0))
        {
          return 0;
        }

        (_.StartupInfoEx.StartupInfo.cb = sizeof(STARTUPINFOEXW), _.StartupInfoEx.StartupInfo.dwFlags = 0x00000080,
            !!CreateProcessW(NtCurrentImage(), 0, 0, 0, 1, CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP | EXTENDED_STARTUPINFO_PRESENT, 0, 0, &_.StartupInfoEx, &_.ProcessInfo));
        AllowSetForegroundWindow(_.ProcessInfo.dwProcessId);
        ResumeThread(_.ProcessInfo.hThread);
        ExitProcess(0);
    }
    else
    {
        // Verification of expected behavior would assert that (0x6969 == NtStartupReserved2()) here 
        FreeConsole(), MessageBoxW(0, L"Yay, you re-executed yourself!", NtCurrentImage(), 0);
    }
}
