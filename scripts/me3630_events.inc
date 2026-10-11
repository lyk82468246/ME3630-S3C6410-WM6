/* RXCHAR subset: V8 waiter owns RX; V9 waiter only observes producer events.
 * No fabricated modem-line status, EV_TXEMPTY or line-error information. */
static BOOL serial_wait_event(PORT *port,PBYTE out,DWORD outSize,PDWORD actual)
{
 DWORD epoch,mask=0,error=0;BOOL ok=FALSE,done;
#ifndef COM_V9
 DWORD bytes;int result;
#endif
 if(!out || outSize<sizeof(DWORD)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
 memset(out,0,sizeof(DWORD));
 if(InterlockedCompareExchange(&port->eventWaiter,1,0)!=0){SetLastError(ERROR_BUSY);return FALSE;}
 InterlockedIncrement(&port->refs);
 EnterCriticalSection(&port->io);epoch=port->eventEpoch;LeaveCriticalSection(&port->io);
#ifndef COM_V9
 EnterCriticalSection(&port->rxIO);
#endif
 for(;;) {
  EnterCriticalSection(&port->io);done=FALSE;
  if(!serial_alive(port)){error=ERROR_DEVICE_NOT_CONNECTED;done=TRUE;}
#ifdef COM_V9
  else if(port->rxFailure){error=port->rxFailure;done=TRUE;}
#endif
  else if(epoch!=port->eventEpoch || !port->eventMask){ok=TRUE;done=TRUE;}
  else if(port->eventPending&port->eventMask){mask=port->eventPending&port->eventMask;port->eventPending&=~mask;ok=TRUE;done=TRUE;}
#ifdef COM_V9
  if(!done)ResetEvent(port->eventWake);
#endif
  LeaveCriticalSection(&port->io);
  if(done)break;
#ifdef COM_V9
  WaitForSingleObject(port->eventWake,20);
#else
  /* Existing unread data has already generated its event. Do not overwrite it
   * or manufacture repeated RX events. ReadFile must drain it before more USB. */
  if(port->rxLen>port->rxPos){WaitForSingleObject(port->parent->stop,20);continue;}
  port->rxLen=port->rxPos=0;
  result=serial_transfer(port,port->in,port->rx,sizeof(port->rx),TRUE,200,&bytes);
  if(result<0){error=GetLastError();break;}
  if(result==1 && bytes){
   port->rxLen=bytes;
   EnterCriticalSection(&port->io);
   /* Preserve bytes even if mask changed during USB transfer; the cancelled
    * wait returns zero, and ReadFile can still obtain this response. */
   if(epoch==port->eventEpoch)port->eventPending|=port->eventMask&EV_RXCHAR;
   LeaveCriticalSection(&port->io);
  }
#endif
 }
#ifndef COM_V9
 LeaveCriticalSection(&port->rxIO);
#endif
 memcpy(out,&mask,sizeof(mask));if(ok && actual)*actual=sizeof(mask);
 InterlockedExchange(&port->eventWaiter,0);port_release(port);
 SetLastError(error);return ok;
}
