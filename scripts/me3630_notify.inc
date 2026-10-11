/* V16 read-only interrupt capture. No modem-line emulation or bulk data.
 * One driver-owned buffer/worker per pipe; detach joins before pipe closure.
 * Keep a pending interrupt IN for up to 30s, avoiding rapid idle abort loops.
 * Raw packets only: interpreting state requires actual firmware evidence. */
static DWORD WINAPI serial_notify_probe(LPVOID param)
{
    PORT *port=(PORT*)param;PROBE *p=port->parent;
    BYTE data[64];WCHAR hex[193];USB_TRANSFER t;
    DWORD begin,bytes,error,i,packets=0,timeouts=0;BOOL complete;
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
        /* Protect against a zero-length completion busy loop. */
        if(!bytes && WaitForSingleObject(p->stop,20)==WAIT_OBJECT_0)break;
    }
    logline(L"V16 NOTIFY_EXIT interface=%lu packets=%lu idle=%lu\r\n",port->index,packets,timeouts);
    port_release(port);return 0;
}
