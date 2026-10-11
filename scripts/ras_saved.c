/* Inspect only ME3630-named saved entries; optional single guarded dial.
 * Never reads passwords, changes/deletes entries, or dials Cellular Line. */
#include <windows.h>
#include <ras.h>
#include <raserror.h>
#include <tapi.h>
#include <stdarg.h>
#undef wsprintfW
#undef wvsprintfW
static HANDLE logFile;static LONG done,cbError;
static void emit(LPCWSTR fmt,...)
{
 WCHAR text[1024];DWORD bytes;int n;va_list ap;va_start(ap,fmt);n=wvsprintfW(text,fmt,ap);va_end(ap);
 WriteFile(logFile,text,n*2,&bytes,NULL);FlushFileBuffers(logFile);
}
static LRESULT CALLBACK notify(HWND window,UINT msg,WPARAM state,LPARAM error)
{
 if(msg!=WM_RASDIALEVENT)return DefWindowProc(window,msg,state,error);
 if(error)InterlockedExchange(&cbError,error);
 if(error || state==RASCS_Connected || state==RASCS_Disconnected)InterlockedExchange(&done,1);
 return 0;
}
static void tapi_probe(void)
{
 HLINEAPP app=NULL;HLINE line=NULL;LINEINITIALIZEEXPARAMS init;LINEEXTENSIONID ext;
 LINEDEVCAPS *caps;VARSTRING *config,*id;DWORD count=0,version=0x20000,api,i,needed;LONG error;WCHAR name[256];
 memset(&init,0,sizeof(init));init.dwTotalSize=sizeof(init);init.dwOptions=LINEINITIALIZEEXOPTION_USEEVENT;
 error=lineInitializeEx(&app,GetModuleHandle(NULL),NULL,L"ME3630 scoped diagnostic",&count,&version,&init);
 emit(L"TAPI_INIT error=%08lx count=%lu api=%08lx\r\n",error,count,version);if(error)return;
 for(i=0;i<count && i<64;i++) {
  error=lineNegotiateAPIVersion(app,i,0x10004,0x20000,&api,&ext);if(error)continue;
  caps=(LINEDEVCAPS*)LocalAlloc(LPTR,16384);if(!caps)break;caps->dwTotalSize=16384;
  error=lineGetDevCaps(app,i,api,0,caps);name[0]=0;
  if(!error && caps->dwLineNameSize>=2 && caps->dwLineNameOffset<=16384 && caps->dwLineNameSize<=16384-caps->dwLineNameOffset) {
   needed=caps->dwLineNameSize/2;if(needed>255)needed=255;memcpy(name,(BYTE*)caps+caps->dwLineNameOffset,needed*2);name[needed]=0;
  }
  if(!wcsstr(name,L"ME3630")){LocalFree(caps);continue;}
  emit(L"TAPI_OWN id=%lu name=%s media=%08lx features=%08lx caps=%08lx\r\n",i,name,caps->dwMediaModes,caps->dwLineFeatures,caps->dwDevCapFlags);
  LocalFree(caps);
  config=(VARSTRING*)LocalAlloc(LPTR,4096);if(!config)break;config->dwTotalSize=4096;
  error=lineGetDevConfig(i,config,L"comm/datamodem");emit(L"TAPI_CONFIG error=%08lx needed=%lu bytes=%lu format=%lu\r\n",error,config->dwNeededSize,config->dwStringSize,config->dwStringFormat);LocalFree(config);
  /* Monitor only: no lineMakeCall, no configuration mutation. */
  error=lineOpen(app,i,&line,api,0,0,LINECALLPRIVILEGE_MONITOR,LINEMEDIAMODE_DATAMODEM,NULL);emit(L"TAPI_OPEN_MONITOR error=%08lx\r\n",error);
  if(!error){
   id=(VARSTRING*)LocalAlloc(LPTR,4096);
   if(id){id->dwTotalSize=4096;error=lineGetID(line,0,NULL,LINECALLSELECT_LINE,id,L"comm/datamodem/portname");emit(L"TAPI_PORTNAME error=%08lx bytes=%lu format=%lu\r\n",error,id->dwStringSize,id->dwStringFormat);
    if(!error && id->dwStringOffset<=4096 && id->dwStringSize<=4096-id->dwStringOffset && id->dwStringSize<=128 && id->dwStringSize>=2 && id->dwStringFormat==STRINGFORMAT_UNICODE){memcpy(name,(BYTE*)id+id->dwStringOffset,id->dwStringSize);name[id->dwStringSize/2]=0;emit(L"TAPI_PORT value=%s\r\n",name);}LocalFree(id);
   }
   lineClose(line);line=NULL;
  }
 }
 lineShutdown(app);
}
void mainCRTStartup(void)
{
 RASENTRYNAME *names;RASENTRY *entry;RASDIALPARAMS params;RASCONNSTATUS status;RASPPPIP ip;
 DWORD bytes,count=0,error,i,entryBytes,configBytes,start=GetTickCount(),matches=0,actual,version;
 BYTE config[4096];WCHAR selected[RAS_MaxEntryName+1];BOOL dial=wcsstr(GetCommandLineW(),L"--dial")!=NULL,connected=FALSE,require38=wcsstr(GetCommandLineW(),L"--require-d38")!=NULL;
 HRASCONN connection=NULL;HANDLE port;HWND window=NULL;WNDCLASS wc;MSG msg;
 logFile=CreateFile(L"\\Temp\\ME3630-ras-saved.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(logFile==INVALID_HANDLE_VALUE)ExitProcess(2);
 emit(L"SAVED_START tick=%lu dial=%lu\r\n",start,dial);
 if(!dial)tapi_probe();
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
 {DWORD diagnostic=0,diagBytes=0;BOOL identified=DeviceIoControl(port,0x00223ff4,NULL,0,&diagnostic,4,&diagBytes,NULL) && diagBytes==4;
  if(identified)emit(L"PORT_DIAGNOSTIC build=%lu\r\n",diagnostic);
  if(require38 && (!identified || diagnostic!=38)){CloseHandle(port);emit(L"PORT_DIAGNOSTIC_REJECTED expected=38 actual=%lu identified=%lu\r\n",diagnostic,identified);goto finish;}
 }
 CloseHandle(port);emit(L"PORT_VERIFIED version=%lu\r\n",version);
 memset(&params,0,sizeof(params));params.dwSize=sizeof(params);lstrcpyW(params.szEntryName,selected);
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=notify;wc.hInstance=GetModuleHandle(NULL);wc.lpszClassName=L"ME3630RasNotify";
 if(!RegisterClass(&wc)){emit(L"WINDOW_CLASS_ERROR=%lu\r\n",GetLastError());goto finish;}
 window=CreateWindow(wc.lpszClassName,L"",WS_POPUP,0,0,0,0,NULL,NULL,wc.hInstance,NULL);
 if(!window){emit(L"WINDOW_ERROR=%lu\r\n",GetLastError());goto finish;}
 /* CE5 supports HWND notification, not desktop RASDIALFUNC callbacks. */
 error=RasDial(NULL,NULL,&params,0xffffffff,(LPVOID)window,&connection);emit(L"DIAL_START error=%lu handle=%08lx\r\n",error,(DWORD)connection);
 if(!error && connection)do {
  while(PeekMessage(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);}
  memset(&status,0,sizeof(status));status.dwSize=sizeof(status);error=RasGetConnectStatus(connection,&status);
  emit(L"STATE query_error=%lu state=%lu state_error=%lu\r\n",error,status.rasconnstate,status.dwError);
  if(!error && status.rasconnstate==RASCS_Connected){connected=TRUE;break;}
  if(error || status.dwError || InterlockedCompareExchange(&done,0,0))break;Sleep(500);
 }while(GetTickCount()-start<45000);
 emit(L"CALLBACK error=%lu\r\n",InterlockedCompareExchange(&cbError,0,0));
 if(connected){memset(&ip,0,sizeof(ip));ip.dwSize=sizeof(ip);bytes=sizeof(ip);error=RasGetProjectionInfo(connection,RASP_PppIp,&ip,&bytes);emit(L"PPP_IP error=%lu projection_error=%lu\r\n",error,ip.dwError);if(!error && !ip.dwError)emit(L"PPP_ADDRESS ip=%s\r\n",ip.szIpAddress);else connected=FALSE;}
 if(connection){emit(L"HANGUP_OWN error=%lu\r\n",RasHangUp(connection));Sleep(500);}
finish:
 if(window)DestroyWindow(window);
 emit(L"SAVED_COMPLETE tick=%lu connected=%lu elapsed=%lu\r\n",GetTickCount(),connected,GetTickCount()-start);CloseHandle(logFile);ExitProcess(0);
}
