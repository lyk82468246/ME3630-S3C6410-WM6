/* Codes measured using native APIs on CE5.2 build20782; see ioctl-abi.txt.
 * V4 deliberately supports a bounded subset, NOT Unimodem/PPP yet.
 * V4-V6 serialize I/O; V7 separates RX/TX and snapshots configuration. */
#ifdef COM_DIAG35
#include "me3630_timeout.h"
#endif
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
    PROBE *p=port->parent;USB_TRANSFER t;DWORD start=GetTickCount(),error=USB_NOT_COMPLETE_ERROR;BOOL complete,expired=FALSE;
    *bytes=0;
#ifdef COM_DIAG38
    LIFE(M38_TRANSFER_ISSUE,port,size,port->index|(input?0x100:0x200));
#endif
#ifdef COM_DIAG35
    if(!input){
        EnterCriticalSection(&port->io);
        if(port->txIssueEpoch!=port->txClearEpoch){LeaveCriticalSection(&port->io);return 2;}
        t=p->usb->lpIssueBulkTransfer(pipe,NULL,NULL,USB_NO_WAIT|USB_OUT_TRANSFER,size,buf,0);
        LeaveCriticalSection(&port->io);
    }else
#endif
    t=p->usb->lpIssueBulkTransfer(pipe,NULL,NULL,USB_NO_WAIT|(input?(USB_IN_TRANSFER|USB_SHORT_TRANSFER_OK):USB_OUT_TRANSFER),size,buf,0);
#ifdef COM_DIAG38
    LIFE(M38_TRANSFER_SUBMITTED,port,t,t?0:GetLastError());
#endif
    if(!t){SetLastError(ERROR_IO_DEVICE);return -1;}
    while(!p->usb->lpIsTransferComplete(t)) {
        if(WaitForSingleObject(p->stop,1)==WAIT_OBJECT_0)break;
        if(GetTickCount()-start>=timeout){expired=TRUE;break;}
    }
    complete=p->usb->lpIsTransferComplete(t);
#ifdef COM_DIAG37
    life_finish_transfer(p,t,complete,port->index|(input?0x100:0x200),bytes,&error);
#else
    if(!complete)p->usb->lpAbortTransfer(t,0);
    p->usb->lpGetTransferStatus(t,bytes,&error);p->usb->lpCloseTransfer(t);
