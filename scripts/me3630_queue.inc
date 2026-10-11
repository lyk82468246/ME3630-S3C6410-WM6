/* One USB receive producer, bounded ring, independent application reader and
 * event waiter. Configuration lock protects queue and notification predicates. */
static DWORD WINAPI serial_rx_pump(LPVOID context)
{
 PORT *p=(PORT*)context;BYTE stage[512];DWORD bytes,freeBytes,error=0;int result;
 logline(L"V9 RX_START interface=%lu\r\n",p->index);
 while(WaitForSingleObject(p->parent->stop,0)!=WAIT_OBJECT_0) {
  EnterCriticalSection(&p->io);freeBytes=sizeof(p->queue)-p->qCount;LeaveCriticalSection(&p->io);
  /* Reserve a whole USB packet batch; no dropping/overwriting when full. */
  if(freeBytes<sizeof(stage)){WaitForSingleObject(p->parent->stop,20);continue;}
  result=serial_transfer(p,p->in,stage,sizeof(stage),TRUE,200,&bytes);
  if(result<0){error=GetLastError();break;}
  if(result==1 && bytes){
   EnterCriticalSection(&p->io);
   if(!ring_push(p,stage,bytes)){error=ERROR_BUFFER_OVERFLOW;LeaveCriticalSection(&p->io);break;}
   p->eventPending|=p->eventMask&EV_RXCHAR;SetEvent(p->rxReady);
#ifdef COM_V10
   logline(L"Q_PUSH port=%lu tid=%lu ptr=%08lx bytes=%lu queued=%lu\r\n",p->index,GetCurrentThreadId(),(DWORD)p,bytes,p->qCount);
#endif
   if(p->eventPending& p->eventMask)SetEvent(p->eventWake);
   LeaveCriticalSection(&p->io);
  }
 }
 if(!error)error=ERROR_DEVICE_NOT_CONNECTED;
 EnterCriticalSection(&p->io);p->rxFailure=error;SetEvent(p->rxReady);SetEvent(p->eventWake);LeaveCriticalSection(&p->io);
 logline(L"V9 RX_EXIT interface=%lu error=%lu\r\n",p->index,error);
 port_release(p);return 0;
}
static DWORD serial_queue_read(PORT *p,LPVOID buffer,DWORD count)
{
 DWORD start,limit,total=0,n,error=0;BOOL first;
 EnterCriticalSection(&p->rxIO);
 EnterCriticalSection(&p->io);limit=p->timeouts.ReadTotalTimeoutConstant;first=p->timeouts.ReadIntervalTimeout==MAXDWORD;LeaveCriticalSection(&p->io);
 start=GetTickCount();
#ifdef COM_V10
 logline(L"Q_READ_BEGIN port=%lu tid=%lu ptr=%08lx count=%lu timeout=%lu\r\n",p->index,GetCurrentThreadId(),(DWORD)p,count,limit);
#endif
 while(total<count){
  EnterCriticalSection(&p->io);
  if(!serial_alive(p)){error=ERROR_DEVICE_NOT_CONNECTED;LeaveCriticalSection(&p->io);break;}
  n=ring_pop(p,(BYTE*)buffer+total,count-total);total+=n;
#ifdef COM_V10
  if(n)logline(L"Q_POP port=%lu tid=%lu ptr=%08lx bytes=%lu queued=%lu\r\n",p->index,GetCurrentThreadId(),(DWORD)p,n,p->qCount);
#endif
  if(!p->qCount)ResetEvent(p->rxReady);
  error=p->rxFailure;
  LeaveCriticalSection(&p->io);
  if((first && total) || total==count || error)break;
  n=GetTickCount()-start;if(n>=limit)break;
  /* Stop is checked periodically too, even while RX producer is stuck. */
  n=limit-n;if(n>20)n=20;WaitForSingleObject(p->rxReady,n);
 }
 LeaveCriticalSection(&p->rxIO);
#ifdef COM_V10
 logline(L"Q_READ_END port=%lu tid=%lu bytes=%lu elapsed=%lu error=%lu\r\n",p->index,GetCurrentThreadId(),total,GetTickCount()-start,error);
#endif
 if(error && !total){SetLastError(error);return (DWORD)-1;}
 SetLastError(0);return total;
}
