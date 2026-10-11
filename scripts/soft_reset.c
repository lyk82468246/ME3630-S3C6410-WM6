/* User-approved WM6 warm reset only; never sets a clean/cold boot flag. */
#include <windows.h>
#include <pm.h>
void mainCRTStartup(void)
{
    HANDLE log; DWORD bytes, result;
    const WCHAR start[]=L"SOFT_RESET requested via SetSystemPowerState RESET FORCE\r\n";
    RegFlushKey(HKEY_LOCAL_MACHINE);
    RegFlushKey(HKEY_CURRENT_USER);
    log=CreateFile(L"\\Temp\\ME3630-soft-reset.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    if(log!=INVALID_HANDLE_VALUE){WriteFile(log,start,sizeof(start)-2,&bytes,NULL);FlushFileBuffers(log);CloseHandle(log);}
    Sleep(2000);
    result=SetSystemPowerState(NULL,POWER_STATE_RESET,POWER_FORCE);
    ExitProcess(result);
}
