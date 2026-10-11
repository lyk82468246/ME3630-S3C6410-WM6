/* One USB receive producer, bounded ring, independent application reader and
 * event waiter. Configuration lock protects queue and notification predicates. */
static DWORD WINAPI serial_rx_pump(LPVOID context)
{
 PORT *p=(PORT*)context;BYTE stage[512];DWORD bytes,freeBytes,error=0;int result;
 logline(L"V9 RX_START interface=%lu\r\n",p->index);
#ifdef COM_V11
 logline(L"Q_PUMP_CONTEXT port=%lu pid=%lu tid=%lu priority=%ld\r\n",p->index,GetCurrentProcessId(),GetCurrentThreadId(),CeGetThreadPriority(GetCurrentThread()));
#endif
 while(WaitForSingleObject(p->parent->stop,0)!=WAIT_OBJECT_0) {
#ifdef COM_V12
  InterlockedExchange(&p->pumpPhase,1);
#endif
  EnterCriticalSection(&p->io);freeBytes=sizeof(p->queue)-p->qCount;LeaveCriticalSection(&p->io);
  /* Reserve a whole USB packet batch; no dropping/overwriting when full. */
  if(freeBytes<sizeof(stage)){
#ifdef COM_V12
   InterlockedExchange(&p->pumpPhase,4);
#endif
   WaitForSingleObject(p->parent->stop,20);continue;
  }
#ifdef COM_V12
  InterlockedExchange(&p->pumpPhase,2);
#endif
#ifdef COM_V14
  /* Application deadlines remain independent. Stop is still checked by the
   * USB wait loop; this experiment reduces idle abort/reissue frequency. */
  result=serial_transfer(p,p->in,stage,sizeof(stage),TRUE,5000,&bytes);
#else
  result=serial_transfer(p,p->in,stage,sizeof(stage),TRUE,200,&bytes);
#endif
#ifdef COM_V12
  InterlockedIncrement(&p->pumpCycles);if(result==0)InterlockedIncrement(&p->pumpTimeouts);
  InterlockedExchange(&p->pumpPhase,3);
#endif
  if(result<0){error=GetLastError();break;}
  if(result==1 && bytes){
   EnterCriticalSection(&p->io);
   if(!ring_push(p,stage,bytes)){error=ERROR_BUFFER_OVERFLOW;LeaveCriticalSection(&p->io);break;}
#ifdef COM_DIAG35
   p->lastRxTick=GetTickCount();
#endif
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
#ifdef COM_DIAG37
 LIFE(M37_RX_EXIT,p,p->index,error);
#endif
 port_release(p);return 0;
}
static DWORD serial_queue_read(PORT *p,LPVOID buffer,DWORD count)
{
 DWORD start,limit,total=0,n,error=0;BOOL first;
#ifdef COM_DIAG35
 COMMTIMEOUTS snapshot;DWORD interval=0,lastArrival=0,waitLimit;BOOL immediate;
#endif
#ifdef COM_V15
 DWORD abortEpoch;
#endif
#ifdef COM_V11
 DWORD waited,waitError;BOOL waitLogged=FALSE;
#endif
#ifdef COM_V15
 /* Snapshot before serializing readers; never hold config while taking RX. */
 EnterCriticalSection(&p->io);abortEpoch=p->rxAbortEpoch;LeaveCriticalSection(&p->io);
#endif
 EnterCriticalSection(&p->rxIO);
 EnterCriticalSection(&p->io);limit=p->timeouts.ReadTotalTimeoutConstant;first=p->timeouts.ReadIntervalTimeout==MAXDWORD;LeaveCriticalSection(&p->io);
#ifdef COM_DIAG35
 EnterCriticalSection(&p->io);snapshot=p->timeouts;LeaveCriticalSection(&p->io);
 first=snapshot.ReadIntervalTimeout==MAXDWORD && snapshot.ReadTotalTimeoutMultiplier==MAXDWORD;
 immediate=snapshot.ReadIntervalTimeout==MAXDWORD && !snapshot.ReadTotalTimeoutMultiplier && !snapshot.ReadTotalTimeoutConstant;
 interval=snapshot.ReadIntervalTimeout==MAXDWORD?0:snapshot.ReadIntervalTimeout;
 if(first)limit=snapshot.ReadTotalTimeoutConstant;
 else if(!m36_timeout_total(snapshot.ReadTotalTimeoutMultiplier,snapshot.ReadTotalTimeoutConstant,count,&limit)){LeaveCriticalSection(&p->rxIO);SetLastError(ERROR_INVALID_PARAMETER);return (DWORD)-1;}
#endif
 start=GetTickCount();
#ifdef COM_V10
 logline(L"Q_READ_BEGIN port=%lu tid=%lu ptr=%08lx count=%lu timeout=%lu\r\n",p->index,GetCurrentThreadId(),(DWORD)p,count,limit);
#endif
#ifdef COM_V11
 logline(L"Q_READ_CONTEXT port=%lu pid=%lu priority=%ld\r\n",p->index,GetCurrentProcessId(),CeGetThreadPriority(GetCurrentThread()));
#endif
#ifdef COM_V12
 logline(L"Q_PUMP_SNAPSHOT port=%lu at=BEGIN phase=%ld cycles=%ld timeouts=%ld\r\n",p->index,p->pumpPhase,p->pumpCycles,p->pumpTimeouts);
#endif
 while(total<count){
  EnterCriticalSection(&p->io);
  if(!serial_alive(p)){error=ERROR_DEVICE_NOT_CONNECTED;LeaveCriticalSection(&p->io);break;}
#ifdef COM_V15
  if(abortEpoch!=p->rxAbortEpoch){error=ERROR_OPERATION_ABORTED;LeaveCriticalSection(&p->io);break;}
#endif
  n=ring_pop(p,(BYTE*)buffer+total,count-total);total+=n;
#ifdef COM_DIAG35
  if(n)lastArrival=p->lastRxTick;
#endif
#ifdef COM_V10
  if(n)logline(L"Q_POP port=%lu tid=%lu ptr=%08lx bytes=%lu queued=%lu\r\n",p->index,GetCurrentThreadId(),(DWORD)p,n,p->qCount);
#endif
  if(!p->qCount) {
#ifdef COM_V11
   if(!ResetEvent(p->rxReady)){error=GetLastError();LeaveCriticalSection(&p->io);break;}
#else
   ResetEvent(p->rxReady);
#endif
  }
  error=p->rxFailure;
  LeaveCriticalSection(&p->io);
  if((first && total) || total==count || error)break;
#ifdef COM_DIAG35
  if(immediate)break;
  n=GetTickCount()-start;if(limit!=INFINITE && n>=limit)break;
  waitLimit=20;if(limit!=INFINITE && limit-n<waitLimit)waitLimit=limit-n;
  if(total && interval){n=GetTickCount()-lastArrival;if(n>=interval)break;if(interval-n<waitLimit)waitLimit=interval-n;}
  n=waitLimit;
#else
  n=GetTickCount()-start;if(n>=limit)break;
  /* Stop is checked periodically too, even while RX producer is stuck. */
  n=limit-n;if(n>20)n=20;
#endif
#ifdef COM_V11
  waited=WaitForSingleObject(p->rxReady,n);waitError=waited==WAIT_FAILED?GetLastError():0;
  if(!waitLogged || waited==WAIT_FAILED){logline(L"Q_WAIT port=%lu tid=%lu status=%08lx error=%lu\r\n",p->index,GetCurrentThreadId(),waited,waitError);waitLogged=TRUE;}
  if(waited==WAIT_FAILED){error=waitError?waitError:ERROR_INVALID_HANDLE;break;}
#else
  WaitForSingleObject(p->rxReady,n);
#endif
 }
 LeaveCriticalSection(&p->rxIO);
#ifdef COM_V10
 logline(L"Q_READ_END port=%lu tid=%lu bytes=%lu elapsed=%lu error=%lu\r\n",p->index,GetCurrentThreadId(),total,GetTickCount()-start,error);
#endif
#ifdef COM_V12
 logline(L"Q_PUMP_SNAPSHOT port=%lu at=END phase=%ld cycles=%ld timeouts=%ld\r\n",p->index,p->pumpPhase,p->pumpCycles,p->pumpTimeouts);
#endif
 if(error && !total){SetLastError(error);return (DWORD)-1;}
 SetLastError(0);return total;
}
