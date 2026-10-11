#include <windows.h>
#undef wsprintfW
typedef struct {LONG opened,refs;} PORT;
static void port_release(PORT *port){InterlockedDecrement(&port->refs);}
#include "me3630_open.inc"
static DWORD checks,failures;static HANDLE logFile;
static void check(LPCWSTR name,BOOL ok){WCHAR s[140];DWORD bytes,n;checks++;if(!ok)failures++;n=wsprintfW(s,L"CHECK %s pass=%lu\r\n",name,ok);WriteFile(logFile,s,n*2,&bytes,NULL);}
void mainCRTStartup(void)
{
 PORT p={0,1};M36_OPEN *data,*zero,*zero2,*second;DWORD bytes,n;WCHAR s[180];
 logFile=CreateFile(L"\\Temp\\ME3630-open-unit.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(logFile==INVALID_HANDLE_VALUE)ExitProcess(2);
 n=wsprintfW(s,L"OPEN_UNIT start_tick=%lu\r\n",GetTickCount());WriteFile(logFile,s,n*2,&bytes,NULL);
 data=m36_open_new(&p,GENERIC_READ|GENERIC_WRITE);check(L"data_claim",data && p.opened==1 && p.refs==2);if(!data)goto end;
 zero=m36_open_new(&p,0);check(L"zero_alongside_data",zero && !zero->ownsData && p.opened==1 && p.refs==3);if(!zero){m36_open_release(data);goto end;}
 zero2=m36_open_new(&p,0);check(L"second_zero",zero2 && p.opened==1 && p.refs==4);if(!zero2){m36_open_release(zero);m36_open_release(data);goto end;}
 second=m36_open_new(&p,GENERIC_READ);check(L"second_data_rejected",!second && GetLastError()==ERROR_SHARING_VIOLATION && p.refs==4);if(second)m36_open_release(second);
 check(L"zero_query_allowed",m36_open_allows_control(zero,0x00223ffc));
 check(L"zero_dcb_denied",!m36_open_allows_control(zero,0x001b0054));
 check(L"zero_purge_denied",!m36_open_allows_control(zero,0x001b0044));
 check(L"zero_wait_denied",!m36_open_allows_control(zero,0x001b002c));
 check(L"zero_no_read_write",!(zero->access&(GENERIC_READ|GENERIC_WRITE)));
 check(L"data_controls_unchanged",m36_open_allows_control(data,0x001b0054));
 m36_open_release(zero2);check(L"metadata_close_preserves_owner",p.opened==1 && p.refs==3);
 m36_open_release(data);check(L"data_close_with_metadata",p.opened==0 && p.refs==2);
 data=m36_open_new(&p,GENERIC_WRITE);check(L"data_reopen_with_metadata",data && p.opened==1 && p.refs==3);
 m36_open_release(zero);check(L"old_metadata_not_clear_new_owner",p.opened==1 && p.refs==2);
 if(data)m36_open_release(data);check(L"balanced_refs",p.opened==0 && p.refs==1);
end:
 n=wsprintfW(s,L"OPEN_UNIT CHECKS=%lu FAILURES=%lu COMPLETE\r\n",checks,failures);WriteFile(logFile,s,n*2,&bytes,NULL);CloseHandle(logFile);ExitProcess(failures?1:0);
}
