/* Bounded, read-only AT/power diagnostics. Holds a COM handle until actual I/O
 * failure, then closes/reopens: also exercises the removed-handle lifetime. */
#include <windows.h>
#include <pm.h>
#include <stdarg.h>
#undef wsprintfW
#undef wvsprintfW
static HANDLE logFile;
static void emit(LPCWSTR fmt,...)
{
 WCHAR text[512];DWORD bytes;int n;va_list args;
 va_start(args,fmt);n=wvsprintfW(text,fmt,args);va_end(args);
 WriteFile(logFile,text,n*2,&bytes,NULL);FlushFileBuffers(logFile);
}
static BOOL contains(const BYTE *buf,DWORD count,const char *s)
{
 DWORD i,j,len=0;while(s[len])len++;
 for(i=0;i+len<=count;i++){for(j=0;j<len && buf[i+j]==(BYTE)s[j];j++);if(j==len)return TRUE;}return FALSE;
}
static BOOL query(HANDLE port,const char *cmd,DWORD generation,DWORD *success)
{
 BYTE response[256];DWORD count=0,n,written,error,start=GetTickCount(),len=0;BOOL ok;
 while(cmd[len])len++;
 SetLastError(0);ok=WriteFile(port,cmd,len,&written,NULL);error=GetLastError();
 if(!ok || written!=len){emit(L"WRITE_FAIL tick=%lu gen=%lu error=%lu bytes=%lu\r\n",GetTickCount(),generation,error,written);return FALSE;}
 while(count<sizeof(response) && GetTickCount()-start<1500) {
  ok=ReadFile(port,response+count,sizeof(response)-count,&n,NULL);error=GetLastError();
  if(!ok){emit(L"READ_FAIL tick=%lu gen=%lu error=%lu\r\n",GetTickCount(),generation,error);return FALSE;}
  count+=n;if(contains(response,count,"\r\nOK\r\n") || contains(response,count,"ERROR"))break;
 }
 ok=contains(response,count,"\r\nOK\r\n");if(ok)(*success)++;
 /* Publish only derived status; never log SIM/device identifiers or raw URCs. */
 emit(L"QUERY tick=%lu gen=%lu kind=%lu ok=%lu bytes=%lu elapsed=%lu\r\n",GetTickCount(),generation,len==3?0:(cmd[4]=='F'?1:2),ok,count,GetTickCount()-start);
 if(contains(response,count,"+CFUN: 1"))emit(L"CFUN full_functionality\r\n");
 else if(contains(response,count,"+CFUN: 0"))emit(L"CFUN minimum_functionality\r\n");
 if(contains(response,count,"+CPIN: READY"))emit(L"CPIN READY\r\n");
 else if(contains(response,count,"SIM not inserted"))emit(L"CPIN SIM_NOT_INSERTED\r\n");
 return TRUE;
}
void mainCRTStartup(void)
{
 HANDLE port=INVALID_HANDLE_VALUE;DWORD begin=GetTickCount(),now,lastQuery=0,lastPower=0,gen=0,success=0,losses=0,flags,err;
 WCHAR stateName[64];COMMTIMEOUTS timeouts;
 logFile=CreateFile(L"\\Temp\\ME3630-stability.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 if(logFile==INVALID_HANDLE_VALUE)ExitProcess(1);
 emit(L"START tick=%lu duration=90s port=COM1\r\n",begin);
 while((now=GetTickCount())-begin<90000) {
  SystemIdleTimerReset();
  if(now-lastPower>=5000){stateName[0]=0;flags=0;err=GetSystemPowerState(stateName,64,&flags);emit(L"POWER tick=%lu state=%s flags=%08lx error=%lu\r\n",now,stateName,flags,err);lastPower=now;}
  if(port==INVALID_HANDLE_VALUE) {
   port=CreateFile(L"COM1:",GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
   if(port!=INVALID_HANDLE_VALUE) {
    gen++;emit(L"OPEN tick=%lu gen=%lu\r\n",GetTickCount(),gen);
    memset(&timeouts,0,sizeof(timeouts));timeouts.ReadIntervalTimeout=MAXDWORD;timeouts.ReadTotalTimeoutMultiplier=MAXDWORD;
    timeouts.ReadTotalTimeoutConstant=500;timeouts.WriteTotalTimeoutConstant=1000;
    if(!SetCommTimeouts(port,&timeouts) || !query(port,"AT+CFUN?\r",gen,&success) || !query(port,"AT+CPIN?\r",gen,&success)) {
     emit(L"CLOSE_AFTER_INIT_FAILURE tick=%lu gen=%lu error=%lu\r\n",GetTickCount(),gen,GetLastError());CloseHandle(port);port=INVALID_HANDLE_VALUE;losses++;
    }
    lastQuery=0;
   }
  } else if(now-lastQuery>=2000) {
   if(!query(port,"AT\r",gen,&success)){CloseHandle(port);port=INVALID_HANDLE_VALUE;losses++;emit(L"CLOSE_AFTER_IO_FAILURE tick=%lu gen=%lu\r\n",GetTickCount(),gen);}
   lastQuery=now;
  }
  Sleep(100);
 }
 if(port!=INVALID_HANDLE_VALUE)CloseHandle(port);
 emit(L"COMPLETE elapsed=%lu generations=%lu successful_queries=%lu losses=%lu\r\n",GetTickCount()-begin,gen,success,losses);
 CloseHandle(logFile);ExitProcess(0);
}
