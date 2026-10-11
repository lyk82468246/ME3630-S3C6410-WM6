/* Change ONLY our existing exact USB match. Preserve V18 Unimodem keys. */
#include <windows.h>
#undef wsprintfW
#ifdef COM_DIAG36
#define DIAG_DLL L"ME3630ComV18D36.dll"
#define DIAG_LOG L"\\Temp\\ME3630-diag36-install.txt"
#define DIAG_NUMBER 36
#elif defined(COM_DIAG35)
#define DIAG_DLL L"ME3630ComV18D35.dll"
#define DIAG_LOG L"\\Temp\\ME3630-diag35-install.txt"
#define DIAG_NUMBER 35
#elif defined(COM_DIAG34R2)
#define DIAG_DLL L"ME3630ComV18D34R2.dll"
#define DIAG_LOG L"\\Temp\\ME3630-diag34R2-install.txt"
#define DIAG_NUMBER 3402
#elif defined(COM_DIAG34)
#define DIAG_DLL L"ME3630ComV18D34.dll"
#define DIAG_LOG L"\\Temp\\ME3630-diag34-install.txt"
#define DIAG_NUMBER 34
#else
#define DIAG_DLL L"ME3630ComV18D33.dll"
#define DIAG_LOG L"\\Temp\\ME3630-diag33-install.txt"
#define DIAG_NUMBER 33
#endif
void mainCRTStartup(void)
{
 HKEY key;DWORD error,bytes,type,size;WCHAR prior[96],text[220];HANDLE log;HMODULE module;
 module=LoadLibrary(DIAG_DLL);error=module?0:GetLastError();
 if(module){FreeLibrary(module);
  error=RegOpenKeyEx(HKEY_LOCAL_MACHINE,L"Drivers\\USB\\LoadClients\\6610_5170\\Default\\Default\\ME3630Probe",0,0,&key);
  if(!error){size=sizeof(prior);error=RegQueryValueEx(key,L"DLL",NULL,&type,(LPBYTE)prior,&size);
   if(!error && (type!=REG_SZ || size>sizeof(prior) || size<2))error=ERROR_INVALID_DATA;
   prior[95]=0;
   if(!error && lstrcmpiW(prior,L"ME3630ComV18.dll") && lstrcmpiW(prior,L"ME3630ComV18D33.dll") && lstrcmpiW(prior,L"ME3630ComV18D34.dll") && lstrcmpiW(prior,L"ME3630ComV18D34R2.dll") && lstrcmpiW(prior,L"ME3630ComV18D35.dll") && lstrcmpiW(prior,DIAG_DLL))error=ERROR_INVALID_DATA;
   if(!error)error=RegSetValueEx(key,L"DLL",0,REG_SZ,(LPBYTE)DIAG_DLL,sizeof(DIAG_DLL));
   RegCloseKey(key);
  }
 }
 log=CreateFile(DIAG_LOG,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 if(log!=INVALID_HANDLE_VALUE){DWORD n=wsprintfW(text,L"D%lu_INSTALL tick=%lu result=%lu next_attach_only=1 COMPLETE\r\n",DIAG_NUMBER,GetTickCount(),error);WriteFile(log,text,n*2,&bytes,NULL);CloseHandle(log);}
 ExitProcess(error?1:0);
}
