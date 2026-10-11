/* Native ring test uses the exact production helpers; no hardware involved. */
typedef struct {DWORD qHead,qCount;BYTE queue[4096];} UNIT_QUEUE;
#define PORT UNIT_QUEUE
#include "me3630_ring.inc"
#undef PORT
static void ring_test(HANDLE log)
{
 UNIT_QUEUE q;BYTE input[4096],output[4096],expected[4096];DWORD i,n;BOOL ok;
 memset(&q,0,sizeof(q));for(i=0;i<sizeof(input);i++)input[i]=(BYTE)(i%251);
 check(log,L"Ring empty pop",ring_pop(&q,output,1)==0 && q.qCount==0);
 check(log,L"Ring exact capacity push",ring_push(&q,input,sizeof(input)) && q.qCount==4096);
 ok=ring_push(&q,input,1);check(log,L"Ring overflow rejected",!ok && q.qCount==4096 && q.qHead==0);
 n=ring_pop(&q,output,3072);check(log,L"Ring partial pop retains tail",n==3072 && memcmp(output,input,3072)==0 && q.qCount==1024 && q.qHead==3072);
 check(log,L"Ring wrapped push",ring_push(&q,input,3072) && q.qCount==4096);
 memcpy(expected,input+3072,1024);memcpy(expected+1024,input,3072);
 n=ring_pop(&q,output,4096);check(log,L"Ring wrapped FIFO data",n==4096 && q.qCount==0 && memcmp(output,expected,4096)==0);
}
static void queue_join_read(HANDLE log,HANDLE thread,DUPLEX *d)
{
 if(WaitForSingleObject(thread,5000)!=WAIT_OBJECT_0){check(log,L"Queue reader stuck",FALSE);FlushFileBuffers(log);ExitProcess(2);}
 CloseHandle(thread);CloseHandle(d->ready);
}
static void queue_test(HANDLE log,HANDLE port)
{
 EVENTJOB event;DUPLEX read;HANDLE waiter,reader;DWORD bytes,start,elapsed,n;BOOL ok;BYTE response[8];WCHAR line[160];
 check(log,L"Queue concurrent RX mask",SetCommMask(port,EV_RXCHAR));
 waiter=event_start(&event,port);check(log,L"Queue event waiter",waiter!=NULL);if(!waiter)return;
 memset(&read,0,sizeof(read));read.port=port;read.ready=CreateEvent(NULL,TRUE,FALSE,NULL);
 reader=read.ready?CreateThread(NULL,0,duplex_reader,&read,0,NULL):NULL;
 check(log,L"Queue simultaneous reader",reader!=NULL);
 if(!reader){SetCommMask(port,0);event_join(log,waiter,&event);if(read.ready)CloseHandle(read.ready);return;}
 WaitForSingleObject(read.ready,2000);Sleep(50);
 ok=WriteFile(port,"AT\r",3,&bytes,NULL);check(log,L"Queue concurrent AT write",ok && bytes==3);
 event_join(log,waiter,&event);check(log,L"Queue event survives competing reader",event.ok && event.mask==EV_RXCHAR);
 queue_join_read(log,reader,&read);check(log,L"Queue reader receives full AT OK",read.ok && read.elapsed<1500 && read.count==6);
 check(log,L"Queue clear concurrent mask",SetCommMask(port,0));
 n=wsprintfW(line,L"QUEUE_CONCURRENT event_ms=%lu read_ms=%lu bytes=%lu error=%lu\r\n",event.elapsed,read.elapsed,read.count,read.error);WriteFile(log,line,n*2,&bytes,NULL);
 check(log,L"Backlog RX mask",SetCommMask(port,EV_RXCHAR));
 waiter=event_start(&event,port);check(log,L"Backlog first waiter",waiter!=NULL);if(!waiter)return;
 ok=WriteFile(port,"AT\r",3,&bytes,NULL);check(log,L"Backlog AT write",ok && bytes==3);
 event_join(log,waiter,&event);memset(response,0,sizeof(response));ok=ReadFile(port,response,1,&bytes,NULL);
 check(log,L"Backlog event and partial read",event.ok && event.mask==EV_RXCHAR && ok && bytes==1 && response[0]=='\r');
 waiter=event_start(&event,port);check(log,L"Backlog repeat waiter",waiter!=NULL);if(!waiter)return;
 start=GetTickCount();ok=ReadFile(port,response+1,5,&bytes,NULL);elapsed=GetTickCount()-start;
 check(log,L"Unread backlog drains during event wait",ok && bytes==5 && memcmp(response,"\r\nOK\r\n",6)==0 && elapsed<500);
 ok=SetCommMask(port,0);event_join(log,waiter,&event);
 check(log,L"Backlog waiter cancels without RX deadlock",ok && event.ok && event.mask==0 && event.elapsed<1500);
 n=wsprintfW(line,L"QUEUE_BACKLOG read_ms=%lu cancel_wait_ms=%lu mask=%08lx error=%lu\r\n",elapsed,event.elapsed,event.mask,event.error);WriteFile(log,line,n*2,&bytes,NULL);
}
