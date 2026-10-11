/* Bounded RAS attempt, ONLY our uniquely named ME3630 modem. No Cellular Line. */
#include <windows.h>
#include <ras.h>
#include <raserror.h>
#undef wsprintfW
static HANDLE file;static LONG terminal,callbackError;
static void emit(LPCWSTR label,DWORD a,DWORD b)
{
    WCHAR text[180];DWORD n,bytes;n=wsprintfW(text,L"%s a=%lu b=%lu tick=%lu\r\n",label,a,b,GetTickCount());WriteFile(file,text,n*2,&bytes,NULL);FlushFileBuffers(file);
}
static LRESULT CALLBACK notify(HWND window,UINT message,WPARAM state,LPARAM error)
{
    if(message!=WM_RASDIALEVENT)return DefWindowProc(window,message,state,error);
    if(error)InterlockedExchange(&callbackError,error);
    if(error || state==RASCS_Connected || state==RASCS_Disconnected)InterlockedExchange(&terminal,1);
    return 0;
}
void mainCRTStartup(void)
{
    RASDEVINFOW devices[8];RASENTRY entry;RASDIALPARAMS params;RASCONNSTATUS status;RASPPPIP projection;
    DWORD bytes,count,error,i,attempt,begin,allBegin=GetTickCount();BOOL found,connected=FALSE,created=FALSE;HRASCONN connection=NULL;
    BYTE config[]={0x10,0,0,0,5,0,0,0,0x10,1,0,0,0,0xc2,1,0,0,0,8,0,0,0,0,0,0};
    WCHAR entryName[64];WNDCLASS wc;HWND window=NULL;MSG msg;
    file=CreateFile(L"\\Temp\\ME3630-ras-attempt.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(file==INVALID_HANDLE_VALUE)ExitProcess(2);
    emit(L"RAS_ATTEMPT_START",allBegin,0);wsprintfW(entryName,L"ME3630-Test-%lu",allBegin);
    memset(&wc,0,sizeof(wc));wc.lpfnWndProc=notify;wc.hInstance=GetModuleHandle(NULL);wc.lpszClassName=L"ME3630RasAttemptNotify";
    if(RegisterClass(&wc))window=CreateWindow(wc.lpszClassName,L"",WS_POPUP,0,0,0,0,NULL,NULL,wc.hInstance,NULL);
    if(!window){emit(L"WINDOW_ERROR",GetLastError(),0);emit(L"RAS_ATTEMPT_COMPLETE",0,GetTickCount()-allBegin);CloseHandle(file);ExitProcess(1);}
    for(attempt=1;attempt<=3 && GetTickCount()-allBegin<60000;attempt++) {
        found=FALSE;begin=GetTickCount();
        do {
            memset(devices,0,sizeof(devices));devices[0].dwSize=sizeof(RASDEVINFOW);bytes=sizeof(devices);count=0;
            error=RasEnumDevices(devices,&bytes,&count);emit(L"ENUM",error,count);
            if(!error && count<=8)for(i=0;i<count;i++)if(!lstrcmpW(devices[i].szDeviceName,L"ME3630 Test COM5") && !lstrcmpW(devices[i].szDeviceType,RASDT_Modem)){found=TRUE;break;}
            if(!found)Sleep(1500);
        }while(!found && GetTickCount()-begin<15000 && GetTickCount()-allBegin<60000);
        if(!found){emit(L"OUR_MODEM_NOT_ENUMERATED",attempt,0);continue;}
        if(!created) {
            memset(&entry,0,sizeof(entry));entry.dwSize=sizeof(entry);entry.dwfOptions=RASEO_RemoteDefaultGateway|RASEO_DialAsLocalCall;
            entry.dwfNetProtocols=RASNP_Ip;entry.dwFramingProtocol=RASFP_Ppp;entry.dwCountryCode=86;
            lstrcpyW(entry.szDeviceType,RASDT_Modem);lstrcpyW(entry.szDeviceName,L"ME3630 Test COM5");lstrcpyW(entry.szLocalPhoneNumber,L"*99***2#");
            error=RasValidateEntryName(NULL,entryName);emit(L"VALIDATE_ENTRY",error,0);if(error)break;
            error=RasSetEntryProperties(NULL,entryName,&entry,sizeof(entry),config,sizeof(config));emit(L"CREATE_ENTRY",error,0);if(error)break;created=TRUE;
        }
        memset(&params,0,sizeof(params));params.dwSize=sizeof(params);lstrcpyW(params.szEntryName,entryName);
        InterlockedExchange(&terminal,0);InterlockedExchange(&callbackError,0);connection=NULL;
        error=RasDial(NULL,NULL,&params,0xffffffff,(LPVOID)window,&connection);emit(L"DIAL_START",error,(DWORD)connection);begin=GetTickCount();
        if(!error && connection)do {
            while(PeekMessage(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);}
            memset(&status,0,sizeof(status));status.dwSize=sizeof(status);error=RasGetConnectStatus(connection,&status);emit(L"DIAL_STATE",error,!error?status.rasconnstate:0);
            if(!error && status.rasconnstate==RASCS_Connected){connected=TRUE;break;}
            if(error || status.dwError || InterlockedCompareExchange(&terminal,0,0))break;Sleep(250);
        }while(GetTickCount()-begin<15000 && GetTickCount()-allBegin<60000);
        emit(L"DIAL_CALLBACK_ERROR",InterlockedCompareExchange(&callbackError,0,0),attempt);
        if(connected) {
            memset(&projection,0,sizeof(projection));projection.dwSize=sizeof(projection);bytes=sizeof(projection);
            error=RasGetProjectionInfo(connection,RASP_PppIp,&projection,&bytes);emit(L"PPP_IP_RESULT",error,projection.dwError);
            /* Connection is kept only if IPv4 projection succeeds. No internet claim. */
            if(error || projection.dwError)connected=FALSE;
        }
        if(connection){emit(L"HANGUP_OWN_ATTEMPT",RasHangUp(connection),attempt);connection=NULL;Sleep(500);}
        if(connected)break;Sleep(1500);
    }
    if(created)emit(L"DELETE_OWN_TEST_ENTRY",RasDeleteEntry(NULL,entryName),0);
    DestroyWindow(window);
    emit(L"RAS_ATTEMPT_COMPLETE",connected,GetTickCount()-allBegin);CloseHandle(file);ExitProcess(connected?0:1);
}
