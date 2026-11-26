/* Included after PORT and port_release. Independent per-open ownership.
 * D41 aligns zero-access handles (Unimodem hDevice_r0) with CE5 COM_MDD2:
 * each open owns independent event mask/pending/wait state and may call
 * Get/SetCommMask, WaitCommEvent, and GetCommModemStatus without mutating
 * data ownership, DCB, timeouts, purge, or line control. */
typedef struct M36_OPEN_TAG {
    PORT *port;
    DWORD access;
    BOOL ownsData;
#ifdef COM_DIAG41
    struct M36_OPEN_TAG *next;
    LONG refs, eventWaiter;
    DWORD eventMask, eventPending, eventEpoch;
    HANDLE eventWake;
    BOOL closing;
#endif
} M36_OPEN;
static M36_OPEN *m36_open_new(PORT *port,DWORD access)
{
 M36_OPEN *open=(M36_OPEN*)LocalAlloc(LPTR,sizeof(M36_OPEN));
 if(!open){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return NULL;}
 open->port=port;open->access=access;open->ownsData=access!=0;
#ifdef COM_DIAG41
 open->refs=1;
 open->eventWake=CreateEvent(NULL,TRUE,FALSE,NULL);
 if(!open->eventWake){LocalFree(open);SetLastError(ERROR_NOT_ENOUGH_MEMORY);return NULL;}
#endif
 if(open->ownsData && InterlockedCompareExchange(&port->opened,1,0)!=0){
#ifdef COM_DIAG41
  CloseHandle(open->eventWake);
#endif
  LocalFree(open);SetLastError(ERROR_SHARING_VIOLATION);return NULL;
 }
 InterlockedIncrement(&port->refs);
#ifdef COM_DIAG41
 EnterCriticalSection(&port->io);
 open->next=port->openList;port->openList=open;
 LeaveCriticalSection(&port->io);
#endif
 return open;
}
#ifdef COM_DIAG41
static void m36_open_deref(M36_OPEN *open)
{
 if(InterlockedDecrement(&open->refs)==0){
  PORT *port=open->port;
  if(open->eventWake)CloseHandle(open->eventWake);
  LocalFree(open);port_release(port);
 }
}
static void m36_broadcast_events(PORT *port,DWORD events)
{
 M36_OPEN *o;
 for(o=port->openList;o;o=o->next){
  DWORD hit=events&o->eventMask;
  if(hit){o->eventPending|=hit;SetEvent(o->eventWake);}
 }
}
static void m36_wake_all_opens(PORT *port)
{
 M36_OPEN *o;
 for(o=port->openList;o;o=o->next)SetEvent(o->eventWake);
}
static void m36_clear_rx_pending_opens(PORT *port)
{
 M36_OPEN *o;
 for(o=port->openList;o;o=o->next){
  o->eventPending&=~EV_RXCHAR;
  if(!(o->eventPending&o->eventMask))ResetEvent(o->eventWake);
 }
}
#endif
static void m36_open_release(M36_OPEN *open)
{
#ifdef COM_DIAG41
 PORT *port=open->port;M36_OPEN **pp;
 EnterCriticalSection(&port->io);
 for(pp=&port->openList;*pp;pp=&(*pp)->next){if(*pp==open){*pp=open->next;open->next=NULL;break;}}
 open->closing=TRUE;open->eventMask=0;open->eventPending=0;open->eventEpoch++;
 if(open->eventWake)SetEvent(open->eventWake);
 LeaveCriticalSection(&port->io);
 if(open->ownsData)InterlockedExchange(&port->opened,0);
 m36_open_deref(open);
#else
 if(open->ownsData)InterlockedExchange(&open->port->opened,0);
 port_release(open->port);LocalFree(open);
#endif
}
static BOOL m36_open_allows_control(M36_OPEN *open,DWORD code)
{
 if(open->ownsData)return TRUE;
#ifdef COM_DIAG41
 /* Matches CE5 COM_MDD2\mdd.c:2134-2142 + private read-only queries. */
 return code==0x00223ffc || code==0x00223ff4 || code==0x00223ff8 ||
        code==0x001b0024 || code==0x001b0028 || code==0x001b002c ||
        code==0x001b0034 || code==0x001b0050 || code==0x001b0040;
#else
 /* Metadata opens cannot mutate lines, events, FIFO or transport settings. */
 return code==0x00223ffc || code==0x00223ff4 || code==0x00223ff8 ||
        code==0x001b0024 || code==0x001b0050 || code==0x001b0040;
#endif
}
