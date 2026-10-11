typedef struct {HANDLE port,ready;BOOL ok;DWORD mask,error,elapsed;} EVENTJOB;
static DWORD WINAPI event_reader(LPVOID arg)
{
 EVENTJOB *j=(EVENTJOB*)arg;DWORD start=GetTickCount();SetEvent(j->ready);
 j->ok=WaitCommEvent(j->port,&j->mask,NULL);j->error=GetLastError();j->elapsed=GetTickCount()-start;return 0;
}
static void event_join(HANDLE log,HANDLE thread,EVENTJOB *j)
{
 if(WaitForSingleObject(thread,5000)!=WAIT_OBJECT_0){SetCommMask(j->port,0);if(WaitForSingleObject(thread,2000)!=WAIT_OBJECT_0){check(log,L"Event thread stuck",FALSE);FlushFileBuffers(log);ExitProcess(2);}}
 CloseHandle(thread);CloseHandle(j->ready);
}
static HANDLE event_start(EVENTJOB *j,HANDLE port)
{
 HANDLE thread;memset(j,0,sizeof(*j));j->port=port;j->ready=CreateEvent(NULL,TRUE,FALSE,NULL);
 thread=j->ready?CreateThread(NULL,0,event_reader,j,0,NULL):NULL;
 if(thread){WaitForSingleObject(j->ready,2000);Sleep(200);}else if(j->ready)CloseHandle(j->ready);
 return thread;
}
static void event_test(HANDLE log,HANDLE port)
{
 DWORD mask,bytes,start,elapsed,n;BOOL ok;EVENTJOB j;HANDLE thread;BYTE data[512];WCHAR line[160];
 check(log,L"Set RXCHAR mask",SetCommMask(port,EV_RXCHAR));
 mask=0;ok=GetCommMask(port,&mask);check(log,L"Get RXCHAR mask",ok && mask==EV_RXCHAR);
 ok=SetCommMask(port,EV_CTS);mask=0;
 {DWORD error=GetLastError();BOOL got=GetCommMask(port,&mask);check(log,L"Unsupported mask preserves state",!ok && error==ERROR_NOT_SUPPORTED && got && mask==EV_RXCHAR);}
 thread=event_start(&j,port);ok=FALSE;
 if(thread){mask=MAXDWORD;ok=!WaitCommEvent(port,&mask,NULL) && GetLastError()==ERROR_BUSY && mask==0;}
 check(log,L"RX waiter rejects duplicate wait",thread!=NULL && ok);
 if(!thread)return;
 ok=WriteFile(port,"AT\r",3,&bytes,NULL);check(log,L"AT write during event wait",ok && bytes==3);
 event_join(log,thread,&j);check(log,L"RXCHAR event received",j.ok && j.mask==EV_RXCHAR && j.elapsed>=150 && j.elapsed<2000);
 memset(data,0,sizeof(data));ok=ReadFile(port,data,sizeof(data),&bytes,NULL);
 check(log,L"Event preserves AT response",ok && bytes==6 && memcmp(data,"\r\nOK\r\n",6)==0);
 n=wsprintfW(line,L"EVENT_RX elapsed=%lu mask=%08lx error=%lu bytes=%lu\r\n",j.elapsed,j.mask,j.error,bytes);WriteFile(log,line,n*2,&bytes,NULL);
 check(log,L"Reset RX mask before cancel",SetCommMask(port,EV_RXCHAR));
 thread=event_start(&j,port);check(log,L"Cancel event waiter thread",thread!=NULL);
 if(!thread)return;
 start=GetTickCount();ok=SetCommMask(port,0);elapsed=GetTickCount()-start;
 check(log,L"Mask cancel not blocked by waiter",ok && elapsed<500);
 event_join(log,thread,&j);check(log,L"Mask change returns zero event",j.ok && j.mask==0 && j.elapsed>=150 && j.elapsed<1500);
 mask=MAXDWORD;ok=GetCommMask(port,&mask);start=GetTickCount();
 if(ok && mask==0){mask=MAXDWORD;ok=WaitCommEvent(port,&mask,NULL);}
 check(log,L"Zero mask wait returns immediately",ok && mask==0 && GetTickCount()-start<500);
 n=wsprintfW(line,L"EVENT_CANCEL set_ms=%lu wait_ms=%lu mask=%08lx error=%lu\r\n",elapsed,j.elapsed,j.mask,j.error);WriteFile(log,line,n*2,&bytes,NULL);
}