#endif
    if(WaitForSingleObject(p->stop,0)==WAIT_OBJECT_0){SetLastError(ERROR_DEVICE_NOT_CONNECTED);return -1;}
    if(complete && error==USB_NO_ERROR && *bytes<=size)return 1;
    *bytes=0;
    if(expired){SetLastError(0);return 0;}
    SetLastError(ERROR_IO_DEVICE);return -1;
}
#ifdef COM_V9
#include "me3630_ring.inc"
#include "me3630_queue.inc"
#endif
static DWORD serial_io(PORT *port,LPVOID buffer,DWORD count,BOOL input)
{
    DWORD start,limit,total=0,n,bytes;BOOL first;int result;
#ifdef COM_DIAG35
    COMMTIMEOUTS snapshot;DWORD clearEpoch;
#endif
#ifdef COM_V13
    BYTE *stage=port->txStage;
#else
    BYTE stage[512];
#endif
#ifdef COM_V7
    CRITICAL_SECTION *direction=input?&port->rxIO:&port->txIO;
#endif
    if(!count)return 0;
#ifdef COM_V9
    if(input)return serial_queue_read(port,buffer,count);
#endif
#ifdef COM_V7
#ifdef COM_DIAG35
    EnterCriticalSection(&port->io);clearEpoch=port->txClearEpoch;LeaveCriticalSection(&port->io);
#endif
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
#ifdef COM_DIAG35
    snapshot=port->timeouts;
    port->txIssueEpoch=clearEpoch;
    if(!m36_timeout_total(snapshot.WriteTotalTimeoutMultiplier,snapshot.WriteTotalTimeoutConstant,count,&limit)){LeaveCriticalSection(&port->io);LeaveCriticalSection(direction);SetLastError(ERROR_INVALID_PARAMETER);return (DWORD)-1;}
#endif
    first=port->timeouts.ReadIntervalTimeout==MAXDWORD;
#ifdef COM_V15
    port->txQueued=count;
#endif
#ifdef COM_V7
    /* Per-call configuration snapshot; control remains independent of bulk I/O.
     * The direction lock keeps this pipe/buffer alive until transfer completes. */
    LeaveCriticalSection(&port->io);
#endif
    while(total<count) {
#ifdef COM_DIAG35
        EnterCriticalSection(&port->io);n=clearEpoch!=port->txClearEpoch;LeaveCriticalSection(&port->io);if(n)break;
#endif
        if(input && port->rxLen>port->rxPos) {
            n=port->rxLen-port->rxPos;if(n>count-total)n=count-total;
            memcpy((PBYTE)buffer+total,port->rx+port->rxPos,n);port->rxPos+=n;total+=n;
            if(first || total==count)break;
        }
        n=GetTickCount()-start;if(limit!=INFINITE && n>=limit)break;
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
            bytes=count-total;if(bytes>512)bytes=512;
            memcpy(stage,(PBYTE)buffer+total,bytes);
#ifdef COM_V12
            logline(L"Q_TX port=%lu tid=%lu size=%lu expected_AT=%lu phase=%ld cycles=%ld timeouts=%ld\r\n",port->index,GetCurrentThreadId(),bytes,bytes==3 && memcmp(stage,"AT\r",3)==0,port->pumpPhase,port->pumpCycles,port->pumpTimeouts);
#endif
#ifdef COM_DIAG35
            result=serial_transfer(port,port->out,stage,bytes,FALSE,limit==INFINITE?5000:limit-n,&bytes);
#else
            result=serial_transfer(port,port->out,stage,bytes,FALSE,limit-n,&bytes);
#endif
            if(result==1)total+=bytes;
#ifdef COM_DIAG35
            if(result==2)break;
#endif
#ifdef COM_V15
            EnterCriticalSection(&port->io);port->txQueued=result<0?0:count-total;
#ifdef COM_DIAG35
            if(clearEpoch!=port->txClearEpoch)port->txQueued=0;
#endif
            LeaveCriticalSection(&port->io);
#endif
        }
        if(result<0){
#ifdef COM_V7
            LeaveCriticalSection(direction);
#else
            LeaveCriticalSection(&port->io);
#endif
            return (DWORD)-1;
        }
        if(result==0){
#ifdef COM_DIAG35
            if(limit==INFINITE)continue;
#endif
            break;
        }
    }
#ifdef COM_V15
    EnterCriticalSection(&port->io);port->txQueued=0;LeaveCriticalSection(&port->io);
