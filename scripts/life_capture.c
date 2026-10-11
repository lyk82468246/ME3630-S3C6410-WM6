/* Independent observer keeps named memory alive; no USB/COM/RAS writes. */
#include <windows.h>
#include "me3630_life.h"
#include <stdarg.h>
#undef wvsprintfW
static HANDLE logFile;
static void emit(LPCWSTR format,...){WCHAR s[512];DWORD n,bytes;va_list args;va_start(args,format);n=wvsprintfW(s,format,args);va_end(args);WriteFile(logFile,s,n*2,&bytes,NULL);
#ifndef COM_DIAG38
FlushFileBuffers(logFile);
#endif
}
void mainCRTStartup(void)
{
 HANDLE handle=NULL;M37_MAP *m;M37_RECORD r;DWORD start=GetTickCount(),duration=wcsstr(GetCommandLineW(),L"--watch")?90000:0;LONG seq,end,previous=0,lower;
#ifdef COM_DIAG38
 M37_RECORD snapshot[M37_CAP];BOOL valid[M37_CAP];LONG slot;
#endif
#ifdef COM_DIAG39
 logFile=CreateFile(L"\\Temp\\ME3630-life-capture-d39.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
#elif defined(COM_DIAG38)
 logFile=CreateFile(L"\\Temp\\ME3630-life-capture-d38.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
#else
 logFile=CreateFile(L"\\Temp\\ME3630-life-capture.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
#endif
 if(logFile==INVALID_HANDLE_VALUE)ExitProcess(2);
 emit(L"LIFE_CAPTURE start_tick=%lu duration=%lu\r\n",start,duration);m=m37_open(M37_NAME,&handle);
 if(!m){emit(L"MAP_ERROR error=%lu\r\n",GetLastError());goto done;}
 do {
  end=InterlockedCompareExchange(&m->next,0,0);lower=previous+1;
  if(end-lower>=M37_CAP){emit(L"OVERWRITTEN count=%ld\r\n",end-lower-M37_CAP+1);lower=end-M37_CAP+1;}
#ifdef COM_DIAG38
  /* Copy the whole available batch before slow file formatting/writes. */
  for(seq=lower;seq<=end;seq++)valid[seq-lower]=m37_get(m,seq,&snapshot[seq-lower]);
#endif
  for(seq=lower;seq<=end;seq++){
#ifdef COM_DIAG38
   slot=seq-lower;if(valid[slot]){r=snapshot[slot];
    emit(L"seq=%ld tick=%lu pid=%lu tid=%lu event=%s object=%08lx value=%08lx error=%lu\r\n",seq,r.tick,r.pid,r.tid,m37_event_name(r.event),r.object,r.value,r.error);
   }else emit(L"SKIP seq=%ld busy_dropped_or_overwritten=1\r\n",seq);
#else
   if(m37_get(m,seq,&r))emit(L"seq=%ld tick=%lu pid=%lu tid=%lu event=%s object=%08lx value=%08lx error=%lu\r\n",seq,r.tick,r.pid,r.tid,m37_event_name(r.event),r.object,r.value,r.error);
   else emit(L"SKIP seq=%ld busy_dropped_or_overwritten=1\r\n",seq);
#endif
  }
#ifdef COM_DIAG38
  previous=end;FlushFileBuffers(logFile);
  if(!duration || GetTickCount()-start>=duration)break;
  Sleep(200);
#else
  previous=end;if(duration)Sleep(200);
#endif
 }while(
#ifdef COM_DIAG38
 TRUE
#else
 GetTickCount()-start<duration
#endif
 );
 emit(L"SUMMARY next=%ld dropped=%ld\r\n",InterlockedCompareExchange(&m->next,0,0),InterlockedCompareExchange(&m->dropped,0,0));m37_close(m,handle);
done:
 emit(L"LIFE_CAPTURE COMPLETE tick=%lu elapsed=%lu\r\n",GetTickCount(),GetTickCount()-start);FlushFileBuffers(logFile);CloseHandle(logFile);ExitProcess(0);
}
