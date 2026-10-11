/* V16 read-only interrupt capture. No modem-line emulation or bulk data.
 * One driver-owned buffer/worker per pipe; detach joins before pipe closure.
 * Keep a pending interrupt IN for up to 30s, avoiding rapid idle abort loops.
 * Raw packets only: interpreting state requires actual firmware evidence. */
static DWORD WINAPI serial_notify_probe(LPVOID param)
{
    PORT *port=(PORT*)param;PROBE *p=port->parent;
    BYTE data[64];WCHAR hex[193];USB_TRANSFER t;
    DWORD begin,bytes,error,i,packets=0,timeouts=0;BOOL complete;
#ifdef COM_V17
    DWORD raw,status;BOOL valid;
#endif
    logline(L"V16 NOTIFY_START interface=%lu size=%lu\r\n",port->index,port->notifySize);
    while(WaitForSingleObject(p->stop,0)!=WAIT_OBJECT_0) {
        memset(data,0,sizeof(data));
        t=p->usb->lpIssueInterruptTransfer(port->notify,NULL,NULL,
          USB_NO_WAIT|USB_IN_TRANSFER|USB_SHORT_TRANSFER_OK,port->notifySize,data,0);
        if(!t){logline(L"V16 NOTIFY_ISSUE_FAILED interface=%lu error=%lu\r\n",port->index,GetLastError());break;}
        begin=GetTickCount();
        while(!p->usb->lpIsTransferComplete(t)) {
            if(WaitForSingleObject(p->stop,20)==WAIT_OBJECT_0 || GetTickCount()-begin>=30000)break;
        }
        complete=p->usb->lpIsTransferComplete(t);
        if(!complete)p->usb->lpAbortTransfer(t,0);
        bytes=0;error=USB_NOT_COMPLETE_ERROR;
        p->usb->lpGetTransferStatus(t,&bytes,&error);p->usb->lpCloseTransfer(t);
        if(WaitForSingleObject(p->stop,0)==WAIT_OBJECT_0)break;
        if(!complete){timeouts++;logline(L"V16 NOTIFY_IDLE interface=%lu count=%lu\r\n",port->index,timeouts);continue;}
        if(error!=USB_NO_ERROR || bytes>port->notifySize){logline(L"V16 NOTIFY_USB_ERROR interface=%lu bytes=%lu usb_error=%lu\r\n",port->index,bytes,error);break;}
        for(i=0;i<bytes;i++)wsprintfW(hex+i*3,L"%02x ",data[i]);hex[bytes*3]=0;
        packets++;logline(L"V16 NOTIFY_PACKET interface=%lu number=%lu bytes=%lu hex=%s\r\n",port->index,packets,bytes,hex);
#ifdef COM_V17
        valid=m36_parse_notify(data,bytes,port->index,&raw,&status);
        EnterCriticalSection(&port->io);
#ifdef COM_V18
        m36_update_notify(&port->notifyState,data,bytes,port->index,GetTickCount(),port->eventMask,&port->eventPending);
#ifdef COM_DIAG35
        if(valid)port->commErrors|=m36_notify_errors(raw);
#endif
        if(port->eventPending&port->eventMask)SetEvent(port->eventWake);
#else
        if(valid) {
            port->notifyState.raw=raw;port->notifyState.status=status;
            port->notifyState.known=M36_NOTIFY_KNOWN;port->notifyState.accepted++;
            port->notifyState.tick=GetTickCount();port->notifyState.error=0;
        } else port->notifyState.rejected++;
#endif
        LeaveCriticalSection(&port->io);
        logline(L"V17 NOTIFY_PARSED interface=%lu valid=%lu raw=%02lx status=%02lx\r\n",port->index,valid,valid?raw:0,valid?status:0);
#endif
        /* Protect against a zero-length completion busy loop. */
        if(!bytes && WaitForSingleObject(p->stop,20)==WAIT_OBJECT_0)break;
    }
    logline(L"V16 NOTIFY_EXIT interface=%lu packets=%lu idle=%lu\r\n",port->index,packets,timeouts);
#ifdef COM_V17
    EnterCriticalSection(&port->io);port->notifyState.known=0;
    port->notifyState.error=WaitForSingleObject(p->stop,0)==WAIT_OBJECT_0?ERROR_DEVICE_NOT_CONNECTED:ERROR_IO_DEVICE;
#ifdef COM_V18
    SetEvent(port->eventWake);
#endif
    LeaveCriticalSection(&port->io);
#endif
    port_release(port);return 0;
}
