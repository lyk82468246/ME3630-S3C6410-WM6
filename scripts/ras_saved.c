/* Inspect only ME3630-named saved entries; optional single guarded dial.
 * Never reads passwords, changes/deletes entries, or dials Cellular Line. */
#include <windows.h>
#include <ras.h>
#include <raserror.h>
#include <stdarg.h>
#undef wsprintfW
#undef wvsprintfW
static HANDLE logFile;static LONG done,cbError;
static void emit(LPCWSTR fmt,...)
{
 WCHAR text[1024];DWORD bytes;int n;va_list ap;va_start(ap,fmt);n=wvsprintfW(text,fmt,ap);va_end(ap);
 WriteFile(logFile,text,n*2,&bytes,NULL);FlushFileBuffers(logFile);
}
static void WINAPI notify(UINT msg,RASCONNSTATE state,DWORD error)
{
 if(error)InterlockedExchange(&cbError,error);
 if(error || state==RASCS_Connected || state==RASCS_Disconnected)InterlockedExchange(&done,1);
}
void mainCRTStartup(void)
{
 RASENTRYNAME *names;RASENTRY *entry;RASDIALPARAMS params;RASCONNSTATUS status;RASPPPIP ip;
 DWORD bytes,count=0,error,i,entryBytes,configBytes,start=GetTickCount(),matches=0,actual,version;
 BYTE config[4096];WCHAR selected[RAS_MaxEntryName+1];BOOL dial=wcsstr(GetCommandLineW(),L"--dial")!=NULL,connected=FALSE;
 HRASCONN connection=NULL;HANDLE port;
 logFile=CreateFile(L"\\Temp\\ME3630-ras-saved.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(logFile==INVALID_HANDLE_VALUE)ExitProcess(2);
 emit(L"SAVED_START tick=%lu dial=%lu\r\n",start,dial);
 bytes=sizeof(RASENTRYNAME);names=(RASENTRYNAME*)LocalAlloc(LPTR,bytes);if(!names)goto finish;
 names[0].dwSize=sizeof(RASENTRYNAME);error=RasEnumEntries(NULL,NULL,names,&bytes,&count);
 if(error==ERROR_BUFFER_TOO_SMALL && bytes<=65536){LocalFree(names);names=(RASENTRYNAME*)LocalAlloc(LPTR,bytes);if(!names)goto finish;names[0].dwSize=sizeof(RASENTRYNAME);error=RasEnumEntries(NULL,NULL,names,&bytes,&count);}
 emit(L"ENUM_ENTRIES error=%lu count=%lu bytes=%lu\r\n",error,count,bytes);
 if(!error && count<=bytes/sizeof(RASENTRYNAME))for(i=0;i<count;i++) {
  if(!wcsstr(names[i].szEntryName,L"ME3630"))continue;
  entryBytes=16384;entry=(RASENTRY*)LocalAlloc(LPTR,entryBytes);if(!entry)break;entry->dwSize=sizeof(RASENTRY);configBytes=sizeof(config);
  error=RasGetEntryProperties(NULL,names[i].szEntryName,entry,&entryBytes,config,&configBytes);
  emit(L"ENTRY name=%s error=%lu entry_bytes=%lu config_bytes=%lu\r\n",names[i].szEntryName,error,entryBytes,configBytes);
  if(!error) {
   emit(L"DEVICE type=%s name=%s phone=%s framing=%lu protocols=%lu options=%08lx\r\n",entry->szDeviceType,entry->szDeviceName,entry->szLocalPhoneNumber,entry->dwFramingProtocol,entry->dwfNetProtocols,entry->dwfOptions);
   if(!lstrcmpW(entry->szDeviceType,RASDT_Modem) && !lstrcmpW(entry->szDeviceName,L"ME3630 Test COM5") && !lstrcmpW(entry->szLocalPhoneNumber,L"*99***2#") && entry->dwFramingProtocol==RASFP_Ppp && (entry->dwfNetProtocols&RASNP_Ip)) {
    matches++;lstrcpyW(selected,names[i].szEntryName);
   }
  }
  LocalFree(entry);
 }
 LocalFree(names);emit(L"GUARDED_MATCHES count=%lu\r\n",matches);
 if(!dial || matches!=1)goto finish;
 /* Ensure COM5 is our current driver, not a reallocated LeadCore port. */
 do {
  port=CreateFile(L"COM5:",GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
  if(port!=INVALID_HANDLE_VALUE)break;
  Sleep(500);
 }while(GetTickCount()-start<30000);
 if(port==INVALID_HANDLE_VALUE){emit(L"PORT_UNAVAILABLE error=%lu\r\n",GetLastError());goto finish;}
 version=actual=0;
 if(!DeviceIoControl(port,0x00223ffc,NULL,0,&version,4,&actual,NULL) || version!=18 || actual!=4){CloseHandle(port);emit(L"PORT_IDENTITY_REJECTED version=%lu bytes=%lu\r\n",version,actual);goto finish;}
 CloseHandle(port);emit(L"PORT_VERIFIED version=%lu\r\n",version);
 memset(&params,0,sizeof(params));params.dwSize=sizeof(params);lstrcpyW(params.szEntryName,selected);
 error=RasDial(NULL,NULL,&params,0,(LPVOID)notify,&connection);emit(L"DIAL_START error=%lu handle=%08lx\r\n",error,(DWORD)connection);
 if(!error && connection)do {
  memset(&status,0,sizeof(status));status.dwSize=sizeof(status);error=RasGetConnectStatus(connection,&status);
  emit(L"STATE query_error=%lu state=%lu state_error=%lu\r\n",error,status.rasconnstate,status.dwError);
  if(!error && status.rasconnstate==RASCS_Connected){connected=TRUE;break;}
  if(error || status.dwError || InterlockedCompareExchange(&done,0,0))break;Sleep(500);
 }while(GetTickCount()-start<45000);
 emit(L"CALLBACK error=%lu\r\n",InterlockedCompareExchange(&cbError,0,0));
 if(connected){memset(&ip,0,sizeof(ip));ip.dwSize=sizeof(ip);bytes=sizeof(ip);error=RasGetProjectionInfo(connection,RASP_PppIp,&ip,&bytes);emit(L"PPP_IP error=%lu projection_error=%lu\r\n",error,ip.dwError);if(!error && !ip.dwError)emit(L"PPP_ADDRESS ip=%s\r\n",ip.szIpAddress);else connected=FALSE;}
 if(connection){emit(L"HANGUP_OWN error=%lu\r\n",RasHangUp(connection));Sleep(500);}
finish:
 emit(L"SAVED_COMPLETE tick=%lu connected=%lu elapsed=%lu\r\n",GetTickCount(),connected,GetTickCount()-start);CloseHandle(logFile);ExitProcess(0);
}
