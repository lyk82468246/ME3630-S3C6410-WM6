/* CE5.2 USB client: exact 19d2:1432 match, descriptor logging, AT on MI1/MI2.
 * This is a bounded transport probe, not yet a COM/PPP production driver. */
#include <windows.h>
#include <usbdi.h>
#include <stdarg.h>
#ifdef STREAM_V3
#include <devload.h>
#endif
#undef wsprintf
#undef wsprintfW
#undef wvsprintf
#undef wvsprintfW

typedef struct {
    USB_HANDLE device;
    LPCUSB_FUNCS usb;
    HANDLE stop, thread;
    USB_ENDPOINT_DESCRIPTOR in[2], out[2];
    DWORD iface[2], count;
#ifdef COM_V6
    USB_ENDPOINT_DESCRIPTOR adbIn,adbOut;
    BOOL adbFound;
#endif
#ifdef STREAM_V3
    void *ports[2];
    LONG refs;
#endif
} PROBE;

static void logline(LPCWSTR format,...)
{
    WCHAR text[768],stamp[64];
    HANDLE file;
    DWORD bytes;
    int n;
    va_list args;
    va_start(args,format);
    n=wvsprintfW(text,format,args);
    va_end(args);
#ifdef STREAM_V3
#ifdef COM_V4
#ifdef COM_V5
#ifdef COM_V6
#ifdef COM_V7
#ifdef COM_V8
    file=CreateFile(L"\\Temp\\ME3630-com-v8.txt",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
#else
    file=CreateFile(L"\\Temp\\ME3630-com-v7.txt",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
#endif
#else
    file=CreateFile(L"\\Temp\\ME3630-com-v6.txt",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
#endif
#else
    file=CreateFile(L"\\Temp\\ME3630-com-v5.txt",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
#endif
#else
    file=CreateFile(L"\\Temp\\ME3630-com-v4.txt",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
#endif
#else
    file=CreateFile(L"\\Temp\\ME3630-stream-v3.txt",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
#endif
#else
    file=CreateFile(L"\\Temp\\ME3630-usb-probe.txt",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
#endif
    if(file!=INVALID_HANDLE_VALUE) {
        SetFilePointer(file,0,NULL,FILE_END);
        bytes=wsprintfW(stamp,L"[tick=%lu] ",GetTickCount());
        WriteFile(file,stamp,bytes*sizeof(WCHAR),&bytes,NULL);
        WriteFile(file,text,n*sizeof(WCHAR),&bytes,NULL);
        CloseHandle(file);
    }
}

static BOOL wait_transfer(PROBE *p,USB_TRANSFER t,DWORD timeout,LPDWORD bytes)
{
    DWORD start=GetTickCount(),error=USB_NOT_COMPLETE_ERROR;
    BOOL complete;
    if(!t){logline(L"issue failed winerr=%lu\r\n",GetLastError());return FALSE;}
    while(!p->usb->lpIsTransferComplete(t)) {
        if(WaitForSingleObject(p->stop,10)==WAIT_OBJECT_0 || GetTickCount()-start>=timeout)break;
    }
    complete=p->usb->lpIsTransferComplete(t);
    if(!complete)p->usb->lpAbortTransfer(t,0);
    *bytes=0;
    p->usb->lpGetTransferStatus(t,bytes,&error);
    p->usb->lpCloseTransfer(t);
    logline(L"transfer complete=%lu bytes=%lu usb_error=%lu\r\n",complete,*bytes,error);
    return complete && error==USB_NO_ERROR;
}

#ifdef STREAM_V3
#ifdef COM_V4
#define M36_Init COM_Init
#define M36_Deinit COM_Deinit
#define M36_Open COM_Open
#define M36_Close COM_Close
#define M36_Read COM_Read
#define M36_Write COM_Write
#define M36_Seek COM_Seek
#define M36_IOControl COM_IOControl
#define M36_PowerUp COM_PowerUp
#define M36_PowerDown COM_PowerDown
#endif
#include "me3630_stream.inc"
#else
static DWORD WINAPI worker(LPVOID param)
{
    PROBE *p=(PROBE*)param;
    DWORD i,j,bytes;
    USB_PIPE in,out;
    USB_TRANSFER t;
    BYTE buffer[512];
    WCHAR text[520];
    static const BYTE command[]={'A','T','\r'};
    USB_DEVICE_REQUEST request;
    /* Power-cycle enumeration can precede readiness of the modem's SMD ports. */
    logline(L"WAIT_MODEM_READY 20s\r\n");
    if(WaitForSingleObject(p->stop,20000)==WAIT_OBJECT_0)return 0;
    for(i=0;i<p->count;i++) {
        if(WaitForSingleObject(p->stop,0)==WAIT_OBJECT_0)break;
        logline(L"TEST interface=%lu\r\n",p->iface[i]);
        in=p->usb->lpOpenPipe(p->device,&p->in[i]);
        out=p->usb->lpOpenPipe(p->device,&p->out[i]);
        logline(L"pipes in=%lu out=%lu winerr=%lu\r\n",in!=NULL,out!=NULL,GetLastError());
        if(in && out) {
            memset(&request,0,sizeof(request));
            request.bmRequestType=0x21;request.bRequest=0x22;
            request.wValue=3;request.wIndex=(USHORT)p->iface[i];
            logline(L"SET_DTR_RTS interface=%lu\r\n",p->iface[i]);
            t=p->usb->lpIssueVendorTransfer(p->device,NULL,NULL,USB_OUT_TRANSFER|USB_NO_WAIT|USB_SEND_TO_INTERFACE,&request,NULL,0);
            wait_transfer(p,t,1000,&bytes);
            /* Bulk reads allow short packets; all waits are bounded. */
            t=p->usb->lpIssueBulkTransfer(out,NULL,NULL,USB_OUT_TRANSFER|USB_NO_WAIT,sizeof(command),(LPVOID)command,0);
            if(wait_transfer(p,t,3000,&bytes)) {
                t=p->usb->lpIssueBulkTransfer(in,NULL,NULL,USB_IN_TRANSFER|USB_NO_WAIT|USB_SHORT_TRANSFER_OK,sizeof(buffer),buffer,0);
                if(wait_transfer(p,t,3000,&bytes)) {
                    if(bytes>sizeof(buffer))bytes=sizeof(buffer);
                    for(j=0;j<bytes;j++)text[j]=(WCHAR)buffer[j];
                    text[bytes]=0;
                    logline(L"RX interface=%lu [%s]\r\n",p->iface[i],text);
                }
            }
        }
        if(in)p->usb->lpClosePipe(in);
        if(out)p->usb->lpClosePipe(out);
    }
    logline(L"PROBE_COMPLETE\r\n");
    return 0;
}
#endif

static BOOL WINAPI detach(LPVOID ctx,DWORD code,LPDWORD *a,LPDWORD *b,LPDWORD *c,LPDWORD *d)
{
    PROBE *p=(PROBE*)ctx;
    DWORD waited=WAIT_OBJECT_0;
    if(code!=USB_CLOSE_DEVICE)return FALSE;
    SetEvent(p->stop);
    if(p->thread)waited=WaitForSingleObject(p->thread,10000);
    logline(L"DETACH wait=%lu\r\n",waited);
    /* Retain context if a host call did not return, avoiding a use-after-free. */
    if(waited==WAIT_OBJECT_0) {
        p->usb->lpUnRegisterNotificationRoutine(p->device,detach,p);
#ifdef STREAM_V3
        stream_cleanup(p);
        probe_release(p);
#else
        if(p->thread)CloseHandle(p->thread);
        CloseHandle(p->stop);
        LocalFree(p);
#endif
    }
    return TRUE;
}

BOOL USBDeviceAttach(USB_HANDLE device,LPCUSB_FUNCS usb,LPCUSB_INTERFACE selected,LPCWSTR id,LPBOOL accept,LPCUSB_DRIVER_SETTINGS settings,DWORD unused)
{
    LPCUSB_DEVICE info;
    LPCUSB_CONFIGURATION config;
    LPCUSB_INTERFACE iface;
    LPCUSB_ENDPOINT_DESCRIPTOR ep,in,out;
    PROBE *p;
    DWORD i,j;
    *accept=FALSE;
    info=usb->lpGetDeviceInfo(device);
    if(!info)return FALSE;
    logline(L"ATTACH vid=%04x pid=%04x interface=%ld funcs_size=%lu\r\n",info->Descriptor.idVendor,info->Descriptor.idProduct,selected?(LONG)selected->Descriptor.bInterfaceNumber:-1,usb->dwCount);
    if(info->Descriptor.idVendor!=0x19d2 || info->Descriptor.idProduct!=0x1432)return FALSE;
    config=info->lpActiveConfig;
    if(!config)return FALSE;
    p=(PROBE*)LocalAlloc(LPTR,sizeof(PROBE));
    if(!p)return FALSE;
    p->device=device;p->usb=usb;
#ifdef STREAM_V3
    p->refs=1;
#endif
    p->stop=CreateEvent(NULL,TRUE,FALSE,NULL);
    if(!p->stop){LocalFree(p);return FALSE;}
    logline(L"CONFIG interfaces=%lu\r\n",config->dwNumInterfaces);
    for(i=0;i<config->dwNumInterfaces;i++) {
        iface=&config->lpInterfaces[i];in=NULL;out=NULL;
        logline(L"IF number=%lu alt=%lu class=%02x subclass=%02x protocol=%02x endpoints=%lu\r\n",iface->Descriptor.bInterfaceNumber,iface->Descriptor.bAlternateSetting,iface->Descriptor.bInterfaceClass,iface->Descriptor.bInterfaceSubClass,iface->Descriptor.bInterfaceProtocol,iface->Descriptor.bNumEndpoints);
        for(j=0;j<iface->Descriptor.bNumEndpoints;j++) {
            ep=&iface->lpEndpoints[j].Descriptor;
            logline(L"EP address=%02x attributes=%02x packet=%lu\r\n",ep->bEndpointAddress,ep->bmAttributes,ep->wMaxPacketSize);
            if((ep->bmAttributes & 3)==2) {if(ep->bEndpointAddress & 0x80)in=ep;else out=ep;}
        }
        if(iface->Descriptor.bAlternateSetting==0 && (iface->Descriptor.bInterfaceNumber==1 || iface->Descriptor.bInterfaceNumber==2) && in && out && p->count<2) {
            p->iface[p->count]=iface->Descriptor.bInterfaceNumber;
            p->in[p->count]=*in;p->out[p->count]=*out;p->count++;
        }
#ifdef COM_V6
        if(iface->Descriptor.bAlternateSetting==0 && iface->Descriptor.bInterfaceNumber==4 &&
           iface->Descriptor.bInterfaceClass==0xff && iface->Descriptor.bInterfaceSubClass==0x42 &&
           iface->Descriptor.bInterfaceProtocol==1 && in && out) {
            p->adbIn=*in;p->adbOut=*out;p->adbFound=TRUE;
        }
#endif
    }
    if(!usb->lpRegisterNotificationRoutine(device,detach,p)) {CloseHandle(p->stop);LocalFree(p);return FALSE;}
    p->thread=CreateThread(NULL,0,worker,p,0,NULL);
    if(!p->thread) {
        usb->lpUnRegisterNotificationRoutine(device,detach,p);
        CloseHandle(p->stop);LocalFree(p);return FALSE;
    }
    *accept=TRUE;
    return TRUE;
}

BOOL USBInstallDriver(LPCWSTR dll)
{
    HMODULE usbd=LoadLibrary(L"usbd.dll");
    LPREGISTER_CLIENT_DRIVER_ID regid;
    LPREGISTER_CLIENT_SETTINGS regsettings;
    USB_DRIVER_SETTINGS s;
    BOOL ok=FALSE;
    if(!usbd)return FALSE;
    regid=(LPREGISTER_CLIENT_DRIVER_ID)GetProcAddressW(usbd,L"RegisterClientDriverID");
    regsettings=(LPREGISTER_CLIENT_SETTINGS)GetProcAddressW(usbd,L"RegisterClientSettings");
    memset(&s,0xff,sizeof(s));
    s.dwCount=sizeof(s);s.dwVendorId=0x19d2;s.dwProductId=0x1432;
    if(regid && regsettings && regid(L"ME3630Probe"))ok=regsettings(dll,L"ME3630Probe",NULL,&s);
    logline(L"INSTALL ok=%lu winerr=%lu\r\n",ok,GetLastError());
    FreeLibrary(usbd);
    return ok;
}
BOOL USBUnInstallDriver(void)
{
    HMODULE usbd=LoadLibrary(L"usbd.dll");
    LPUN_REGISTER_CLIENT_SETTINGS unreg;
    LPUN_REGISTER_CLIENT_DRIVER_ID unregid;
    USB_DRIVER_SETTINGS s;
    BOOL ok=FALSE;
    if(!usbd)return FALSE;
    memset(&s,0xff,sizeof(s));s.dwCount=sizeof(s);s.dwVendorId=0x19d2;s.dwProductId=0x1432;
    unreg=(LPUN_REGISTER_CLIENT_SETTINGS)GetProcAddressW(usbd,L"UnRegisterClientSettings");
    unregid=(LPUN_REGISTER_CLIENT_DRIVER_ID)GetProcAddressW(usbd,L"UnRegisterClientDriverID");
    if(unreg)ok=unreg(L"ME3630Probe",NULL,&s);
    if(unregid)unregid(L"ME3630Probe");
    FreeLibrary(usbd);return ok;
}
BOOL WINAPI DllMain(HANDLE module,DWORD reason,LPVOID reserved){return TRUE;}
