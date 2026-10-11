/* Change ONLY our existing exact USB match. Preserve V18 Unimodem keys. */
#include <windows.h>
#undef wsprintfW
void mainCRTStartup(void)
{
 HKEY key;DWORD error,bytes,type,size;WCHAR prior[96],text[220];HANDLE log;HMODULE module;
 module=LoadLibrary(L"ME3630ComV18D33.dll");error=module?0:GetLastError();
 if(module){FreeLibrary(module);
  error=RegOpenKeyEx(HKEY_LOCAL_MACHINE,L"Drivers\\USB\\LoadClients\\6610_5170\\Default\\Default\\ME3630Probe",0,0,&key);
  if(!error){size=sizeof(prior);error=RegQueryValueEx(key,L"DLL",NULL,&type,(LPBYTE)prior,&size);
   if(!error && (type!=REG_SZ || size>sizeof(prior) || size<2))error=ERROR_INVALID_DATA;
   prior[95]=0;
   if(!error && lstrcmpiW(prior,L"ME3630ComV18.dll") && lstrcmpiW(prior,L"ME3630ComV18D33.dll"))error=ERROR_INVALID_DATA;
   if(!error)error=RegSetValueEx(key,L"DLL",0,REG_SZ,(LPBYTE)L"ME3630ComV18D33.dll",sizeof(L"ME3630ComV18D33.dll"));
   RegCloseKey(key);
  }
 }
 log=CreateFile(L"\\Temp\\ME3630-diag33-install.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 if(log!=INVALID_HANDLE_VALUE){DWORD n=wsprintfW(text,L"D33_INSTALL tick=%lu result=%lu next_attach_only=1 COMPLETE\r\n",GetTickCount(),error);WriteFile(log,text,n*2,&bytes,NULL);CloseHandle(log);}
 ExitProcess(error?1:0);
}
