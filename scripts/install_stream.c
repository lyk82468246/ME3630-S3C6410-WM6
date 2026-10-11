#include <windows.h>
#undef wsprintfW
typedef BOOL (*INSTALL)(LPCWSTR);
typedef BOOL (*UNINSTALL)(void);
void mainCRTStartup(void)
{
    HMODULE dll=LoadLibrary(L"ME3630StreamV3.dll"),old;
    INSTALL install;UNINSTALL uninstall;BOOL ok=FALSE;DWORD error=dll?0:GetLastError(),bytes;
    HANDLE file;WCHAR text[256];int n;
    if(dll) {
        old=LoadLibrary(L"ME3630ProbeV2.dll");
        if(old){uninstall=(UNINSTALL)GetProcAddressW(old,L"USBUnInstallDriver");if(uninstall)uninstall();FreeLibrary(old);}
        install=(INSTALL)GetProcAddressW(dll,L"USBInstallDriver");
        if(install)ok=install(L"ME3630StreamV3.dll");
        if(!ok) {
            error=GetLastError();old=LoadLibrary(L"ME3630ProbeV2.dll");
            if(old){install=(INSTALL)GetProcAddressW(old,L"USBInstallDriver");if(install)install(L"ME3630ProbeV2.dll");FreeLibrary(old);}
        }
        FreeLibrary(dll);
    }
    file=CreateFile(L"\\Temp\\ME3630-install-v3.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    if(file!=INVALID_HANDLE_VALUE){n=wsprintfW(text,L"dll_load=%lu installed=%lu error=%lu COMPLETE\r\n",dll!=NULL,ok,error);WriteFile(file,text,n*sizeof(WCHAR),&bytes,NULL);CloseHandle(file);}
    ExitProcess(ok?0:1);
}
