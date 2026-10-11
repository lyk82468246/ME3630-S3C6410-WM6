#include <windows.h>
#undef wsprintfW
typedef BOOL (*INSTALL)(LPCWSTR);
typedef BOOL (*UNINSTALL)(void);
#ifdef COM_V17
#define NEW_DLL L"ME3630ComV17.dll"
#define OLD_DLL L"ME3630ComV16.dll"
#define RESULT_PATH L"\\Temp\\ME3630-install-v17.txt"
#elif defined(COM_V16)
#define NEW_DLL L"ME3630ComV16.dll"
#define OLD_DLL L"ME3630ComV15.dll"
#define RESULT_PATH L"\\Temp\\ME3630-install-v16.txt"
#elif defined(COM_V15)
#define NEW_DLL L"ME3630ComV15.dll"
#define OLD_DLL L"ME3630ComV14.dll"
#define RESULT_PATH L"\\Temp\\ME3630-install-v15.txt"
#elif defined(COM_V14)
#define NEW_DLL L"ME3630ComV14.dll"
#define OLD_DLL L"ME3630ComV13.dll"
#define RESULT_PATH L"\\Temp\\ME3630-install-v14.txt"
#elif defined(COM_V13)
#define NEW_DLL L"ME3630ComV13.dll"
#define OLD_DLL L"ME3630ComV12.dll"
#define RESULT_PATH L"\\Temp\\ME3630-install-v13.txt"
#elif defined(COM_V12)
#define NEW_DLL L"ME3630ComV12.dll"
#define OLD_DLL L"ME3630ComV11.dll"
#define RESULT_PATH L"\\Temp\\ME3630-install-v12.txt"
#elif defined(COM_V11)
#define NEW_DLL L"ME3630ComV11.dll"
#define OLD_DLL L"ME3630ComV10.dll"
#define RESULT_PATH L"\\Temp\\ME3630-install-v11.txt"
#elif defined(COM_V10)
#define NEW_DLL L"ME3630ComV10.dll"
#define OLD_DLL L"ME3630ComV9.dll"
#define RESULT_PATH L"\\Temp\\ME3630-install-v10.txt"
#elif defined(COM_V9)
#define NEW_DLL L"ME3630ComV9.dll"
#define OLD_DLL L"ME3630ComV8.dll"
#define RESULT_PATH L"\\Temp\\ME3630-install-v9.txt"
#elif defined(COM_V8)
#define NEW_DLL L"ME3630ComV8.dll"
#define OLD_DLL L"ME3630ComV7.dll"
#define RESULT_PATH L"\\Temp\\ME3630-install-v8.txt"
#elif defined(COM_V7)
#define NEW_DLL L"ME3630ComV7.dll"
#define OLD_DLL L"ME3630ComV6.dll"
#define RESULT_PATH L"\\Temp\\ME3630-install-v7.txt"
#elif defined(COM_V6)
#define NEW_DLL L"ME3630ComV6.dll"
#define OLD_DLL L"ME3630ComV5.dll"
#define RESULT_PATH L"\\Temp\\ME3630-install-v6.txt"
#elif defined(COM_V5)
#define NEW_DLL L"ME3630ComV5.dll"
#define OLD_DLL L"ME3630ComV4.dll"
#define RESULT_PATH L"\\Temp\\ME3630-install-v5.txt"
#else
#define NEW_DLL L"ME3630ComV4.dll"
#define OLD_DLL L"ME3630StreamV3.dll"
#define RESULT_PATH L"\\Temp\\ME3630-install-v4.txt"
#endif
void mainCRTStartup(void)
{
 HMODULE dll=LoadLibrary(NEW_DLL),old;INSTALL install;UNINSTALL uninstall;
 BOOL ok=FALSE;DWORD error=dll?0:GetLastError(),bytes;HANDLE file;WCHAR text[256];int n;
 if(dll) {
  old=LoadLibrary(OLD_DLL);
  if(old){uninstall=(UNINSTALL)GetProcAddressW(old,L"USBUnInstallDriver");if(uninstall)uninstall();FreeLibrary(old);}
  install=(INSTALL)GetProcAddressW(dll,L"USBInstallDriver");if(install)ok=install(NEW_DLL);
  if(!ok){error=GetLastError();old=LoadLibrary(OLD_DLL);if(old){install=(INSTALL)GetProcAddressW(old,L"USBInstallDriver");if(install)install(OLD_DLL);FreeLibrary(old);}}
  FreeLibrary(dll);
 }
 file=CreateFile(RESULT_PATH,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 if(file!=INVALID_HANDLE_VALUE){n=wsprintfW(text,L"dll_load=%lu installed=%lu error=%lu COMPLETE\r\n",dll!=NULL,ok,error);WriteFile(file,text,n*2,&bytes,NULL);CloseHandle(file);}
 ExitProcess(ok?0:1);
}
