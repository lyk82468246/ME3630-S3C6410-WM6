/* Install/uninstall the observe-only ME3630UsbWatch USB client driver.
 * Uninstall is selected with WATCH_REMOVE; install never unregisters the
 * production ME3630 drivers. */
#include <windows.h>
#undef wsprintfW
typedef BOOL (*INSTALL)(LPCWSTR);
typedef BOOL (*UNINSTALL)(void);
void mainCRTStartup(void)
{
    HMODULE dll=LoadLibrary(L"ME3630UsbWatch.dll");
    DWORD error=dll?0:GetLastError(),bytes;
    BOOL ok=FALSE;
    HANDLE file;
    WCHAR text[256];
    INSTALL install;
    UNINSTALL uninstall;
    int n;
    if(dll) {
        uninstall=(UNINSTALL)GetProcAddressW(dll,L"USBUnInstallDriver");
        install=(INSTALL)GetProcAddressW(dll,L"USBInstallDriver");
#ifdef WATCH_REMOVE
        if(uninstall)ok=uninstall();
#else
        if(install)ok=install(L"ME3630UsbWatch.dll");
#endif
        if(!ok)error=GetLastError();
        FreeLibrary(dll);
    }
#ifdef WATCH_REMOVE
    file=CreateFile(L"\\Temp\\ME3630-watch-uninstall.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
#else
    file=CreateFile(L"\\Temp\\ME3630-watch-install.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
#endif
    if(file!=INVALID_HANDLE_VALUE){n=wsprintfW(text,L"dll_load=%lu installed=%lu error=%lu COMPLETE\r\n",dll!=NULL,ok,error);WriteFile(file,text,n*sizeof(WCHAR),&bytes,NULL);CloseHandle(file);}
    ExitProcess(ok?0:1);
}
