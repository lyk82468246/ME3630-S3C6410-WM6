/* Included after PORT and port_release. Independent per-open ownership. */
typedef struct {PORT *port;DWORD access;BOOL ownsData;} M36_OPEN;
static M36_OPEN *m36_open_new(PORT *port,DWORD access)
{
 M36_OPEN *open=(M36_OPEN*)LocalAlloc(LPTR,sizeof(M36_OPEN));
 if(!open){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return NULL;}
 open->port=port;open->access=access;open->ownsData=access!=0;
 if(open->ownsData && InterlockedCompareExchange(&port->opened,1,0)!=0){LocalFree(open);SetLastError(ERROR_SHARING_VIOLATION);return NULL;}
 InterlockedIncrement(&port->refs);return open;
}
static void m36_open_release(M36_OPEN *open)
{
 if(open->ownsData)InterlockedExchange(&open->port->opened,0);
 port_release(open->port);LocalFree(open);
}
static BOOL m36_open_allows_control(M36_OPEN *open,DWORD code)
{
 if(open->ownsData)return TRUE;
 /* Metadata opens cannot mutate lines, events, FIFO or transport settings. */
 return code==0x00223ffc || code==0x00223ff4 || code==0x00223ff8 ||
        code==0x001b0024 || code==0x001b0050 || code==0x001b0040;
}
