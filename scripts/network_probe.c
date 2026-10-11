/* Read-only registration/context and RAS-device discovery. No dialing. */
#include <windows.h>
#include <ras.h>
#include <raserror.h>
#include <stdarg.h>
#undef wsprintfW
#undef wvsprintfW
static HANDLE logFile;
static void emit(LPCWSTR fmt,...)
{
    WCHAR text[768];DWORD bytes;int n;va_list args;
    va_start(args,fmt);n=wvsprintfW(text,fmt,args);va_end(args);
    WriteFile(logFile,text,n*2,&bytes,NULL);FlushFileBuffers(logFile);
}
static BOOL has(const BYTE *data,DWORD bytes,const char *word)
{
    DWORD i,j,len=0;while(word[len])len++;
    for(i=0;i+len<=bytes;i++){for(j=0;j<len && data[i+j]==(BYTE)word[j];j++);if(j==len)return TRUE;}return FALSE;
}
static BOOL query(HANDLE port,const char *cmd,DWORD generation)
{
    BYTE response[2048];WCHAR chunk[257];DWORD begin=GetTickCount(),count=0,n,written,i,j,len=0,error;BOOL ok;
    while(cmd[len])len++;
    emit(L"QUERY gen=%lu cmd=",generation);for(i=0;i<len;i++)chunk[i]=(WCHAR)cmd[i];chunk[len]=0;emit(L"%s\r\n",chunk);
    if(!WriteFile(port,cmd,len,&written,NULL) || written!=len){emit(L"WRITE_ERROR=%lu\r\n",GetLastError());return FALSE;}
    while(count<sizeof(response) && GetTickCount()-begin<2200) {
        ok=ReadFile(port,response+count,sizeof(response)-count,&n,NULL);error=GetLastError();
        if(!ok){emit(L"READ_ERROR=%lu\r\n",error);return FALSE;}
        count+=n;if(has(response,count,"\r\nOK\r\n") || has(response,count,"ERROR"))break;
    }
    for(i=0;i<count;i+=256){n=count-i;if(n>256)n=256;for(j=0;j<n;j++)chunk[j]=(WCHAR)response[i+j];chunk[n]=0;emit(L"%s",chunk);}
    emit(L"\r\nQUERY_END bytes=%lu terminal=%lu\r\n",count,has(response,count,"OK") || has(response,count,"ERROR"));
    return has(response,count,"\r\nOK\r\n") || has(response,count,"ERROR");
}
static void ras_devices(void)
{
    typedef DWORD (WINAPI *ENUMDEV)(LPRASDEVINFOW,LPDWORD,LPDWORD);
    HMODULE module=LoadLibrary(L"coredll.dll");ENUMDEV enumerate=NULL;RASDEVINFOW *devices;DWORD bytes=sizeof(RASDEVINFOW),count=0,error,i;
    if(!module){emit(L"RAS_LOAD_ERROR=%lu\r\n",GetLastError());return;}
    enumerate=(ENUMDEV)GetProcAddressW(module,L"RasEnumDevicesW");if(!enumerate)enumerate=(ENUMDEV)GetProcAddressW(module,L"RasEnumDevices");
    if(!enumerate){FreeLibrary(module);module=LoadLibrary(L"ppp.dll");if(module){enumerate=(ENUMDEV)GetProcAddressW(module,L"RasEnumDevicesW");if(!enumerate)enumerate=(ENUMDEV)GetProcAddressW(module,L"RasEnumDevices");}}
    if(!enumerate){emit(L"RAS_ENUM_EXPORT_MISSING\r\n");if(module)FreeLibrary(module);return;}
    devices=(RASDEVINFOW*)LocalAlloc(LPTR,bytes);if(!devices){FreeLibrary(module);return;}devices[0].dwSize=sizeof(RASDEVINFOW);
    error=enumerate(devices,&bytes,&count);
    if(error==ERROR_BUFFER_TOO_SMALL && bytes<=65536) {
        LocalFree(devices);devices=(RASDEVINFOW*)LocalAlloc(LPTR,bytes);if(!devices){FreeLibrary(module);return;}
        devices[0].dwSize=sizeof(RASDEVINFOW);error=enumerate(devices,&bytes,&count);
    }
    emit(L"RAS_ENUM error=%lu count=%lu bytes=%lu\r\n",error,count,bytes);
    if(!error && count<=bytes/sizeof(RASDEVINFOW))for(i=0;i<count;i++)emit(L"RAS_DEVICE type=%s name=%s\r\n",devices[i].szDeviceType,devices[i].szDeviceName);
    LocalFree(devices);FreeLibrary(module);
}
void mainCRTStartup(void)
{
    HANDLE port=INVALID_HANDLE_VALUE;DWORD begin=GetTickCount(),version,actual,i,generation=0;BOOL completed=FALSE;COMMTIMEOUTS timeouts;
    static const char *commands[]={"AT\r","AT+CFUN?\r","AT+CPIN?\r","AT+CSQ\r","AT+COPS?\r","AT+CREG?\r","AT+CEREG?\r","AT+CGATT?\r","AT+CGDCONT?\r","AT+CGACT?\r"};
    logFile=CreateFile(L"\\Temp\\ME3630-network-probe.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(logFile==INVALID_HANDLE_VALUE)ExitProcess(2);
    emit(L"NETWORK_PROBE start_tick=%lu read_only=1 duration=60s\r\n",begin);ras_devices();
    while(GetTickCount()-begin<60000) {
        port=CreateFile(L"COM1:",GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
        if(port==INVALID_HANDLE_VALUE){Sleep(300);continue;}
        version=actual=0;
        if(!DeviceIoControl(port,0x00223ffc,NULL,0,&version,4,&actual,NULL) || version!=18 || actual!=4){CloseHandle(port);port=INVALID_HANDLE_VALUE;Sleep(300);continue;}
        generation++;emit(L"OPEN gen=%lu instance=%lu tick=%lu\r\n",generation,version,GetTickCount());
        memset(&timeouts,0,sizeof(timeouts));timeouts.ReadIntervalTimeout=MAXDWORD;timeouts.ReadTotalTimeoutMultiplier=MAXDWORD;timeouts.ReadTotalTimeoutConstant=400;timeouts.WriteTotalTimeoutConstant=1000;
        if(SetCommTimeouts(port,&timeouts)) {
            for(i=0;i<sizeof(commands)/sizeof(commands[0]) && GetTickCount()-begin<60000;i++)if(!query(port,commands[i],generation))break;
            completed=i==sizeof(commands)/sizeof(commands[0]);
        }
        CloseHandle(port);port=INVALID_HANDLE_VALUE;if(completed)break;Sleep(300);
    }
    emit(L"NETWORK_PROBE generations=%lu all_queries=%lu elapsed=%lu COMPLETE\r\n",generation,completed,GetTickCount()-begin);
    CloseHandle(logFile);ExitProcess(completed?0:1);
}
