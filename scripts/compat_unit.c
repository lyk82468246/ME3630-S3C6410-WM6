/* Production timeout/read/purge/notification policy; no USB, AT or registration. */
#include <windows.h>
#include "me3630_timeout.h"
#include "me3630_notify_state.h"
#undef wsprintfW
typedef struct {HANDLE stop;} PROBE;
typedef struct {
 PROBE *parent;HANDLE in,out,rxReady,eventWake;CRITICAL_SECTION io,rxIO;
 COMMTIMEOUTS timeouts;DWORD index,qHead,qCount,rxFailure,eventMask,eventPending;
 DWORD rxAbortEpoch,txClearEpoch,txQueued,lastRxTick;BYTE queue[4096];
#ifdef COM_DIAG36
 DWORD eventEpoch;LONG refs,eventWaiter;M36_NOTIFY_STATE notifyState;
#endif
} PORT;
static BOOL serial_alive(PORT *p){return WaitForSingleObject(p->parent->stop,0)!=WAIT_OBJECT_0;}
static void port_release(PORT *p){}
static void logline(LPCWSTR format,...){}
#define LIFE(event,object,value,error) ((void)0)
static int serial_transfer(PORT *p,HANDLE pipe,LPVOID buf,DWORD size,BOOL input,DWORD timeout,PDWORD bytes){*bytes=0;return 0;}
#include "me3630_ring.inc"
#include "me3630_queue.inc"
#include "me3630_purge.inc"
#ifdef COM_DIAG36
#include "me3630_mask.inc"
#include "me3630_events.inc"
#endif
static HANDLE logFile;static DWORD checks,failures;
static void check(LPCWSTR name,BOOL ok){WCHAR s[160];DWORD n,bytes;checks++;if(!ok)failures++;n=wsprintfW(s,L"CHECK %s pass=%lu\r\n",name,ok);WriteFile(logFile,s,n*2,&bytes,NULL);}
static void push(PORT *p,BYTE value){EnterCriticalSection(&p->io);ring_push(p,&value,1);p->lastRxTick=GetTickCount();SetEvent(p->rxReady);LeaveCriticalSection(&p->io);}
static DWORD WINAPI feed(LPVOID arg){PORT *p=(PORT*)arg;Sleep(10);push(p,'A');Sleep(30);push(p,'B');Sleep(100);push(p,'C');return 0;}
typedef struct {PORT *p;HANDLE ready;DWORD result,error;} JOB;
static DWORD WINAPI blocked(LPVOID arg){JOB *j=(JOB*)arg;BYTE buf[16];SetEvent(j->ready);j->result=serial_queue_read(j->p,buf,16);j->error=GetLastError();return 0;}
#ifdef COM_DIAG36
static DWORD WINAPI event_blocked(LPVOID arg){JOB *j=(JOB*)arg;DWORD mask=99,actual=0;j->result=serial_wait_event(j->p,(PBYTE)&mask,4,&actual);j->error=GetLastError();SetEvent(j->ready);return 0;}
#endif
void mainCRTStartup(void)
{
 PROBE parent;PORT p;COMMTIMEOUTS t,bad;DWORD limit,n,begin,elapsed,bytes;WCHAR s[180];BYTE data[16],packet[10]={0xa1,0x20,0,0,1,0,2,0,0,0};
 M36_NOTIFY_STATE state;DWORD pending=0;HANDLE thread;JOB job;
 logFile=CreateFile(L"\\Temp\\ME3630-compat-unit.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(logFile==INVALID_HANDLE_VALUE)ExitProcess(2);
 n=wsprintfW(s,L"COMPAT_UNIT start_tick=%lu\r\n",GetTickCount());WriteFile(logFile,s,n*2,&bytes,NULL);
 memset(&t,0,sizeof(t));t.ReadIntervalTimeout=50;t.ReadTotalTimeoutMultiplier=1;t.ReadTotalTimeoutConstant=50;t.WriteTotalTimeoutMultiplier=5;t.WriteTotalTimeoutConstant=500;
 check(L"measured_timeouts_valid",m36_timeout_valid(&t));check(L"read_budget66",m36_timeout_total(1,50,16,&limit) && limit==66);check(L"write_budget580",m36_timeout_total(5,500,16,&limit) && limit==580);
 check(L"overflow_rejected",!m36_timeout_total(10000,10000,MAXDWORD,&limit));check(L"disabled_total",m36_timeout_total(0,0,16,&limit) && limit==INFINITE);
 bad=t;bad.ReadIntervalTimeout=MAXDWORD;bad.ReadTotalTimeoutMultiplier=1;check(L"invalid_MAX_rejected",!m36_timeout_valid(&bad));
 bad=t;bad.ReadIntervalTimeout=MAXDWORD;bad.ReadTotalTimeoutMultiplier=MAXDWORD;bad.ReadTotalTimeoutConstant=400;check(L"firstbyte_valid",m36_timeout_valid(&bad));
 memset(&p,0,sizeof(p));parent.stop=CreateEvent(NULL,TRUE,FALSE,NULL);p.parent=&parent;p.rxReady=CreateEvent(NULL,TRUE,FALSE,NULL);p.eventWake=CreateEvent(NULL,TRUE,FALSE,NULL);InitializeCriticalSection(&p.io);InitializeCriticalSection(&p.rxIO);p.timeouts=t;
 begin=GetTickCount();n=serial_queue_read(&p,data,16);elapsed=GetTickCount()-begin;check(L"empty_total_deadline",n==0 && elapsed>=50 && elapsed<250);
 p.timeouts.ReadTotalTimeoutConstant=500;push(&p,'A');push(&p,'B');begin=GetTickCount();n=serial_queue_read(&p,data,16);elapsed=GetTickCount()-begin;check(L"partial_interval50",n==2 && data[0]=='A' && data[1]=='B' && elapsed>=35 && elapsed<200);
 thread=CreateThread(NULL,0,feed,&p,0,NULL);begin=GetTickCount();n=serial_queue_read(&p,data,16);elapsed=GetTickCount()-begin;check(L"arrival_resets_interval",thread && n==2 && data[0]=='A' && data[1]=='B' && elapsed>=70 && elapsed<140);if(thread){WaitForSingleObject(thread,1000);CloseHandle(thread);}
 m36_purge_host_buffers(&p,PURGE_RXCLEAR);check(L"late_C_cleared",p.qCount==0);
 memset(&p.timeouts,0,sizeof(p.timeouts));p.timeouts.ReadIntervalTimeout=MAXDWORD;begin=GetTickCount();n=serial_queue_read(&p,data,16);check(L"immediate_empty",n==0 && GetTickCount()-begin<50);push(&p,'X');n=serial_queue_read(&p,data,16);check(L"immediate_buffered",n==1 && data[0]=='X');
 p.timeouts=bad;push(&p,'Y');begin=GetTickCount();n=serial_queue_read(&p,data,16);check(L"firstbyte_buffered",n==1 && data[0]=='Y' && GetTickCount()-begin<50);
 p.timeouts=t;push(&p,'Q');p.txQueued=100;p.eventMask=EV_RXCHAR|EV_DSR;p.eventPending=EV_RXCHAR|EV_DSR;
 check(L"combined_clear",m36_purge_host_buffers(&p,0x0c) && !p.qCount && !p.txQueued && p.txClearEpoch==1 && p.eventPending==EV_DSR);
 push(&p,'R');p.txQueued=12;n=p.txClearEpoch;check(L"TXABORT_atomic_reject",!m36_purge_host_buffers(&p,PURGE_RXCLEAR|PURGE_TXABORT) && p.qCount==1 && p.txQueued==12 && p.txClearEpoch==n);
 check(L"purge_zero_reject",!m36_purge_host_buffers(&p,0));m36_purge_host_buffers(&p,PURGE_RXCLEAR);
 check(L"RAS_mask_accepted",m36_event_mask_supported(0x1f9));check(L"TXEMPTY_not_claimed",!m36_event_mask_supported(EV_TXEMPTY));check(L"RXFLAG_not_claimed",!m36_event_mask_supported(EV_RXFLAG));
 memset(&state,0,sizeof(state));state.size=sizeof(state);m36_update_notify(&state,packet,10,1,GetTickCount(),0x1f9,&pending);check(L"baseline_no_edges",pending==0 && !(state.known&MS_CTS_ON));
 packet[8]=0x74;m36_update_notify(&state,packet,10,1,GetTickCount(),0x1f9,&pending);check(L"real_error_break",(pending&(EV_BREAK|EV_ERR))==(EV_BREAK|EV_ERR) && !(pending&EV_CTS));check(L"error_mapping",m36_notify_errors(state.raw)==(CE_BREAK|CE_FRAME|CE_RXPARITY|CE_OVERRUN));
 pending=0;m36_update_notify(&state,packet,10,1,GetTickCount(),0x1f9,&pending);check(L"break_not_repeated",!(pending&EV_BREAK) && (pending&EV_ERR));
 pending=0;packet[9]=1;check(L"invalid_notify_no_events",!m36_update_notify(&state,packet,10,1,GetTickCount(),0x1f9,&pending) && !pending);
#ifdef COM_DIAG36
 p.eventMask=0x1f9;p.eventPending=EV_RXCHAR;p.eventEpoch=20;
 check(L"mask_preserves_pending_RX",m36_set_event_mask(&p,0x1f9) && p.eventPending==EV_RXCHAR && p.eventEpoch==21);
 n=bytes=0;check(L"wait_delivers_retained_RX",serial_wait_event(&p,(PBYTE)&n,4,&bytes) && n==EV_RXCHAR && bytes==4 && !p.eventPending);
 push(&p,'Z');check(L"mask_no_replay_consumed",m36_set_event_mask(&p,0x1f9) && !p.eventPending && p.qCount==1);
 p.eventPending=EV_RXCHAR|EV_DSR;check(L"mask_prunes_removed",m36_set_event_mask(&p,EV_RXCHAR) && p.eventPending==EV_RXCHAR);
 check(L"mask_zero_clears",m36_set_event_mask(&p,0) && !p.eventPending && !p.eventMask);
 n=p.eventEpoch;check(L"mask_invalid_atomic",!m36_set_event_mask(&p,EV_TXEMPTY) && p.eventEpoch==n && !p.eventMask);
 m36_purge_host_buffers(&p,PURGE_RXCLEAR);m36_set_event_mask(&p,EV_RXCHAR);
 memset(&job,0,sizeof(job));job.p=&p;job.ready=CreateEvent(NULL,TRUE,FALSE,NULL);thread=CreateThread(NULL,0,event_blocked,&job,0,NULL);
 begin=GetTickCount();while(!p.eventWaiter && GetTickCount()-begin<500)Sleep(1);Sleep(30);m36_set_event_mask(&p,EV_RXCHAR);n=WaitForSingleObject(thread,1000);
 check(L"same_mask_cancels_wait",thread && n==WAIT_OBJECT_0 && job.result && !job.error && !p.eventPending);if(thread)CloseHandle(thread);CloseHandle(job.ready);
#endif
 memset(&p.timeouts,0,sizeof(p.timeouts));memset(&job,0,sizeof(job));job.p=&p;job.ready=CreateEvent(NULL,TRUE,FALSE,NULL);thread=CreateThread(NULL,0,blocked,&job,0,NULL);WaitForSingleObject(job.ready,1000);Sleep(30);SetEvent(parent.stop);n=WaitForSingleObject(thread,1000);check(L"unbounded_read_disconnect",n==WAIT_OBJECT_0 && job.result==(DWORD)-1 && job.error==ERROR_DEVICE_NOT_CONNECTED);if(thread)CloseHandle(thread);CloseHandle(job.ready);
 DeleteCriticalSection(&p.rxIO);DeleteCriticalSection(&p.io);CloseHandle(p.rxReady);CloseHandle(p.eventWake);CloseHandle(parent.stop);
 n=wsprintfW(s,L"COMPAT_UNIT CHECKS=%lu FAILURES=%lu COMPLETE\r\n",checks,failures);WriteFile(logFile,s,n*2,&bytes,NULL);CloseHandle(logFile);ExitProcess(failures?1:0);
}
