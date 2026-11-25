/* Read-only CE 5.2 runtime dependency check for CE 5.0 rndismp.dll and usb8023.dll.
 * Verifies every static COREDLL ordinal and every named NDIS/CXPORT/USBD import
 * without modifying registry or loading untrusted drivers. Optional: if invoked
 * with argument "loadtest", also calls LoadLibrary on \Temp\rndismp.dll and
 * \Temp\usb8023.dll (without registering anything) to verify CE5.2 loader/signature acceptance.
 */
#include <windows.h>
#undef wsprintf
#undef wsprintfW
#define wsprintf wsprintfW

static HANDLE logFile;
static DWORD totalChecks;
static DWORD missingChecks;

static void writeLine(LPCWSTR text)
{
    DWORD written;
    WriteFile(logFile, text, (DWORD)(wcslen(text) * sizeof(WCHAR)), &written, NULL);
}

static void checkOrdinals(LPCWSTR dll, const DWORD *ords, DWORD count)
{
    HMODULE mod = LoadLibraryW(dll);
    WCHAR buf[256];
    DWORD i, err = mod ? 0 : GetLastError();
    totalChecks++;
    if (!mod) missingChecks++;
    wsprintf(buf, L"DLL %s loaded=%lu error=%lu\r\n", dll, mod != NULL, err);
    writeLine(buf);
    if (!mod) return;
    for (i = 0; i < count; i++) {
        FARPROC p = GetProcAddressW(mod, (LPCWSTR)ords[i]);
        totalChecks++;
        if (!p) missingChecks++;
        wsprintf(buf, L"ORD %s #%lu present=%lu error=%lu\r\n", dll, ords[i], p != NULL, p ? 0 : GetLastError());
        writeLine(buf);
    }
    FreeLibrary(mod);
}

static void checkNames(LPCWSTR dll, const LPCWSTR *names, DWORD count)
{
    HMODULE mod = LoadLibraryW(dll);
    WCHAR buf[256];
    DWORD i, err = mod ? 0 : GetLastError();
    totalChecks++;
    if (!mod) missingChecks++;
    wsprintf(buf, L"DLL %s loaded=%lu error=%lu\r\n", dll, mod != NULL, err);
    writeLine(buf);
    if (!mod) return;
    for (i = 0; i < count; i++) {
        FARPROC p = GetProcAddressW(mod, names[i]);
        totalChecks++;
        if (!p) missingChecks++;
        wsprintf(buf, L"SYM %s %s present=%lu error=%lu\r\n", dll, names[i], p != NULL, p ? 0 : GetLastError());
        writeLine(buf);
    }
    FreeLibrary(mod);
}

static void testLoad(LPCWSTR path)
{
    HMODULE mod = LoadLibraryW(path);
    WCHAR buf[256];
    DWORD err = mod ? 0 : GetLastError();
    wsprintf(buf, L"LOADTEST %s loaded=%lu error=%lu\r\n", path, mod != NULL, err);
    writeLine(buf);
    if (mod) FreeLibrary(mod);
}

void mainCRTStartup(void)
{
    static const DWORD coreOrds[] = {
        2, 3, 4, 5, 6, 10, 11, 33, 36, 63, 87,
        455, 456, 461, 463, 464, 492, 494, 495, 496, 497, 498,
        516, 517, 524, 525, 553, 560, 611, 612,
        1044, 1046, 1047, 1097, 1232, 1486, 1487
    };
    static const LPCWSTR ndisSyms[] = {
        L"NdisAcquireSpinLock",
        L"NdisAllocateBuffer",
        L"NdisAllocateBufferPool",
        L"NdisAllocateFromNPagedLookasideList",
        L"NdisAllocatePacket",
        L"NdisAllocatePacketPoolEx",
        L"NdisAllocateSpinLock",
        L"NdisCloseConfiguration",
        L"NdisDeregisterAdapter",
        L"NdisFreeBuffer",
        L"NdisFreeBufferPool",
        L"NdisFreeEvent",
        L"NdisFreePacket",
        L"NdisFreePacketPool",
        L"NdisFreeSpinLock",
        L"NdisGetSystemUpTime",
        L"NdisInitializeEvent",
        L"NdisInitializeWrapper",
        L"NdisInterlockedDecrement",
        L"NdisInterlockedIncrement",
        L"NdisMCancelTimer",
        L"NdisMGetDeviceProperty",
        L"NdisMInitializeTimer",
        L"NdisMRegisterAdapterShutdownHandler",
        L"NdisMRegisterMiniport",
        L"NdisMRegisterUnloadHandler",
        L"NdisMSetAttributesEx",
        L"NdisMSleep",
        L"NdisOpenConfiguration",
        L"NdisQueryPacket",
        L"NdisReadNetworkAddress",
        L"NdisRegisterAdapter",
        L"NdisReleaseSpinLock",
        L"NdisSetEvent",
        L"NdisSetTimer",
        L"NdisTerminateWrapper",
        L"NdisWaitEvent"
    };
    static const LPCWSTR cxportSyms[] = {
        L"CTEAllocMem",
        L"CTEFreeMem"
    };
    static const LPCWSTR usbdSyms[] = {
        L"RegisterClientDriverID",
        L"RegisterClientSettings",
        L"UnRegisterClientDriverID",
        L"UnRegisterClientSettings"
    };

    WCHAR buf[256];
    LPCWSTR cmd = GetCommandLineW();
    BOOL doLoadTest = (cmd && wcsstr(cmd, L"loadtest") != NULL);

    logFile = CreateFileW(L"\\Temp\\ME3630-rndis-dep-probe.txt", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (logFile == INVALID_HANDLE_VALUE) ExitProcess(1);

    wsprintf(buf, L"RNDIS_DEP_PROBE start_tick=%lu loadtest=%lu\r\n", GetTickCount(), (DWORD)doLoadTest);
    writeLine(buf);

    checkOrdinals(L"coredll.dll", coreOrds, sizeof(coreOrds) / sizeof(coreOrds[0]));
    checkNames(L"ndis.dll", ndisSyms, sizeof(ndisSyms) / sizeof(ndisSyms[0]));
    checkNames(L"cxport.dll", cxportSyms, sizeof(cxportSyms) / sizeof(cxportSyms[0]));
    checkNames(L"usbd.dll", usbdSyms, sizeof(usbdSyms) / sizeof(usbdSyms[0]));

    if (doLoadTest) {
        HMODULE r = LoadLibraryW(L"\\Temp\\rndismp.dll");
        DWORD rerr = r ? 0 : GetLastError();
        wsprintf(buf, L"LOADTEST \\Temp\\rndismp.dll loaded=%lu error=%lu\r\n", r != NULL, rerr);
        writeLine(buf);
        testLoad(L"\\Temp\\usb8023.dll");
        if (r) FreeLibrary(r);
    }

    wsprintf(buf, L"SUMMARY checks=%lu missing=%lu COMPLETE\r\n", totalChecks, missingChecks);
    writeLine(buf);
    CloseHandle(logFile);
    ExitProcess(missingChecks ? 2 : 0);
}
