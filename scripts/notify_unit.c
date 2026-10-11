#include <windows.h>
#include "me3630_notify_state.h"
#undef wsprintfW
static DWORD checks,failures;
static void check(HANDLE log,LPCWSTR name,BOOL pass)
{
    WCHAR text[160];DWORD n,bytes;checks++;if(!pass)failures++;
    n=wsprintfW(text,L"CHECK %s pass=%lu\r\n",name,pass);WriteFile(log,text,n*2,&bytes,NULL);
}
void mainCRTStartup(void)
{
    BYTE good[10]={0xa1,0x20,0,0,1,0,2,0,0,0},bad[10];
    DWORD raw,status,n,bytes,i;WCHAR text[160];BOOL ok;
    HANDLE log=CreateFile(L"\\Temp\\ME3630-notify-unit.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    if(log==INVALID_HANDLE_VALUE)ExitProcess(2);
    n=wsprintfW(text,L"NOTIFY_UNIT start_tick=%lu no_usb=1\r\n",GetTickCount());WriteFile(log,text,n*2,&bytes,NULL);
    check(log,L"Private state ABI 32 bytes",sizeof(M36_NOTIFY_STATE)==32);
    ok=m36_parse_notify(good,10,1,&raw,&status);check(log,L"Captured zero state accepted",ok && !raw && !status);
    good[8]=1;ok=m36_parse_notify(good,10,1,&raw,&status);check(log,L"DCD maps RLSD",ok && status==MS_RLSD_ON);
    good[8]=2;ok=m36_parse_notify(good,10,1,&raw,&status);check(log,L"DSR maps DSR",ok && status==MS_DSR_ON);
    good[8]=8;ok=m36_parse_notify(good,10,1,&raw,&status);check(log,L"RI maps RING",ok && status==MS_RING_ON);
    good[8]=0x7f;ok=m36_parse_notify(good,10,1,&raw,&status);check(log,L"Other bits never become CTS",ok && raw==0x7f && status==M36_NOTIFY_KNOWN && !(status&MS_CTS_ON));
    good[4]=2;check(log,L"MI2 accepted",m36_parse_notify(good,10,2,&raw,&status));good[4]=1;
    check(log,L"Wrong interface rejected",!m36_parse_notify(good,10,2,&raw,&status));
    check(log,L"Unsupported interface rejected",!m36_parse_notify(good,10,0,&raw,&status));
    check(log,L"Null input rejected",!m36_parse_notify(NULL,10,1,&raw,&status));
    check(log,L"Null output rejected",!m36_parse_notify(good,10,1,NULL,&status));
    check(log,L"Truncation rejected",!m36_parse_notify(good,9,1,&raw,&status));
    check(log,L"Oversize rejected before read",!m36_parse_notify(good,11,1,&raw,&status));
    /* Mutate each validated header/reserved byte; invalid packets must not
       overwrite the caller's previously known state. */
    for(i=0;i<10;i++) {
        if(i==8)continue;
        memcpy(bad,good,10);bad[i]^=0x80;raw=0x1234;status=0x5678;
        ok=m36_parse_notify(bad,10,1,&raw,&status);
        check(log,L"Invalid byte preserves outputs",!ok && raw==0x1234 && status==0x5678);
    }
    good[8]=0x80;check(log,L"Reserved payload bit rejected",!m36_parse_notify(good,10,1,&raw,&status));
    n=wsprintfW(text,L"NOTIFY_UNIT CHECKS=%lu FAILURES=%lu COMPLETE\r\n",checks,failures);WriteFile(log,text,n*2,&bytes,NULL);CloseHandle(log);
    ExitProcess(failures?1:0);
}
