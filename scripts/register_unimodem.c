/* Scoped opt-in registration for our existing V18 MI2/COM5 only.
 * Uses vendor's Unimodem layout, changing only its baud to 115200.
 * Re-enumeration activates the registry configuration; no host reset/dial. */
#include <windows.h>
#undef wsprintfW
void mainCRTStartup(void)
{
    HKEY parent,key;DWORD result,disp,type=1,bytes,n;HANDLE log;WCHAR text[180];
    /* Vendor's 25-byte layout, baud changed to 115200. Other flags not decoded. */
    BYTE config[]={0x10,0,0,0,5,0,0,0,0x10,1,0,0,0,0xc2,1,0,0,0,8,0,0,0,0,0,0};
    result=RegOpenKeyEx(HKEY_LOCAL_MACHINE,L"Drivers\\ME3630ComV18\\Port2",0,0,&parent);
    if(!result) {
        result=RegSetValueEx(parent,L"IClass",0,REG_SZ,(LPBYTE)L"{CC5195AC-BA49-48a0-BE17-DF6D1B0173DD}",sizeof(L"{CC5195AC-BA49-48a0-BE17-DF6D1B0173DD}"));
        if(!result)result=RegCreateKeyEx(parent,L"Unimodem",0,NULL,0,0,NULL,&key,&disp);
        if(!result) {
            result=RegSetValueEx(key,L"Tsp",0,REG_SZ,(LPBYTE)L"Unimodem.dll",sizeof(L"Unimodem.dll"));
            if(!result)result=RegSetValueEx(key,L"DeviceType",0,REG_DWORD,(LPBYTE)&type,4);
            if(!result)result=RegSetValueEx(key,L"FriendlyName",0,REG_SZ,(LPBYTE)L"ME3630 Test COM5",sizeof(L"ME3630 Test COM5"));
            if(!result)result=RegSetValueEx(key,L"DevConfig",0,REG_BINARY,config,sizeof(config));
            RegCloseKey(key);
        }
        RegCloseKey(parent);
    }
    log=CreateFile(L"\\Temp\\ME3630-unimodem-register.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    if(log!=INVALID_HANDLE_VALUE){n=wsprintfW(text,L"UNIMODEM_REGISTER tick=%lu result=%lu path=V18/Port2 config_bytes=25 COMPLETE\r\n",GetTickCount(),result);WriteFile(log,text,n*2,&bytes,NULL);CloseHandle(log);}
    ExitProcess(result?1:0);
}
