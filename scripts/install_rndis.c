/* Reversible installer/uninstaller for CE 5.0 Host RNDIS (USB8023 + RNDISMP).
 * Follows Microsoft CE 5.0 common.reg layout (Comm\USB8023 -> ImagePath=USB8023.DLL)
 * without touching Function-side RNDISFN, LeadCore LC6K_Modem, or ME3630ComV18D40.
 * Compile with /DRNDIS_REMOVE for uninstall.
 */
#include <windows.h>
#undef wsprintf
#undef wsprintfW
#define wsprintf wsprintfW

typedef BOOL (*PFN_USBINSTALL)(LPCWSTR);
typedef BOOL (*PFN_USBUNINSTALL)(void);

static BOOL setSz(HKEY key, LPCWSTR name, LPCWSTR val)
{
    return RegSetValueExW(key, name, 0, REG_SZ, (const BYTE*)val, (DWORD)((wcslen(val) + 1) * sizeof(WCHAR))) == ERROR_SUCCESS;
}

static BOOL setMultiSz(HKEY key, LPCWSTR name, LPCWSTR val, DWORD wcharsWithDoubleNull)
{
    return RegSetValueExW(key, name, 0, REG_MULTI_SZ, (const BYTE*)val, wcharsWithDoubleNull * sizeof(WCHAR)) == ERROR_SUCCESS;
}

static BOOL setDword(HKEY key, LPCWSTR name, DWORD val)
{
    return RegSetValueExW(key, name, 0, REG_DWORD, (const BYTE*)&val, sizeof(val)) == ERROR_SUCCESS;
}

static BOOL createKey(LPCWSTR path, HKEY *out)
{
    DWORD disp = 0;
    return RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, 0, NULL, out, &disp) == ERROR_SUCCESS;
}

static void deleteTree(LPCWSTR path)
{
    HKEY key = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, 0, &key) == ERROR_SUCCESS) {
        WCHAR sub[128];
        for (;;) {
            DWORD len = sizeof(sub) / sizeof(sub[0]);
            if (RegEnumKeyExW(key, 0, sub, &len, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
            {
                WCHAR child[256];
                wsprintf(child, L"%s\\%s", path, sub);
                deleteTree(child);
            }
        }
        RegCloseKey(key);
    }
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, path);
}

void mainCRTStartup(void)
{
    HMODULE mod = LoadLibraryW(L"\\Windows\\usb8023.dll");
    HANDLE log = CreateFileW(
#ifdef RNDIS_REMOVE
        L"\\Temp\\ME3630-rndis-uninstall.txt",
#else
        L"\\Temp\\ME3630-rndis-install.txt",
#endif
        GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    WCHAR buf[512];
    DWORD written = 0;
    BOOL usbOk = FALSE, regOk = TRUE;
    DWORD loadErr = mod ? 0 : GetLastError();

#ifdef RNDIS_REMOVE
    if (mod) {
        PFN_USBUNINSTALL fn = (PFN_USBUNINSTALL)GetProcAddressW(mod, L"USBUnInstallDriver");
        if (fn) usbOk = fn();
    }
    deleteTree(L"Drivers\\USB\\LoadClients\\Default\\2\\default\\USB8023");
    deleteTree(L"Drivers\\USB\\ClientDrivers\\USB8023");
    deleteTree(L"Comm\\USB8023");
    deleteTree(L"Comm\\USB80231");
    wsprintf(buf, L"RNDIS_UNINSTALL loaded=%lu load_err=%lu usb_uninst=%lu COMPLETE\r\n", mod != NULL, loadErr, usbOk);
#else
    if (mod) {
        PFN_USBINSTALL fn = (PFN_USBINSTALL)GetProcAddressW(mod, L"USBInstallDriver");
        if (fn) usbOk = fn(L"USB8023.DLL");
    }
    if (usbOk) {
        HKEY k = NULL;
        /* Ensure explicit plain DisplayName + DHCP TcpIp parameters matching CE5 common.reg */
        if (createKey(L"Comm\\USB8023", &k)) {
            regOk &= setSz(k, L"DisplayName", L"USB RNDIS Host");
            regOk &= setSz(k, L"Group", L"NDIS");
            regOk &= setSz(k, L"ImagePath", L"USB8023.DLL");
            RegCloseKey(k);
        } else regOk = FALSE;

        if (createKey(L"Comm\\USB8023\\Linkage", &k)) {
            static const WCHAR route[] = L"USB80231\0";
            regOk &= setMultiSz(k, L"Route", route, 10);
            RegCloseKey(k);
        } else regOk = FALSE;

        if (createKey(L"Comm\\USB80231", &k)) {
            regOk &= setSz(k, L"DisplayName", L"USB RNDIS Host");
            regOk &= setSz(k, L"Group", L"NDIS");
            regOk &= setSz(k, L"ImagePath", L"USB8023.DLL");
            RegCloseKey(k);
        } else regOk = FALSE;

        if (createKey(L"Comm\\USB80231\\Parms", &k)) {
            regOk &= setDword(k, L"BusNumber", 0);
            regOk &= setDword(k, L"BusType", 0);
            regOk &= setDword(k, L"RebindOnResume", 0);
            RegCloseKey(k);
        } else regOk = FALSE;

        if (createKey(L"Comm\\USB80231\\Parms\\TcpIp", &k)) {
            regOk &= setDword(k, L"EnableDHCP", 1);
            regOk &= setDword(k, L"UseZeroBroadcast", 0);
            regOk &= setSz(k, L"IpAddress", L"0.0.0.0");
            regOk &= setSz(k, L"Subnetmask", L"0.0.0.0");
            regOk &= setSz(k, L"DefaultGateway", L"");
            RegCloseKey(k);
        } else regOk = FALSE;
    } else {
        regOk = FALSE;
    }
    wsprintf(buf, L"RNDIS_INSTALL loaded=%lu load_err=%lu usb_inst=%lu reg_ok=%lu COMPLETE\r\n", mod != NULL, loadErr, usbOk, regOk);
#endif
    if (log != INVALID_HANDLE_VALUE) {
        WriteFile(log, buf, (DWORD)(wcslen(buf) * sizeof(WCHAR)), &written, NULL);
        CloseHandle(log);
    }
    if (mod) FreeLibrary(mod);
    ExitProcess((usbOk && regOk) ? 0 : 1);
}
