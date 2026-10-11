/* Codes measured using native APIs on CE5.2 build20782; see ioctl-abi.txt.
 * V4 deliberately supports a bounded subset, NOT Unimodem/PPP yet.
 * V4-V6 serialize I/O; V7 separates RX/TX and snapshots configuration. */
static BOOL serial_alive(PORT *port)
{return WaitForSingleObject(port->parent->stop,0)!=WAIT_OBJECT_0 && port->in && port->out;}
static BOOL serial_lines(PORT *port,DWORD lines)
{
    USB_DEVICE_REQUEST req;USB_TRANSFER t;DWORD bytes;
    memset(&req,0,sizeof(req));req.bmRequestType=0x21;req.bRequest=0x22;
    req.wValue=(USHORT)lines;req.wIndex=(USHORT)port->index;
    t=port->parent->usb->lpIssueVendorTransfer(port->parent->device,NULL,NULL,USB_OUT_TRANSFER|USB_NO_WAIT|USB_SEND_TO_INTERFACE,&req,NULL,0);
    if(!wait_transfer(port->parent,t,1000,&bytes)){SetLastError(ERROR_IO_DEVICE);return FALSE;}
    port->lines=lines;port->dcb.fDtrControl=(lines&1)?DTR_CONTROL_ENABLE:DTR_CONTROL_DISABLE;
    port->dcb.fRtsControl=(lines&2)?RTS_CONTROL_ENABLE:RTS_CONTROL_DISABLE;return TRUE;
}
/* Returns 1=success, 0=deadline, -1=disconnect/USB failure. */
static int serial_transfer(PORT *port,USB_PIPE pipe,LPVOID buf,DWORD size,BOOL input,DWORD timeout,PDWORD bytes)
{
    PROBE *p=port->parent;USB_TRANSFER t;DWORD start=GetTickCount(),error;BOOL complete,expired=FALSE;
    *bytes=0;
    t=p->usb->lpIssueBulkTransfer(pipe,NULL,NULL,USB_NO_WAIT|(input?(USB_IN_TRANSFER|USB_SHORT_TRANSFER_OK):USB_OUT_TRANSFER),size,buf,0);
    if(!t){SetLastError(ERROR_IO_DEVICE);return -1;}
    while(!p->usb->lpIsTransferComplete(t)) {
        if(WaitForSingleObject(p->stop,1)==WAIT_OBJECT_0)break;
        if(GetTickCount()-start>=timeout){expired=TRUE;break;}
    }
    complete=p->usb->lpIsTransferComplete(t);
    if(!complete)p->usb->lpAbortTransfer(t,0);
    p->usb->lpGetTransferStatus(t,bytes,&error);p->usb->lpCloseTransfer(t);
    if(WaitForSingleObject(p->stop,0)==WAIT_OBJECT_0){SetLastError(ERROR_DEVICE_NOT_CONNECTED);return -1;}
    if(complete && error==USB_NO_ERROR && *bytes<=size)return 1;
    *bytes=0;
    if(expired){SetLastError(0);return 0;}
    SetLastError(ERROR_IO_DEVICE);return -1;
}
static DWORD serial_io(PORT *port,LPVOID buffer,DWORD count,BOOL input)
{
    DWORD start,limit,total=0,n,bytes;BOOL first;int result;BYTE stage[512];
#ifdef COM_V7
    CRITICAL_SECTION *direction=input?&port->rxIO:&port->txIO;
#endif
    if(!count)return 0;
#ifdef COM_V7
    EnterCriticalSection(direction);
#endif
    EnterCriticalSection(&port->io);
    if(!serial_alive(port)){
        LeaveCriticalSection(&port->io);
#ifdef COM_V7
        LeaveCriticalSection(direction);
#endif
        SetLastError(ERROR_DEVICE_NOT_CONNECTED);return (DWORD)-1;
    }
    start=GetTickCount();limit=input?port->timeouts.ReadTotalTimeoutConstant:port->timeouts.WriteTotalTimeoutConstant;
    first=port->timeouts.ReadIntervalTimeout==MAXDWORD;
#ifdef COM_V7
    /* Per-call configuration snapshot; control remains independent of bulk I/O.
     * The direction lock keeps this pipe/buffer alive until transfer completes. */
    LeaveCriticalSection(&port->io);
#endif
    while(total<count) {
        if(input && port->rxLen>port->rxPos) {
            n=port->rxLen-port->rxPos;if(n>count-total)n=count-total;
            memcpy((PBYTE)buffer+total,port->rx+port->rxPos,n);port->rxPos+=n;total+=n;
            if(first || total==count)break;
        }
        n=GetTickCount()-start;if(n>=limit)break;
        if(input) {
            port->rxPos=port->rxLen=0;
            result=serial_transfer(port,port->in,port->rx,sizeof(port->rx),TRUE,limit-n,&bytes);
            if(result==1){
                port->rxLen=bytes;
#ifdef COM_V8
                if(bytes){EnterCriticalSection(&port->io);port->eventPending|=port->eventMask&EV_RXCHAR;LeaveCriticalSection(&port->io);}
#endif
            }
        } else {
            bytes=count-total;if(bytes>sizeof(stage))bytes=sizeof(stage);
            memcpy(stage,(PBYTE)buffer+total,bytes);
            result=serial_transfer(port,port->out,stage,bytes,FALSE,limit-n,&bytes);
            if(result==1)total+=bytes;
        }
        if(result<0){
#ifdef COM_V7
            LeaveCriticalSection(direction);
#else
            LeaveCriticalSection(&port->io);
#endif
            return (DWORD)-1;
        }
        if(result==0)break;
    }
#ifdef COM_V7
    LeaveCriticalSection(direction);
#else
    LeaveCriticalSection(&port->io);
#endif
    SetLastError(0);return total;
}
#ifdef COM_V8
#include "me3630_events.inc"
#endif
static BOOL serial_control(PORT *port,DWORD code,PBYTE in,DWORD inSize,PBYTE out,DWORD outSize,PDWORD actual)
{
    BOOL ok=FALSE;DWORD error=ERROR_INVALID_PARAMETER,lines;DCB dcb;COMMTIMEOUTS timeouts;
    if(actual)*actual=0;
#ifdef COM_V8
    /* Wait must not hold the configuration lock: SetCommMask cancels it. */
    if(code==0x001b002c)return serial_wait_event(port,out,outSize,actual);
#endif
    EnterCriticalSection(&port->io);
    if(!serial_alive(port)){LeaveCriticalSection(&port->io);SetLastError(ERROR_DEVICE_NOT_CONNECTED);return FALSE;}
    logline(L"V4 IOCTL code=%08lx in=%lu out=%lu\r\n",code,inSize,outSize);
    switch(code) {
#ifdef COM_V8
    case 0x00223ffc: /* Private version query, not a CE serial IOCTL. */
        if(out && outSize>=sizeof(DWORD)){*(DWORD*)out=8;if(actual)*actual=sizeof(DWORD);ok=TRUE;}break;
    case 0x001b0024: /* GetCommMask, measured on this ROM. */
        if(out && outSize>=sizeof(DWORD)){memcpy(out,&port->eventMask,sizeof(DWORD));if(actual)*actual=sizeof(DWORD);ok=TRUE;}break;
    case 0x001b0028: /* SetCommMask: only RXCHAR and zero are supported. */
        if(!in || inSize!=sizeof(DWORD))break;
        memcpy(&lines,in,sizeof(DWORD));
        if(lines&~EV_RXCHAR){error=ERROR_NOT_SUPPORTED;break;}
        port->eventMask=lines;port->eventEpoch++;port->eventPending=0;ok=TRUE;break;
#endif
    case 0x001b0050: /* GetCommState */
        if(out && outSize>=sizeof(DCB)){memcpy(out,&port->dcb,sizeof(DCB));if(actual)*actual=sizeof(DCB);ok=TRUE;}break;
    case 0x001b0054: /* SetCommState: validate before changing physical control lines. */
        if(!in || inSize!=sizeof(DCB))break;
        memcpy(&dcb,in,sizeof(dcb));
        if(dcb.DCBlength!=sizeof(DCB))break;
        if(dcb.BaudRate!=115200 || dcb.ByteSize!=8 || dcb.Parity!=NOPARITY || dcb.StopBits!=ONESTOPBIT || !dcb.fBinary ||
          dcb.fParity || dcb.fOutxCtsFlow || dcb.fOutxDsrFlow || dcb.fDsrSensitivity || dcb.fOutX || dcb.fInX ||
          dcb.fErrorChar || dcb.fNull || dcb.fAbortOnError || dcb.fDtrControl>DTR_CONTROL_ENABLE || dcb.fRtsControl>RTS_CONTROL_ENABLE) {error=ERROR_NOT_SUPPORTED;break;}
        lines=(dcb.fDtrControl==DTR_CONTROL_ENABLE?1:0)|(dcb.fRtsControl==RTS_CONTROL_ENABLE?2:0);
        ok=serial_lines(port,lines);if(ok)port->dcb=dcb;else error=GetLastError();break;
    case 0x001b0040: /* GetCommTimeouts */
        if(out && outSize>=sizeof(COMMTIMEOUTS)){memcpy(out,&port->timeouts,sizeof(COMMTIMEOUTS));if(actual)*actual=sizeof(COMMTIMEOUTS);ok=TRUE;}break;
    case 0x001b003c: /* SetCommTimeouts: total-only or first-byte special mode. */
        if(!in || inSize!=sizeof(COMMTIMEOUTS))break;
        memcpy(&timeouts,in,sizeof(timeouts));
        if(!((timeouts.ReadIntervalTimeout==0 && timeouts.ReadTotalTimeoutMultiplier==0) ||
          (timeouts.ReadIntervalTimeout==MAXDWORD && timeouts.ReadTotalTimeoutMultiplier==MAXDWORD)) ||
          !timeouts.ReadTotalTimeoutConstant || timeouts.ReadTotalTimeoutConstant>10000 ||
          timeouts.WriteTotalTimeoutMultiplier!=0 || !timeouts.WriteTotalTimeoutConstant || timeouts.WriteTotalTimeoutConstant>10000) {error=ERROR_NOT_SUPPORTED;break;}
        port->timeouts=timeouts;ok=TRUE;break;
    case 0x001b000c:case 0x001b0010:case 0x001b0014:case 0x001b0018:
        lines=port->lines;
        if(code==0x001b000c)lines|=1;else if(code==0x001b0010)lines&=~1;
        else if(code==0x001b0014)lines|=2;else lines&=~2;
        ok=serial_lines(port,lines);if(!ok)error=GetLastError();break;
    default:error=ERROR_NOT_SUPPORTED;break;
    }
    LeaveCriticalSection(&port->io);SetLastError(ok?0:error);return ok;
}
