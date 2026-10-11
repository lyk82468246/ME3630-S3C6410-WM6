#include <windows.h>
#undef wsprintfW
typedef BOOL (*INSTALL)(LPCWSTR);
typedef BOOL (*UNINSTALL)(void);
void mainCRTStartup(void)
{
#ifdef PROBE_V2
    HMODULE dll=LoadLibrary(L"ME3630ProbeV2.dll"),old;
#else
    HMODULE dll=LoadLibrary(L"ME3630Probe.dll");
#endif
    DWORD error=dll?0:GetLastError(),bytes;
    BOOL ok=FALSE;
    HANDLE file;
    WCHAR text[256];
    INSTALL install;
    UNINSTALL uninstall;
    int n;
    if(dll) {
#ifdef REMOVE_PROBE
        uninstall=(UNINSTALL)GetProcAddressW(dll,L"USBUnInstallDriver");
        if(uninstall)ok=uninstall();
#else
#ifdef PROBE_V2
        old=LoadLibrary(L"ME3630Probe.dll");
        if(old){uninstall=(UNINSTALL)GetProcAddressW(old,L"USBUnInstallDriver");if(uninstall)uninstall();FreeLibrary(old);}
        install=(INSTALL)GetProcAddressW(dll,L"USBInstallDriver");
        if(install)ok=install(L"ME3630ProbeV2.dll");
        if(!ok){old=LoadLibrary(L"ME3630Probe.dll");if(old){install=(INSTALL)GetProcAddressW(old,L"USBInstallDriver");if(install)install(L"ME3630Probe.dll");FreeLibrary(old);}}
#else
        install=(INSTALL)GetProcAddressW(dll,L"USBInstallDriver");
        if(install)ok=install(L"ME3630Probe.dll");
#endif
#endif
        if(!ok)error=GetLastError();
        FreeLibrary(dll);
    }
#ifdef REMOVE_PROBE
    file=CreateFile(L"\\Temp\\ME3630-uninstall.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
#else
#ifdef PROBE_V2
    file=CreateFile(L"\\Temp\\ME3630-install-v2.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
#else
    file=CreateFile(L"\\Temp\\ME3630-install.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
#endif
#endif
    if(file!=INVALID_HANDLE_VALUE){n=wsprintfW(text,L"dll_load=%lu installed=%lu error=%lu COMPLETE\r\n",dll!=NULL,ok,error);WriteFile(file,text,n*sizeof(WCHAR),&bytes,NULL);CloseHandle(file);}
    ExitProcess(ok?0:1);
}
