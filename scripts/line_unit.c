/* Production notification-update and WaitCommEvent helpers; no USB/AT. */
#include <windows.h>
#include "me3630_notify_state.h"
#undef wsprintfW
typedef struct {HANDLE stop;} PROBE;
typedef struct {
    PROBE *parent;CRITICAL_SECTION io;LONG refs,eventWaiter;
    DWORD eventEpoch,eventMask,eventPending,rxFailure;
    HANDLE eventWake;M36_NOTIFY_STATE notifyState;
} PORT;
static BOOL serial_alive(PORT *p){return WaitForSingleObject(p->parent->stop,0)!=WAIT_OBJECT_0;}
static void port_release(PORT *p){InterlockedDecrement(&p->refs);}
#include "me3630_events.inc"
typedef struct {PORT *port;HANDLE ready;BOOL ok;DWORD mask,actual,error;} JOB;
static DWORD WINAPI waiter(LPVOID arg)
{
    JOB *j=(JOB*)arg;SetEvent(j->ready);
    j->ok=serial_wait_event(j->port,(PBYTE)&j->mask,4,&j->actual);j->error=GetLastError();return 0;
}
static DWORD checks,failures;
static void check(HANDLE log,LPCWSTR name,BOOL ok)
{
    WCHAR text[160];DWORD n,bytes;checks++;if(!ok)failures++;
    n=wsprintfW(text,L"CHECK %s pass=%lu\r\n",name,ok);WriteFile(log,text,n*2,&bytes,NULL);
}
static BOOL sample(PORT *p,BYTE raw)
{
    BYTE data[10]={0xa1,0x20,0,0,1,0,2,0,0,0};BOOL valid;data[8]=raw;
    EnterCriticalSection(&p->io);
    valid=m36_update_notify(&p->notifyState,data,10,1,GetTickCount(),p->eventMask,&p->eventPending);
    if(p->eventPending&p->eventMask)SetEvent(p->eventWake);
    LeaveCriticalSection(&p->io);return valid;
}
void mainCRTStartup(void)
{
    PORT p;PROBE parent;JOB j;HANDLE log,thread;DWORD n,bytes,mask,actual;BOOL ok;WCHAR text[120];M36_NOTIFY_STATE old;
    memset(&p,0,sizeof(p));memset(&j,0,sizeof(j));p.parent=&parent;p.refs=1;
    parent.stop=CreateEvent(NULL,TRUE,FALSE,NULL);p.eventWake=CreateEvent(NULL,TRUE,FALSE,NULL);j.ready=CreateEvent(NULL,TRUE,FALSE,NULL);j.port=&p;
    if(!parent.stop || !p.eventWake || !j.ready)ExitProcess(2);
    InitializeCriticalSection(&p.io);p.eventMask=M36_LINE_EVENTS;
    log=CreateFile(L"\\Temp\\ME3630-line-unit.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(log==INVALID_HANDLE_VALUE)ExitProcess(2);
    n=wsprintfW(text,L"LINE_UNIT start_tick=%lu no_usb=1\r\n",GetTickCount());WriteFile(log,text,n*2,&bytes,NULL);
    check(log,L"State ABI unchanged",sizeof(p.notifyState)==32);
    ok=sample(&p,11);check(log,L"First high state creates baseline only",ok && !p.eventPending && p.notifyState.status==M36_NOTIFY_KNOWN);
    sample(&p,0);check(log,L"Falling DSR/DCD edges without ring",p.eventPending==(EV_DSR|EV_RLSD));
    mask=actual=0;ok=serial_wait_event(&p,(PBYTE)&mask,4,&actual);check(log,L"Production waiter consumes merged edges",ok && mask==(EV_DSR|EV_RLSD) && actual==4 && !p.eventPending);
    sample(&p,11);mask=0;ok=serial_wait_event(&p,(PBYTE)&mask,4,&actual);check(log,L"Rising DSR DCD ring delivered",ok && mask==M36_LINE_EVENTS);
    sample(&p,11);check(log,L"Repeated high state creates no event",!p.eventPending);
    sample(&p,3);check(log,L"Ring falling alone creates no ring event",!p.eventPending);
    old=p.notifyState;sample(&p,0x80);check(log,L"Malformed input preserves state and pending",p.notifyState.status==old.status && p.notifyState.tick==old.tick && p.notifyState.accepted==old.accepted && p.notifyState.rejected==old.rejected+1 && !p.eventPending);
    p.eventMask=EV_DSR;sample(&p,0);check(log,L"Mask filters unrequested DCD",p.eventPending==EV_DSR);p.eventPending=0;
    p.eventMask=EV_RXCHAR|M36_LINE_EVENTS;p.eventPending=EV_RXCHAR;sample(&p,11);
    check(log,L"Line update preserves pending RX event",p.eventPending==(EV_RXCHAR|M36_LINE_EVENTS));p.eventPending=0;
    p.eventMask=0;sample(&p,11);check(log,L"Disabled mask still updates baseline",!p.eventPending && p.notifyState.status==M36_NOTIFY_KNOWN);
    p.eventMask=M36_LINE_EVENTS;sample(&p,11);check(log,L"Reenable does not synthesize edge",!p.eventPending);
    thread=CreateThread(NULL,0,waiter,&j,0,NULL);if(!thread)ExitProcess(2);WaitForSingleObject(j.ready,1000);Sleep(80);
    check(log,L"Line waiter is pending",WaitForSingleObject(thread,0)==WAIT_TIMEOUT && p.eventWaiter==1);
    mask=MAXDWORD;actual=0;ok=serial_wait_event(&p,(PBYTE)&mask,4,&actual);check(log,L"Duplicate waiter rejected without state loss",!ok && GetLastError()==ERROR_BUSY && !mask && !actual);
    EnterCriticalSection(&p.io);p.eventMask=0;p.eventEpoch++;p.eventPending=0;SetEvent(p.eventWake);LeaveCriticalSection(&p.io);
    if(WaitForSingleObject(thread,2000)!=WAIT_OBJECT_0)ExitProcess(2);
    check(log,L"Mask generation cancels with zero result",j.ok && !j.mask && j.actual==4);CloseHandle(thread);
    p.eventMask=M36_LINE_EVENTS;sample(&p,0);p.eventPending=0;ResetEvent(j.ready);
    thread=CreateThread(NULL,0,waiter,&j,0,NULL);if(!thread)ExitProcess(2);WaitForSingleObject(j.ready,1000);Sleep(80);
    check(log,L"Thread waits before simulated edge",WaitForSingleObject(thread,0)==WAIT_TIMEOUT);
    sample(&p,11);if(WaitForSingleObject(thread,2000)!=WAIT_OBJECT_0)ExitProcess(2);
    check(log,L"Simulated edge wakes production waiter",j.ok && j.mask==M36_LINE_EVENTS && j.actual==4);CloseHandle(thread);
    p.eventMask=M36_LINE_EVENTS;p.notifyState.error=ERROR_IO_DEVICE;mask=0;
    ok=serial_wait_event(&p,(PBYTE)&mask,4,&actual);check(log,L"Notification failure rejects line wait",!ok && GetLastError()==ERROR_IO_DEVICE && !mask);
    p.eventMask=EV_RXCHAR;p.eventPending=EV_RXCHAR;mask=0;
    ok=serial_wait_event(&p,(PBYTE)&mask,4,&actual);check(log,L"RX-only remains independent of line failure",ok && mask==EV_RXCHAR);
    p.notifyState.error=0;p.eventMask=M36_LINE_EVENTS;ResetEvent(j.ready);
    thread=CreateThread(NULL,0,waiter,&j,0,NULL);if(!thread)ExitProcess(2);WaitForSingleObject(j.ready,1000);Sleep(80);SetEvent(parent.stop);
    if(WaitForSingleObject(thread,2000)!=WAIT_OBJECT_0)ExitProcess(2);
    check(log,L"Stop releases pending line waiter",!j.ok && j.error==ERROR_DEVICE_NOT_CONNECTED && !j.mask);CloseHandle(thread);
    check(log,L"Waiter references balanced",p.refs==1 && !p.eventWaiter);
    n=wsprintfW(text,L"LINE_UNIT CHECKS=%lu FAILURES=%lu COMPLETE\r\n",checks,failures);WriteFile(log,text,n*2,&bytes,NULL);CloseHandle(log);
    DeleteCriticalSection(&p.io);CloseHandle(parent.stop);CloseHandle(p.eventWake);CloseHandle(j.ready);ExitProcess(failures?1:0);
}
