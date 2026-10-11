/* WM6 thread/queue isolation test. No USB handles, device registration or AT.
 * Uses production pump/read/ring helpers with a deterministic fake transport.
 * This cannot validate USB, stream-manager contexts, or hardware concurrency. */
#include <windows.h>
#undef wsprintfW
typedef struct { HANDLE stop; } PROBE;
typedef struct {
 PROBE *parent; HANDLE in,out,rxReady,eventWake,arrival;
 CRITICAL_SECTION io,rxIO; COMMTIMEOUTS timeouts;
 DWORD index,qHead,qCount,rxFailure,eventMask,eventPending;
 BYTE queue[4096];
#ifdef COM_V15
 DWORD rxAbortEpoch;
#endif
} PORT;
static BOOL serial_alive(PORT *p){return WaitForSingleObject(p->parent->stop,0)!=WAIT_OBJECT_0;}
static void port_release(PORT *p){}
static void logline(LPCWSTR format,...){}
static int serial_transfer(PORT *p,HANDLE pipe,LPVOID buf,DWORD size,BOOL input,DWORD timeout,PDWORD bytes)
{
 HANDLE events[2];DWORD result;static const BYTE reply[]={13,10,'O','K',13,10};
 events[0]=p->parent->stop;events[1]=p->arrival;*bytes=0;
 result=WaitForMultipleObjects(2,events,FALSE,timeout);
 if(result==WAIT_OBJECT_0){SetLastError(ERROR_DEVICE_NOT_CONNECTED);return -1;}
 if(result==WAIT_OBJECT_0+1){memcpy(buf,reply,sizeof(reply));*bytes=sizeof(reply);return 1;}
 if(result==WAIT_TIMEOUT)return 0;
 SetLastError(ERROR_IO_DEVICE);return -1;
}
#include "me3630_ring.inc"
#include "me3630_queue.inc"
typedef struct {PORT *port;HANDLE ready;DWORD result,error,elapsed;BYTE data[16];} READJOB;
static DWORD WINAPI reader(LPVOID arg)
{
 READJOB *job=(READJOB*)arg;DWORD begin=GetTickCount();SetEvent(job->ready);
 job->result=serial_queue_read(job->port,job->data,sizeof(job->data));
 job->error=GetLastError();job->elapsed=GetTickCount()-begin;return 0;
}
static DWORD checks,failures;
static void check(HANDLE log,LPCWSTR name,BOOL ok)
{
 WCHAR line[192];DWORD n,bytes;checks++;if(!ok)failures++;
 n=wsprintfW(line,L"CHECK %s pass=%lu\r\n",name,ok);WriteFile(log,line,n*2,&bytes,NULL);
}
void mainCRTStartup(void)
{
 PORT p;PROBE parent;READJOB job;HANDLE log,pump,thread;DWORD n,begin,bytes;BYTE data[16];WCHAR line[192];
#ifdef COM_V15
 log=CreateFile(L"\\Temp\\ME3630-queue-unit-v15.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
#else
 log=CreateFile(L"\\Temp\\ME3630-queue-unit.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
#endif
 if(log==INVALID_HANDLE_VALUE)ExitProcess(2);
 n=wsprintfW(line,L"QUEUE_UNIT start_tick=%lu transport=FAKE no_usb=1\r\n",GetTickCount());WriteFile(log,line,n*2,&bytes,NULL);
 memset(&p,0,sizeof(p));memset(&job,0,sizeof(job));parent.stop=CreateEvent(NULL,TRUE,FALSE,NULL);
 p.parent=&parent;p.rxReady=CreateEvent(NULL,TRUE,FALSE,NULL);p.eventWake=CreateEvent(NULL,TRUE,FALSE,NULL);
 p.arrival=CreateEvent(NULL,FALSE,FALSE,NULL);job.ready=CreateEvent(NULL,TRUE,FALSE,NULL);job.port=&p;
 InitializeCriticalSection(&p.io);InitializeCriticalSection(&p.rxIO);
 p.timeouts.ReadIntervalTimeout=MAXDWORD;p.timeouts.ReadTotalTimeoutConstant=500;p.eventMask=EV_RXCHAR;
 if(!parent.stop || !p.rxReady || !p.eventWake || !p.arrival || !job.ready)ExitProcess(2);
 pump=CreateThread(NULL,0,serial_rx_pump,&p,0,NULL);check(log,L"Production pump thread starts",pump!=NULL);if(!pump)ExitProcess(2);
 thread=CreateThread(NULL,0,reader,&job,0,NULL);check(log,L"Background reader starts",thread!=NULL);if(!thread)ExitProcess(2);
 WaitForSingleObject(job.ready,1000);Sleep(100);SetEvent(p.arrival);
 if(WaitForSingleObject(thread,2000)!=WAIT_OBJECT_0)ExitProcess(2);
 check(log,L"Pending background reader receives six bytes",job.result==6 && job.error==0 && memcmp(job.data,"\r\nOK\r\n",6)==0 && job.elapsed>=90 && job.elapsed<1000);CloseHandle(thread);
 check(log,L"RX event survives reader consuming bytes",WaitForSingleObject(p.eventWake,0)==WAIT_OBJECT_0 && p.eventPending==EV_RXCHAR);
 SetEvent(p.arrival);if(WaitForSingleObject(p.rxReady,1000)!=WAIT_OBJECT_0)ExitProcess(2);
 n=serial_queue_read(&p,data,1);check(log,L"One-byte read preserves five queued bytes",n==1 && data[0]==13 && p.qCount==5);
 n=serial_queue_read(&p,data+1,5);check(log,L"Remaining response drains intact",n==5 && memcmp(data,"\r\nOK\r\n",6)==0 && p.qCount==0);
 p.timeouts.ReadIntervalTimeout=0;p.timeouts.ReadTotalTimeoutConstant=200;SetEvent(p.arrival);begin=GetTickCount();
 n=serial_queue_read(&p,data,8);check(log,L"Total deadline returns partial six-byte response",n==6 && memcmp(data,"\r\nOK\r\n",6)==0 && GetTickCount()-begin>=150 && GetTickCount()-begin<1000);
 begin=GetTickCount();n=serial_queue_read(&p,data,1);check(log,L"Empty read returns zero at deadline",n==0 && GetTickCount()-begin>=150 && GetTickCount()-begin<1000);
#ifdef COM_V15
 {
  READJOB second;HANDLE firstThread,secondThread;
  memset(&second,0,sizeof(second));second.port=&p;second.ready=CreateEvent(NULL,TRUE,FALSE,NULL);
  p.timeouts.ReadIntervalTimeout=MAXDWORD;p.timeouts.ReadTotalTimeoutConstant=500;
  ResetEvent(job.ready);firstThread=CreateThread(NULL,0,reader,&job,0,NULL);
  if(!firstThread || !second.ready)ExitProcess(2);
  WaitForSingleObject(job.ready,1000);Sleep(50);
  secondThread=CreateThread(NULL,0,reader,&second,0,NULL);if(!secondThread)ExitProcess(2);
  WaitForSingleObject(second.ready,1000);Sleep(100);
  EnterCriticalSection(&p.io);p.rxAbortEpoch++;SetEvent(p.rxReady);LeaveCriticalSection(&p.io);
  if(WaitForSingleObject(firstThread,2000)!=WAIT_OBJECT_0 || WaitForSingleObject(secondThread,2000)!=WAIT_OBJECT_0)ExitProcess(2);
  check(log,L"Abort generation releases active reader",job.result==(DWORD)-1 && job.error==ERROR_OPERATION_ABORTED && job.elapsed<1000);
  check(log,L"Abort generation releases serialized reader",second.result==(DWORD)-1 && second.error==ERROR_OPERATION_ABORTED && second.elapsed<1000);
  CloseHandle(firstThread);CloseHandle(secondThread);CloseHandle(second.ready);
  ResetEvent(job.ready);thread=CreateThread(NULL,0,reader,&job,0,NULL);if(!thread)ExitProcess(2);
  WaitForSingleObject(job.ready,1000);Sleep(50);SetEvent(p.arrival);
  if(WaitForSingleObject(thread,2000)!=WAIT_OBJECT_0)ExitProcess(2);
  check(log,L"New generation reader receives complete reply",job.result==6 && job.error==0 && memcmp(job.data,"\r\nOK\r\n",6)==0);CloseHandle(thread);
 }
#endif
 ResetEvent(job.ready);memset(job.data,0,sizeof(job.data));thread=CreateThread(NULL,0,reader,&job,0,NULL);if(!thread)ExitProcess(2);
 WaitForSingleObject(job.ready,1000);Sleep(50);SetEvent(parent.stop);
 if(WaitForSingleObject(thread,2000)!=WAIT_OBJECT_0)ExitProcess(2);
 check(log,L"Stop releases pending reader with disconnect",job.result==(DWORD)-1 && job.error==ERROR_DEVICE_NOT_CONNECTED && job.elapsed<1000);CloseHandle(thread);
 check(log,L"Stop joins production pump",WaitForSingleObject(pump,2000)==WAIT_OBJECT_0);
 n=wsprintfW(line,L"QUEUE_UNIT CHECKS=%lu FAILURES=%lu COMPLETE\r\n",checks,failures);WriteFile(log,line,n*2,&bytes,NULL);CloseHandle(log);
 CloseHandle(pump);CloseHandle(job.ready);CloseHandle(p.arrival);CloseHandle(p.eventWake);CloseHandle(p.rxReady);CloseHandle(parent.stop);
 DeleteCriticalSection(&p.rxIO);DeleteCriticalSection(&p.io);ExitProcess(failures?1:0);
}
