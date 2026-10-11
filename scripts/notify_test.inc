#include "me3630_notify_state.h"
static void notify_test(HANDLE log,HANDLE port)
{
    M36_NOTIFY_STATE state;DWORD actual=0,start=GetTickCount(),dummy=0,n;BOOL ok;WCHAR line[200];
    do {
        memset(&state,0,sizeof(state));
        ok=DeviceIoControl(port,M36_NOTIFY_QUERY,NULL,0,&state,sizeof(state),&actual,NULL);
        if(!ok || state.known || GetTickCount()-start>=1000)break;Sleep(20);
    }while(TRUE);
    check(log,L"Notification snapshot ABI and received state",ok && actual==32 && state.size==32 && state.accepted && state.known==M36_NOTIFY_KNOWN && !state.error);
    check(log,L"CTS explicitly unknown",ok && !(state.known&MS_CTS_ON) && !(state.status&MS_CTS_ON));
    check(log,L"Status contains only known lines",ok && !(state.status&~state.known) && state.raw<=0x7f);
    n=wsprintfW(line,L"NOTIFY_STATE known=%02lx status=%02lx raw=%02lx accepted=%lu rejected=%lu tick=%lu error=%lu\r\n",state.known,state.status,state.raw,state.accepted,state.rejected,state.tick,state.error);WriteFile(log,line,n*2,&n,NULL);
    ok=DeviceIoControl(port,M36_NOTIFY_QUERY,NULL,0,&state,sizeof(state)-1,&actual,NULL);
    check(log,L"Short notification output rejected",!ok && GetLastError()==ERROR_INVALID_PARAMETER && actual==0);
    ok=DeviceIoControl(port,M36_NOTIFY_QUERY,&dummy,sizeof(dummy),&state,sizeof(state),&actual,NULL);
    check(log,L"Notification input rejected",!ok && GetLastError()==ERROR_INVALID_PARAMETER && actual==0);
    ok=GetCommModemStatus(port,&dummy);
    check(log,L"Full modem query remains unsupported",!ok && GetLastError()==ERROR_NOT_SUPPORTED);
}
