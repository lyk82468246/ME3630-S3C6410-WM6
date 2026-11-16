/* USB_WATCH: observe-only CE5.2 USB client driver.
 * Registers with USBD for ALL devices/interfaces (USB_NO_INFO everywhere) and
 * logs full device/configuration/interface/endpoint descriptors on every
 * attach. Never claims the device: *accept stays FALSE, no pipes are opened,
 * no transfers are issued, no notification routine is registered. This keeps
 * the module available for the production ME3630ComV18D40 driver while we
 * identify what the WM6 OHCI host actually enumerates (hubs, dock, module).
 */
#include <windows.h>
#include <usbdi.h>
#include <stdarg.h>
#undef wsprintf
#undef wsprintfW
#undef wvsprintf
#undef wvsprintfW

static CRITICAL_SECTION traceLock;
static LONG traceInit;

static void logline(LPCWSTR format,...)
{
    WCHAR text[1024],stamp[64];
    HANDLE file;
    DWORD bytes;
    int n;
    va_list args;
    va_start(args,format);
    n=wvsprintfW(text,format,args);
    va_end(args);
    if(traceInit)EnterCriticalSection(&traceLock);
    file=CreateFile(L"\\Temp\\ME3630-usb-watch.txt",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file!=INVALID_HANDLE_VALUE) {
        SetFilePointer(file,0,NULL,FILE_END);
        bytes=wsprintfW(stamp,L"[tick=%lu] ",GetTickCount());
        WriteFile(file,stamp,bytes*sizeof(WCHAR),&bytes,NULL);
        WriteFile(file,text,n*sizeof(WCHAR),&bytes,NULL);
        CloseHandle(file);
    }
    if(traceInit)LeaveCriticalSection(&traceLock);
}

static const struct {DWORD value;LPCWSTR name;} class_names[]={
    {0x00,L"per-interface"},{0x01,L"audio"},{0x02,L"cdc"},{0x03,L"hid"},
    {0x05,L"physical"},{0x06,L"image"},{0x07,L"printer"},{0x08,L"mass_storage"},
    {0x09,L"HUB"},{0x0a,L"cdc-data"},{0x0b,L"smartcard"},{0x0d,L"security"},
    {0x0e,L"video"},{0x0f,L"healthcare"},{0x10,L"audio/video"},{0x11,L"billboard"},
    {0x12,L"usb_type_c"},{0xe0,L"wireless"},{0xef,L"misc"},{0xff,L"vendor"}
};

static LPCWSTR class_name(DWORD cls)
{
    DWORD i;
    for(i=0;i<sizeof(class_names)/sizeof(class_names[0]);i++)
        if(class_names[i].value==cls)return class_names[i].name;
    return L"other";
}

