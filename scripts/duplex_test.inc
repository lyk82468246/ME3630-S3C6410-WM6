/* Exercise an outstanding synchronous reader and a second writer on the same
 * handle. Only AT is sent. No dial, modem configuration or device IDs logged. */
typedef struct {HANDLE port,ready;BOOL ok;DWORD count,elapsed,error;BYTE data[512];} DUPLEX;
static DWORD WINAPI duplex_reader(LPVOID arg)
{
 DUPLEX *d=(DUPLEX*)arg;DWORD begin=GetTickCount(),n,k;
 SetEvent(d->ready);
 while(d->count<sizeof(d->data) && GetTickCount()-begin<3000) {
  if(!ReadFile(d->port,d->data+d->count,sizeof(d->data)-d->count,&n,NULL)){d->error=GetLastError();break;}
  d->count+=n;
  for(k=0;k+6<=d->count;k++)if(memcmp(d->data+k,"\r\nOK\r\n",6)==0){d->ok=TRUE;break;}
  if(d->ok)break;
 }
 d->elapsed=GetTickCount()-begin;return 0;
}
static void duplex_test(HANDLE log,HANDLE port)
{
 DUPLEX d;HANDLE thread;COMMTIMEOUTS old,t,got;DWORD bytes,start,writeElapsed,configElapsed,n;BOOL ok;WCHAR line[160];
 memset(&d,0,sizeof(d));d.port=port;
 if(!GetCommTimeouts(port,&old)){check(log,L"Duplex save timeouts",FALSE);return;}
 t=old;t.ReadIntervalTimeout=MAXDWORD;t.ReadTotalTimeoutMultiplier=MAXDWORD;t.ReadTotalTimeoutConstant=2000;t.WriteTotalTimeoutConstant=1000;
 check(log,L"Duplex set timeouts",SetCommTimeouts(port,&t));
 d.ready=CreateEvent(NULL,TRUE,FALSE,NULL);
 thread=d.ready?CreateThread(NULL,0,duplex_reader,&d,0,NULL):NULL;
 check(log,L"Duplex reader thread",thread!=NULL);
 if(!thread){if(d.ready)CloseHandle(d.ready);SetCommTimeouts(port,&old);return;}
 WaitForSingleObject(d.ready,2000);Sleep(200);
 start=GetTickCount();ok=GetCommTimeouts(port,&got);configElapsed=GetTickCount()-start;
 check(log,L"Configuration not blocked by reader",ok && configElapsed<500 && memcmp(&t,&got,sizeof(t))==0);
 start=GetTickCount();ok=WriteFile(port,"AT\r",3,&bytes,NULL);writeElapsed=GetTickCount()-start;
 check(log,L"Writer not blocked by reader",ok && bytes==3 && writeElapsed<1000);
 /* Never leave a thread referencing a returned stack frame. Driver read waits
  * are bounded; if a regression hangs them, terminate only this test process. */
 if(WaitForSingleObject(thread,10000)!=WAIT_OBJECT_0){check(log,L"Duplex reader stuck",FALSE);FlushFileBuffers(log);ExitProcess(2);}
 check(log,L"Concurrent reader received AT OK",d.ok && d.elapsed>=150 && d.elapsed<1500);
 n=wsprintfW(line,L"DUPLEX config_ms=%lu write_ms=%lu read_ms=%lu bytes=%lu read_error=%lu\r\n",configElapsed,writeElapsed,d.elapsed,d.count,d.error);
 WriteFile(log,line,n*2,&bytes,NULL);
 CloseHandle(thread);CloseHandle(d.ready);
 check(log,L"Duplex restore timeouts",SetCommTimeouts(port,&old));
}