#endif
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
#ifdef COM_DIAG35
#include "me3630_purge.inc"
#endif
#ifdef COM_DIAG36
#include "me3630_mask.inc"
#endif
static BOOL serial_control(PORT *port,DWORD code,PBYTE in,DWORD inSize,PBYTE out,DWORD outSize,PDWORD actual)
{
    BOOL ok=FALSE;DWORD error=ERROR_INVALID_PARAMETER,lines;DCB dcb;COMMTIMEOUTS timeouts;
#ifdef COM_DIAG33
    logline(L"D33 IO_ENTER port=%lu pid=%lu code=%08lx in=%lu out=%lu\r\n",port->index,GetCurrentProcessId(),code,inSize,outSize);
#ifdef COM_DIAG34R2
    if(in && inSize==4 && (code==0x001b0028 || code==0x001b0044)){DWORD value;memcpy(&value,in,4);logline(L"D34R2 VALUE code=%08lx value=%08lx\r\n",code,value);}
    if(in && inSize==sizeof(DCB) && code==0x001b0054){DCB value;DWORD flags;memcpy(&value,in,sizeof(value));memcpy(&flags,in+8,4);logline(L"D34R2 DCB length=%lu baud=%lu flags=%08lx byte=%lu parity=%lu stop=%lu\r\n",value.DCBlength,value.BaudRate,flags,value.ByteSize,value.Parity,value.StopBits);}
    if(in && inSize==sizeof(COMMTIMEOUTS) && code==0x001b003c){COMMTIMEOUTS t;memcpy(&t,in,sizeof(t));logline(L"D34R2 TIMEOUTS interval=%lu r_mult=%lu r_const=%lu w_mult=%lu w_const=%lu\r\n",t.ReadIntervalTimeout,t.ReadTotalTimeoutMultiplier,t.ReadTotalTimeoutConstant,t.WriteTotalTimeoutMultiplier,t.WriteTotalTimeoutConstant);}
#endif
#endif
    if(actual)*actual=0;
#ifdef COM_V8
    /* Wait must not hold the configuration lock: SetCommMask cancels it. */
    if(code==0x001b002c)return serial_wait_event(port,out,outSize,actual);
#endif
    EnterCriticalSection(&port->io);
    if(!serial_alive(port)){LeaveCriticalSection(&port->io);SetLastError(ERROR_DEVICE_NOT_CONNECTED);return FALSE;}
    logline(L"V4 IOCTL code=%08lx in=%lu out=%lu\r\n",code,inSize,outSize);
    switch(code) {
#ifdef COM_DIAG34
    case 0x00223ff4:
        if(!inSize && out && outSize==4){
#ifdef COM_DIAG38
            *(DWORD*)out=38;
#elif defined(COM_DIAG37)
            *(DWORD*)out=37;
#elif defined(COM_DIAG36)
            *(DWORD*)out=36;
#elif defined(COM_DIAG35)
            *(DWORD*)out=35;
#elif defined(COM_DIAG34R2)
            *(DWORD*)out=3402;
#else
            *(DWORD*)out=34;
#endif
            if(actual)*actual=4;ok=TRUE;}break;
#endif
#ifdef COM_V17
    case M36_NOTIFY_QUERY: /* Partial state with explicit validity, NOT a modem IOCTL. */
        if(!inSize && out && outSize==sizeof(M36_NOTIFY_STATE)) {
            memcpy(out,&port->notifyState,sizeof(M36_NOTIFY_STATE));
            if(actual)*actual=sizeof(M36_NOTIFY_STATE);ok=TRUE;
        }break;
#endif
#ifdef COM_V8
    case 0x00223ffc: /* Private version query, not a CE serial IOCTL. */
#ifdef COM_V9
#ifdef COM_V10
#ifdef COM_V11
#ifdef COM_V12
#ifdef COM_V13
#ifdef COM_V14
#ifdef COM_V15
#ifdef COM_V16
#ifdef COM_V17
#ifdef COM_V18
        if(out && outSize>=sizeof(DWORD)){*(DWORD*)out=18;if(actual)*actual=sizeof(DWORD);ok=TRUE;}break;
#else
        if(out && outSize>=sizeof(DWORD)){*(DWORD*)out=17;if(actual)*actual=sizeof(DWORD);ok=TRUE;}break;
#endif
#else
        if(out && outSize>=sizeof(DWORD)){*(DWORD*)out=16;if(actual)*actual=sizeof(DWORD);ok=TRUE;}break;
#endif
#else
        if(out && outSize>=sizeof(DWORD)){*(DWORD*)out=15;if(actual)*actual=sizeof(DWORD);ok=TRUE;}break;
#endif
#else
        if(out && outSize>=sizeof(DWORD)){*(DWORD*)out=14;if(actual)*actual=sizeof(DWORD);ok=TRUE;}break;
#endif
#else
        if(out && outSize>=sizeof(DWORD)){*(DWORD*)out=13;if(actual)*actual=sizeof(DWORD);ok=TRUE;}break;
#endif
#else
        if(out && outSize>=sizeof(DWORD)){*(DWORD*)out=12;if(actual)*actual=sizeof(DWORD);ok=TRUE;}break;
#endif
#else
        if(out && outSize>=sizeof(DWORD)){*(DWORD*)out=11;if(actual)*actual=sizeof(DWORD);ok=TRUE;}break;
#endif
#else
        if(out && outSize>=sizeof(DWORD)){*(DWORD*)out=10;if(actual)*actual=sizeof(DWORD);ok=TRUE;}break;
#endif
#else
        if(out && outSize>=sizeof(DWORD)){*(DWORD*)out=9;if(actual)*actual=sizeof(DWORD);ok=TRUE;}break;
#endif
#else
        if(out && outSize>=sizeof(DWORD)){*(DWORD*)out=8;if(actual)*actual=sizeof(DWORD);ok=TRUE;}break;
#endif
    case 0x001b0024: /* GetCommMask, measured on this ROM. */
        if(out && outSize>=sizeof(DWORD)){memcpy(out,&port->eventMask,sizeof(DWORD));if(actual)*actual=sizeof(DWORD);ok=TRUE;}break;
    case 0x001b0028: /* SetCommMask: only RXCHAR and zero are supported. */
        if(!in || inSize!=sizeof(DWORD))break;
        memcpy(&lines,in,sizeof(DWORD));
#ifdef COM_V18
#ifdef COM_DIAG35
        if(!m36_event_mask_supported(lines)){error=ERROR_NOT_SUPPORTED;break;}
#else
        if(lines&~(EV_RXCHAR|M36_LINE_EVENTS)){error=ERROR_NOT_SUPPORTED;break;}
#endif
#else
        if(lines&~EV_RXCHAR){error=ERROR_NOT_SUPPORTED;break;}
#endif
#ifdef COM_DIAG36
        ok=m36_set_event_mask(port,lines);
#ifdef COM_DIAG38
        LIFE(M38_MASK_STATE,port,port->eventMask,port->eventPending);
#endif
        logline(L"D36 MASK port=%lu mask=%08lx pending=%08lx queued=%lu epoch=%lu\r\n",port->index,lines,port->eventPending,port->qCount,port->eventEpoch);
        break;
#else
        port->eventMask=lines;port->eventEpoch++;port->eventPending=0;
#ifdef COM_V9
        SetEvent(port->eventWake);
#endif
        ok=TRUE;break;
#endif
#endif
#ifdef COM_V15
    case 0x001b0030: /* Measured ABI: DWORD errors + 12-byte COMSTAT. */
        if(out && outSize>=sizeof(DWORD)+sizeof(COMSTAT)) {
            COMSTAT stat;DWORD errors=port->rxFailure?CE_IOE:0;
#ifdef COM_DIAG35
            errors|=port->commErrors;port->commErrors=0;
#endif
            memset(&stat,0,sizeof(stat));stat.cbInQue=port->qCount;stat.cbOutQue=port->txQueued;
            memcpy(out,&errors,sizeof(errors));memcpy(out+sizeof(errors),&stat,sizeof(stat));
            if(actual)*actual=sizeof(errors)+sizeof(stat);ok=TRUE;
        }break;
    case 0x001b0044: /* Only RX actions; validate entire mask before mutation. */
        if(!in || inSize!=sizeof(DWORD))break;
        memcpy(&lines,in,sizeof(lines));if(!lines)break;
#ifdef COM_DIAG35
        ok=m36_purge_host_buffers(port,lines);if(!ok)error=ERROR_NOT_SUPPORTED;break;
#else
        if(lines&~(PURGE_RXCLEAR|PURGE_RXABORT)){error=ERROR_NOT_SUPPORTED;break;}
#endif
        if(lines&PURGE_RXCLEAR) {
            port->qHead=port->qCount=0;port->eventPending&=~EV_RXCHAR;
            ResetEvent(port->rxReady);if(!(port->eventPending&port->eventMask))ResetEvent(port->eventWake);
        }
        if(lines&PURGE_RXABORT){port->rxAbortEpoch++;SetEvent(port->rxReady);}
        ok=TRUE;break;
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
#ifdef COM_DIAG35
        if(!m36_timeout_valid(&timeouts)){error=ERROR_NOT_SUPPORTED;break;}
#else
        if(!((timeouts.ReadIntervalTimeout==0 && timeouts.ReadTotalTimeoutMultiplier==0) ||
          (timeouts.ReadIntervalTimeout==MAXDWORD && timeouts.ReadTotalTimeoutMultiplier==MAXDWORD)) ||
          !timeouts.ReadTotalTimeoutConstant || timeouts.ReadTotalTimeoutConstant>10000 ||
          timeouts.WriteTotalTimeoutMultiplier!=0 || !timeouts.WriteTotalTimeoutConstant || timeouts.WriteTotalTimeoutConstant>10000) {error=ERROR_NOT_SUPPORTED;break;}
#endif
        port->timeouts=timeouts;ok=TRUE;break;
    case 0x001b000c:case 0x001b0010:case 0x001b0014:case 0x001b0018:
        lines=port->lines;
        if(code==0x001b000c)lines|=1;else if(code==0x001b0010)lines&=~1;
        else if(code==0x001b0014)lines|=2;else lines&=~2;
        ok=serial_lines(port,lines);if(!ok)error=GetLastError();break;
    default:error=ERROR_NOT_SUPPORTED;break;
    }
#ifdef COM_DIAG33
    logline(L"D33 IO_RETURN port=%lu code=%08lx ok=%lu error=%lu\r\n",port->index,code,ok,ok?0:error);
#endif
    LeaveCriticalSection(&port->io);SetLastError(ok?0:error);return ok;
}