BOOL USBDeviceAttach(USB_HANDLE device,LPCUSB_FUNCS usb,LPCUSB_INTERFACE selected,
                     LPCWSTR id,LPBOOL accept,LPCUSB_DRIVER_SETTINGS settings,DWORD unused)
{
    LPCUSB_DEVICE info;
    const USB_CONFIGURATION_DESCRIPTOR *cfg;
    DWORD i,j,k;
    *accept=FALSE;
    info=usb->lpGetDeviceInfo(device);
    if(!info){logline(L"WATCH_ATTACH info=NULL winerr=%lu\r\n",GetLastError());return FALSE;}
    logline(L"ATTACH vid=%04x pid=%04x devclass=%02x(%s) devsub=%02x devproto=%02x bcdUSB=%04x maxpkt0=%lu configs=%lu selected_if=%ld\r\n",
        info->Descriptor.idVendor,info->Descriptor.idProduct,
        info->Descriptor.bDeviceClass,class_name(info->Descriptor.bDeviceClass),
        info->Descriptor.bDeviceSubClass,info->Descriptor.bDeviceProtocol,
        info->Descriptor.bcdUSB,(DWORD)info->Descriptor.bMaxPacketSize0,
        (DWORD)info->Descriptor.bNumConfigurations,
        selected?(LONG)selected->Descriptor.bInterfaceNumber:-1);
    for(k=0;k<info->Descriptor.bNumConfigurations && k<4;k++) {
        LPCUSB_CONFIGURATION config=&info->lpConfigs[k];
        if(!config)continue;
        cfg=&config->Descriptor;
        logline(L"CONFIG[%lu] value=%lu interfaces=%lu attrs=%02x maxpower=%lumA totallen=%lu active=%lu\r\n",
            k,(DWORD)cfg->bConfigurationValue,config->dwNumInterfaces,
            cfg->bmAttributes,(DWORD)cfg->MaxPower,(DWORD)cfg->wTotalLength,
            (DWORD)(config==info->lpActiveConfig));
        for(i=0;i<config->dwNumInterfaces;i++) {
            LPCUSB_INTERFACE iface=&config->lpInterfaces[i];
            logline(L" IF number=%lu alt=%lu class=%02x(%s) sub=%02x proto=%02x eps=%lu\r\n",
                (DWORD)iface->Descriptor.bInterfaceNumber,(DWORD)iface->Descriptor.bAlternateSetting,
                (DWORD)iface->Descriptor.bInterfaceClass,class_name(iface->Descriptor.bInterfaceClass),
                (DWORD)iface->Descriptor.bInterfaceSubClass,(DWORD)iface->Descriptor.bInterfaceProtocol,
                (DWORD)iface->Descriptor.bNumEndpoints);
            for(j=0;j<iface->Descriptor.bNumEndpoints;j++) {
                const USB_ENDPOINT_DESCRIPTOR *ep=&iface->lpEndpoints[j].Descriptor;
                logline(L"  EP addr=%02x attr=%02x(%s) pkt=%lu interval=%lu\r\n",
                    (DWORD)ep->bEndpointAddress,(DWORD)ep->bmAttributes,
                    ((ep->bmAttributes&3)==0)?L"ctrl":((ep->bmAttributes&3)==1)?L"iso":
                    ((ep->bmAttributes&3)==2)?L"bulk":L"interrupt",
                    (DWORD)ep->wMaxPacketSize,(DWORD)ep->bInterval);
            }
        }
    }
    logline(L"WATCH_DONE accept=0\r\n");
    return FALSE;
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
    s.dwCount=sizeof(s);
    if(regid && regsettings && regid(L"ME3630UsbWatch"))ok=regsettings(dll,L"ME3630UsbWatch",NULL,&s);
    logline(L"WATCH_INSTALL ok=%lu winerr=%lu\r\n",ok,GetLastError());
    FreeLibrary(usbd);
    return ok;
}

BOOL USBUnInstallDriver(void)
{
    HMODULE usbd=LoadLibrary(L"usbd.dll");
    LPUN_REGISTER_CLIENT_SETTINGS unreg;
    LPUN_REGISTER_CLIENT_DRIVER_ID unregid;
    BOOL ok=FALSE;
    if(!usbd)return FALSE;
    unreg=(LPUN_REGISTER_CLIENT_SETTINGS)GetProcAddressW(usbd,L"UnRegisterClientSettings");
    unregid=(LPUN_REGISTER_CLIENT_DRIVER_ID)GetProcAddressW(usbd,L"UnRegisterClientDriverID");
    if(unreg)ok=unreg(L"ME3630UsbWatch",NULL,NULL);
    if(unregid)unregid(L"ME3630UsbWatch");
    logline(L"WATCH_UNINSTALL ok=%lu\r\n",ok);
    FreeLibrary(usbd);
    return ok;
}

BOOL WINAPI DllMain(HANDLE module,DWORD reason,LPVOID reserved)
{
    if(reason==DLL_PROCESS_ATTACH)InitializeCriticalSection(&traceLock);
    if(reason==DLL_PROCESS_DETACH)DeleteCriticalSection(&traceLock);
    return TRUE;
}
