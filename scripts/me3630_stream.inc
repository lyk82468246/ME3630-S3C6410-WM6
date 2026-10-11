/* V3 diagnostic stream, NOT a COM/Unimodem implementation.
 * Device Manager maps stream buffers; USB sees only our local staging buffer.
 * Single opener per channel; remove signals cancellation before taking I/O lock.
 * Parent/port refcounts keep an unplugged open handle safe until Close. */
typedef struct {
    PROBE *parent;
    USB_PIPE in,out;
    HANDLE active;
    CRITICAL_SECTION io;
#ifdef COM_V7
    CRITICAL_SECTION rxIO,txIO;
#endif
    LONG refs,opened;
#ifdef COM_V9
    HANDLE rxThread,rxReady,eventWake;
    DWORD qHead,qCount,rxFailure;
#ifdef COM_V15
    DWORD rxAbortEpoch,txQueued;
#endif
    BYTE queue[4096];
#ifdef COM_V12
    LONG pumpPhase,pumpCycles,pumpTimeouts;
#ifdef COM_V13
    BYTE txStage[512]; /* Driver-owned, serialized by txIO. */
#endif
#endif
#endif
#ifdef COM_V8
    DWORD eventMask,eventEpoch,eventPending;
    LONG eventWaiter;
#endif
    DWORD index;
#ifdef COM_V4
    DCB dcb;
    COMMTIMEOUTS timeouts;
    DWORD lines,rxPos,rxLen;
    BYTE rx[512];
#endif
} PORT;
static void probe_release(PROBE *p)
{
    if(InterlockedDecrement(&p->refs)==0) {
        if(p->thread)CloseHandle(p->thread);
        CloseHandle(p->stop);LocalFree(p);
    }
}
static void port_release(PORT *port)
{
    if(InterlockedDecrement(&port->refs)==0) {
        PROBE *p=port->parent;
#ifdef COM_V9
        if(port->rxThread)CloseHandle(port->rxThread);
        if(port->rxReady)CloseHandle(port->rxReady);
        if(port->eventWake)CloseHandle(port->eventWake);
#endif
#ifdef COM_V7
        DeleteCriticalSection(&port->rxIO);DeleteCriticalSection(&port->txIO);
#endif
        DeleteCriticalSection(&port->io);LocalFree(port);probe_release(p);
    }
}
#ifdef COM_V4
#include "me3630_serial.inc"
#endif
DWORD M36_Init(DWORD context)
{
    HKEY key;DWORD ptr=0,size=sizeof(ptr),type;
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,(LPCWSTR)context,0,0,&key)!=ERROR_SUCCESS)return 0;
    if(RegQueryValueEx(key,L"ClientInfo",NULL,&type,(LPBYTE)&ptr,&size)!=ERROR_SUCCESS || type!=REG_DWORD)ptr=0;
    RegCloseKey(key);
    if(ptr)InterlockedIncrement(&((PORT*)ptr)->refs);
    logline(L"STREAM_INIT ptr=%08lx\r\n",ptr);return ptr;
}
BOOL M36_Deinit(DWORD context){port_release((PORT*)context);return TRUE;}
DWORD M36_Open(DWORD context,DWORD access,DWORD share)
{
    PORT *port=(PORT*)context;
    if(WaitForSingleObject(port->parent->stop,0)==WAIT_OBJECT_0){SetLastError(ERROR_DEVICE_NOT_CONNECTED);return 0;}
    if(InterlockedCompareExchange(&port->opened,1,0)!=0){SetLastError(ERROR_SHARING_VIOLATION);return 0;}
    InterlockedIncrement(&port->refs);
    logline(L"STREAM_OPEN index=%lu access=%08lx\r\n",port->index,access);
    return context;
}
BOOL M36_Close(DWORD context)
{
    PORT *port=(PORT*)context;
#ifdef COM_V8
    EnterCriticalSection(&port->io);port->eventMask=0;port->eventPending=0;port->eventEpoch++;
#ifdef COM_V9
    SetEvent(port->eventWake);
#endif
    LeaveCriticalSection(&port->io);
#endif
    InterlockedExchange(&port->opened,0);port_release(port);return TRUE;
}
static DWORD stream_io(PORT *port,LPVOID buffer,DWORD count,BOOL input)
{
#ifdef COM_V4
    return serial_io(port,buffer,count,input);
#else
    BYTE stage[512];DWORD bytes=0,n;USB_TRANSFER t;BOOL ok;
    if(!count)return 0;
    EnterCriticalSection(&port->io);
    if(WaitForSingleObject(port->parent->stop,0)==WAIT_OBJECT_0 || !port->in || !port->out) {
        LeaveCriticalSection(&port->io);SetLastError(ERROR_DEVICE_NOT_CONNECTED);return (DWORD)-1;
    }
    /* Return a partial operation at 512 bytes; caller must loop for larger writes. */
    n=count<sizeof(stage)?count:sizeof(stage);
    if(!input)memcpy(stage,buffer,n);
    t=port->parent->usb->lpIssueBulkTransfer(input?port->in:port->out,NULL,NULL,
      USB_NO_WAIT|(input?(USB_IN_TRANSFER|USB_SHORT_TRANSFER_OK):USB_OUT_TRANSFER),n,stage,0);
    ok=wait_transfer(port->parent,t,3000,&bytes);
    if(ok && bytes<=n && input)memcpy(buffer,stage,bytes);
    if(bytes>n)ok=FALSE;
    LeaveCriticalSection(&port->io);
    if(!ok){SetLastError(ERROR_IO_DEVICE);return (DWORD)-1;}
    return bytes;
#endif
}
DWORD M36_Read(DWORD context,LPVOID buffer,DWORD count){return stream_io((PORT*)context,buffer,count,TRUE);}
DWORD M36_Write(DWORD context,LPCVOID buffer,DWORD count){return stream_io((PORT*)context,(LPVOID)buffer,count,FALSE);}
DWORD M36_Seek(DWORD context,LONG distance,DWORD method){SetLastError(ERROR_NOT_SUPPORTED);return (DWORD)-1;}
BOOL M36_IOControl(DWORD context,DWORD code,PBYTE in,DWORD inSize,PBYTE out,DWORD outSize,PDWORD actual)
#ifdef COM_V4
{return serial_control((PORT*)context,code,in,inSize,out,outSize,actual);}
#else
{if(actual)*actual=0;SetLastError(ERROR_NOT_SUPPORTED);return FALSE;}
#endif
void M36_PowerUp(DWORD context){}
void M36_PowerDown(DWORD context){}
#ifdef COM_V6
#include "me3630_adb.inc"
#endif
static DWORD WINAPI worker(LPVOID param)
{
    PROBE *p=(PROBE*)param;DWORD i,bytes,index,disp;PORT *port;HKEY key;WCHAR path[96];
    USB_DEVICE_REQUEST request;USB_TRANSFER t;
#ifdef COM_V4
#ifdef COM_V5
    logline(L"V5 WAIT_MODEM_READY 5s\r\n");
    for(i=0;i<5;i++){SystemIdleTimerReset();if(WaitForSingleObject(p->stop,1000)==WAIT_OBJECT_0)return 0;}
#else
    logline(L"V4 WAIT_MODEM_READY 20s\r\n");
    /* Startup-only idle reset; runtime power policy remains unchanged. */
    for(i=0;i<20;i++){SystemIdleTimerReset();if(WaitForSingleObject(p->stop,1000)==WAIT_OBJECT_0)return 0;}
#endif
#else
    logline(L"V3 WAIT_MODEM_READY 20s\r\n");
    if(WaitForSingleObject(p->stop,20000)==WAIT_OBJECT_0)return 0;
#endif
    for(i=0;i<p->count;i++) {
        if(WaitForSingleObject(p->stop,0)==WAIT_OBJECT_0)break;
        port=(PORT*)LocalAlloc(LPTR,sizeof(PORT));if(!port)continue;
        port->parent=p;port->refs=1;port->index=p->iface[i];
        InterlockedIncrement(&p->refs);InitializeCriticalSection(&port->io);p->ports[i]=port;
#ifdef COM_V7
        InitializeCriticalSection(&port->rxIO);InitializeCriticalSection(&port->txIO);
#endif
#ifdef COM_V9
        port->rxReady=CreateEvent(NULL,TRUE,FALSE,NULL);port->eventWake=CreateEvent(NULL,TRUE,FALSE,NULL);
        if(!port->rxReady || !port->eventWake){logline(L"V9 EVENT_CREATE_FAILED\r\n");continue;}
#endif
#ifdef COM_V4
        port->lines=3;port->dcb.DCBlength=sizeof(DCB);port->dcb.BaudRate=115200;port->dcb.ByteSize=8;port->dcb.fBinary=TRUE;
        port->dcb.fDtrControl=DTR_CONTROL_ENABLE;port->dcb.fRtsControl=RTS_CONTROL_ENABLE;
        port->timeouts.ReadIntervalTimeout=MAXDWORD;port->timeouts.ReadTotalTimeoutMultiplier=MAXDWORD;
        port->timeouts.ReadTotalTimeoutConstant=3000;port->timeouts.WriteTotalTimeoutConstant=3000;
#endif
        port->in=p->usb->lpOpenPipe(p->device,&p->in[i]);
        port->out=p->usb->lpOpenPipe(p->device,&p->out[i]);
        if(!port->in || !port->out){logline(L"V3 PIPE_FAILED interface=%lu\r\n",port->index);continue;}
        memset(&request,0,sizeof(request));request.bmRequestType=0x21;request.bRequest=0x22;
        request.wValue=3;request.wIndex=(USHORT)port->index;
        t=p->usb->lpIssueVendorTransfer(p->device,NULL,NULL,USB_OUT_TRANSFER|USB_NO_WAIT|USB_SEND_TO_INTERFACE,&request,NULL,0);
        if(!wait_transfer(p,t,1000,&bytes))continue;
#ifdef COM_V9
        InterlockedIncrement(&port->refs);
        port->rxThread=CreateThread(NULL,0,serial_rx_pump,port,0,NULL);
        if(!port->rxThread){port_release(port);logline(L"V9 RX_THREAD_FAILED\r\n");continue;}
#endif
#ifdef COM_V4
#ifdef COM_V5
#ifdef COM_V6
#ifdef COM_V7
#ifdef COM_V8
#ifdef COM_V9
#ifdef COM_V10
#ifdef COM_V11
#ifdef COM_V12
#ifdef COM_V13
#ifdef COM_V14
#ifdef COM_V15
        wsprintfW(path,L"Drivers\\ME3630ComV15\\Port%lu",port->index);
#else
        wsprintfW(path,L"Drivers\\ME3630ComV14\\Port%lu",port->index);
#endif
#else
        wsprintfW(path,L"Drivers\\ME3630ComV13\\Port%lu",port->index);
#endif
#else
        wsprintfW(path,L"Drivers\\ME3630ComV12\\Port%lu",port->index);
#endif
#else
        wsprintfW(path,L"Drivers\\ME3630ComV11\\Port%lu",port->index);
#endif
#else
        wsprintfW(path,L"Drivers\\ME3630ComV10\\Port%lu",port->index);
#endif
#else
        wsprintfW(path,L"Drivers\\ME3630ComV9\\Port%lu",port->index);
#endif
#else
        wsprintfW(path,L"Drivers\\ME3630ComV8\\Port%lu",port->index);
#endif
#else
        wsprintfW(path,L"Drivers\\ME3630ComV7\\Port%lu",port->index);
#endif
#else
        wsprintfW(path,L"Drivers\\ME3630ComV6\\Port%lu",port->index);
#endif
#else
        wsprintfW(path,L"Drivers\\ME3630ComV5\\Port%lu",port->index);
#endif
#else
        wsprintfW(path,L"Drivers\\ME3630ComV4\\Port%lu",port->index);
#endif
#else
        wsprintfW(path,L"Drivers\\ME3630StreamV3\\Port%lu",port->index);
#endif
        if(RegCreateKeyEx(HKEY_LOCAL_MACHINE,path,0,NULL,0,0,NULL,&key,&disp)!=ERROR_SUCCESS)continue;
        index=port->index;
#ifdef COM_V4
        index=port->index==1?1:5;
#ifdef COM_V5
#ifdef COM_V6
#ifdef COM_V7
#ifdef COM_V8
#ifdef COM_V9
#ifdef COM_V10
#ifdef COM_V11
#ifdef COM_V12
#ifdef COM_V13
#ifdef COM_V14
#ifdef COM_V15
        RegSetValueEx(key,L"Dll",0,REG_SZ,(LPBYTE)L"ME3630ComV15.dll",sizeof(L"ME3630ComV15.dll"));
#else
        RegSetValueEx(key,L"Dll",0,REG_SZ,(LPBYTE)L"ME3630ComV14.dll",sizeof(L"ME3630ComV14.dll"));
#endif
#else
        RegSetValueEx(key,L"Dll",0,REG_SZ,(LPBYTE)L"ME3630ComV13.dll",sizeof(L"ME3630ComV13.dll"));
#endif
#else
        RegSetValueEx(key,L"Dll",0,REG_SZ,(LPBYTE)L"ME3630ComV12.dll",sizeof(L"ME3630ComV12.dll"));
#endif
#else
        RegSetValueEx(key,L"Dll",0,REG_SZ,(LPBYTE)L"ME3630ComV11.dll",sizeof(L"ME3630ComV11.dll"));
#endif
#else
        RegSetValueEx(key,L"Dll",0,REG_SZ,(LPBYTE)L"ME3630ComV10.dll",sizeof(L"ME3630ComV10.dll"));
#endif
#else
        RegSetValueEx(key,L"Dll",0,REG_SZ,(LPBYTE)L"ME3630ComV9.dll",sizeof(L"ME3630ComV9.dll"));
#endif
#else
        RegSetValueEx(key,L"Dll",0,REG_SZ,(LPBYTE)L"ME3630ComV8.dll",sizeof(L"ME3630ComV8.dll"));
#endif
#else
        RegSetValueEx(key,L"Dll",0,REG_SZ,(LPBYTE)L"ME3630ComV7.dll",sizeof(L"ME3630ComV7.dll"));
#endif
#else
        RegSetValueEx(key,L"Dll",0,REG_SZ,(LPBYTE)L"ME3630ComV6.dll",sizeof(L"ME3630ComV6.dll"));
#endif
#else
        RegSetValueEx(key,L"Dll",0,REG_SZ,(LPBYTE)L"ME3630ComV5.dll",sizeof(L"ME3630ComV5.dll"));
#endif
#else
        RegSetValueEx(key,L"Dll",0,REG_SZ,(LPBYTE)L"ME3630ComV4.dll",sizeof(L"ME3630ComV4.dll"));
#endif
        RegSetValueEx(key,L"Prefix",0,REG_SZ,(LPBYTE)L"COM",sizeof(L"COM"));
#else
        RegSetValueEx(key,L"Dll",0,REG_SZ,(LPBYTE)L"ME3630StreamV3.dll",sizeof(L"ME3630StreamV3.dll"));
        RegSetValueEx(key,L"Prefix",0,REG_SZ,(LPBYTE)L"M36",sizeof(L"M36"));
#endif
        RegSetValueEx(key,L"Index",0,REG_DWORD,(LPBYTE)&index,sizeof(index));RegCloseKey(key);
        if(WaitForSingleObject(p->stop,0)==WAIT_OBJECT_0)break;
        port->active=ActivateDevice(path,(DWORD)port);
#ifdef COM_V4
        logline(L"V4 ACTIVATE COM%lu: interface=%lu handle=%08lx error=%lu\r\n",index,port->index,(DWORD)port->active,GetLastError());
#else
        logline(L"V3 ACTIVATE M36%lu: handle=%08lx error=%lu\r\n",index,(DWORD)port->active,GetLastError());
#endif
    }
#ifdef COM_V6
#ifdef COM_V7
#ifdef COM_V8
#ifdef COM_V9
#ifdef COM_V10
#ifdef COM_V11
#ifdef COM_V12
#ifdef COM_V13
#ifdef COM_V14
#ifdef COM_V15
    logline(L"V15 READY duplex=1 queue=4096 status=1 purge=RX\r\n");
#else
    logline(L"V14 READY duplex=1 queue=4096 pump_deadline=5000\r\n");
#endif
#else
    logline(L"V13 READY duplex=1 queue=4096 tx_stage=driver_owned\r\n");
#endif
#else
    logline(L"V12 READY duplex=1 queue=4096 transport_diagnostic=1\r\n");
#endif
#else
    logline(L"V11 READY duplex=1 queue=4096 wait_diagnostic=1\r\n");
#endif
#else
    logline(L"V10 READY duplex=1 queue=4096 diagnostic=1\r\n");
#endif
#else
    logline(L"V9 READY duplex=1 queue=4096 events=RXCHAR\r\n");
#endif
#else
    logline(L"V8 READY duplex=1 events=RXCHAR\r\n");
#endif
#else
    logline(L"V7 READY duplex=1\r\n");
#endif
#else
    logline(L"V6 READY\r\n");
#endif
    adb_boot_probe(p);
#elif defined(COM_V5)
    logline(L"V5 READY\r\n");
#elif defined(COM_V4)
    logline(L"V4 READY\r\n");
#else
    logline(L"V3 READY\r\n");
#endif
    return 0;
}
static void stream_cleanup(PROBE *p)
{
    DWORD i;PORT *port;
    for(i=0;i<p->count;i++) {
        port=(PORT*)p->ports[i];if(!port)continue;
#ifdef COM_V9
        if(port->rxThread) {
            DWORD joined=WaitForSingleObject(port->rxThread,5000);
            logline(L"V9 RX_JOIN interface=%lu wait=%lu\r\n",port->index,joined);
            /* Retain base references/pipes if the host call did not unwind.
             * Never kill a worker or free USB buffer/context underneath it. */
            if(joined!=WAIT_OBJECT_0){logline(L"V9 RETAIN_STUCK_PORT interface=%lu\r\n",port->index);continue;}
        }
#endif
        if(port->active && port->active!=INVALID_HANDLE_VALUE)DeactivateDevice(port->active);
#ifdef COM_V7
        /* stop is already signalled: pending USB waits leave before pipe close.
         * Lock order rx -> tx -> config; neither I/O path takes the other lock. */
        EnterCriticalSection(&port->rxIO);EnterCriticalSection(&port->txIO);
#endif
        EnterCriticalSection(&port->io);
        if(port->in)p->usb->lpClosePipe(port->in);
        if(port->out)p->usb->lpClosePipe(port->out);
        port->in=port->out=NULL;LeaveCriticalSection(&port->io);
#ifdef COM_V7
        LeaveCriticalSection(&port->txIO);LeaveCriticalSection(&port->rxIO);
#endif
        p->ports[i]=NULL;port_release(port);
    }
}
