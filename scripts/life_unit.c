/* Isolated production flight recorder tests. No USB/AT/device registry. */
#include <windows.h>
#include "me3630_life.h"
#undef wsprintfW
static HANDLE logFile;static DWORD checks,failures;
static void check(LPCWSTR name,BOOL ok){WCHAR s[160];DWORD n,bytes;checks++;if(!ok)failures++;n=wsprintfW(s,L"CHECK %s pass=%lu\r\n",name,ok);WriteFile(logFile,s,n*2,&bytes,NULL);}
typedef struct {M37_MAP *m;DWORD id;} JOB;
static DWORD WINAPI write_many(LPVOID arg){JOB *j=(JOB*)arg;DWORD i;for(i=0;i<200;i++)m37_put(j->m,M37_ABORT_BEGIN,j->id,j->id^0x5555,j->id^0xaaaa);return 0;}
void mainCRTStartup(void)
{
 HANDLE a=NULL,b=NULL,threads[4];M37_MAP *first,*second;M37_RECORD record;JOB jobs[4];WCHAR name[96],s[180],exe[MAX_PATH],args[160];DWORD i,bytes,n,valid=0;LONG end,begin,seq;BOOL joined=TRUE,consistent=TRUE,created;PROCESS_INFORMATION child;
 {LPCWSTR option=wcsstr(GetCommandLineW(),L"--child ");if(option){M37_MAP *map=m37_open(option+8,&a);if(!map)ExitProcess(3);m37_put(map,M37_READY,0xabc,0xdef,0);m37_close(map,a);ExitProcess(0);}}
 logFile=CreateFile(L"\\Temp\\ME3630-life-unit.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(logFile==INVALID_HANDLE_VALUE)ExitProcess(2);
 n=wsprintfW(s,L"LIFE_UNIT start_tick=%lu\r\n",GetTickCount());WriteFile(logFile,s,n*2,&bytes,NULL);
 wsprintfW(name,L"ME3630LifeUnit%lu",GetCurrentProcessId());first=m37_open(name,&a);check(L"map_created",first!=NULL);if(!first)goto done;
 second=m37_open(name,&b);check(L"shared_map_open",second!=NULL);if(!second){m37_close(first,a);goto done;}
 check(L"schema_ready",first->magic==M37_MAGIC && first->size==sizeof(M37_MAP) && InterlockedCompareExchange(&first->ready,0,0)==2);
 SetLastError(1234);m37_put(first,M37_ATTACH_ENTER,55,77,99);check(L"record_preserves_error",GetLastError()==1234);
 check(L"published_record",m37_get(first,1,&record) && record.event==M37_ATTACH_ENTER && record.object==55 && record.value==77 && record.error==99);
 InterlockedExchange(&first->slots[1].busy,1);m37_put(first,M37_DETACH_ENTER,1,2,3);check(L"busy_writer_drops",InterlockedCompareExchange(&first->dropped,0,0)==1 && !m37_get(first,2,&record));InterlockedExchange(&first->slots[1].busy,0);
 check(L"unpublished_not_read",!m37_get(first,3,&record));
 for(i=0;i<300;i++)m37_put(first,M37_READY,i,i,0);end=InterlockedCompareExchange(&first->next,0,0);
 check(L"ring_wrap_latest",m37_get(first,end,&record) && record.object==299);check(L"overwritten_rejected",!m37_get(first,1,&record));
 check(L"other_view_sees_record",m37_get(second,end,&record) && record.value==299);
 for(i=0;i<4;i++){jobs[i].m=first;jobs[i].id=i+1;threads[i]=CreateThread(NULL,0,write_many,&jobs[i],0,NULL);}
 for(i=0;i<4;i++){if(!threads[i] || WaitForSingleObject(threads[i],3000)!=WAIT_OBJECT_0)joined=FALSE;if(threads[i])CloseHandle(threads[i]);}
 check(L"concurrent_writers_join",joined);
 end=InterlockedCompareExchange(&first->next,0,0);begin=end>M37_CAP?end-M37_CAP+1:1;
 for(seq=begin;seq<=end;seq++)if(m37_get(first,seq,&record)){valid++;if(record.event!=M37_ABORT_BEGIN || record.object<1 || record.object>4 || record.value!=(record.object^0x5555) || record.error!=(record.object^0xaaaa))consistent=FALSE;}
 check(L"concurrent_snapshot_consistent",valid>0 && consistent);
 memset(&child,0,sizeof(child));GetModuleFileName(NULL,exe,MAX_PATH);wsprintfW(args,L"--child %s",name);created=CreateProcess(exe,args,NULL,NULL,FALSE,0,NULL,NULL,NULL,&child);
 n=created?WaitForSingleObject(child.hProcess,3000):WAIT_FAILED;end=InterlockedCompareExchange(&first->next,0,0);
 check(L"cross_process_named_mapping",created && n==WAIT_OBJECT_0 && m37_get(first,end,&record) && record.object==0xabc && record.value==0xdef && record.pid!=GetCurrentProcessId());if(created){CloseHandle(child.hThread);CloseHandle(child.hProcess);}
 if(!joined || (created && n!=WAIT_OBJECT_0))goto done; /* Keep views alive until process exit if a test worker did not retire. */
 m37_close(first,a);first=NULL;a=NULL;check(L"observer_keeps_map_alive",m37_get(second,end,&record) || InterlockedCompareExchange(&second->next,0,0)==end);
 check(L"event_name_table",!lstrcmpW(m37_event_name(M37_DEACTIVATE_BEGIN),L"DEACTIVATE_BEGIN") && !lstrcmpW(m37_event_name(999),L"UNKNOWN"));
#ifdef COM_DIAG38
 check(L"d38_event_name_table",!lstrcmpW(m37_event_name(M38_READ_END),L"READ_END") && !lstrcmpW(m37_event_name(M38_TRANSFER_SUBMITTED),L"TRANSFER_SUBMITTED"));
 SetLastError(4321);m38_status_record(second,55,TRUE,6,0,0);end=InterlockedCompareExchange(&second->next,0,0);
 check(L"usb_status_distinct_from_api",m37_get(second,end,&record) && record.event==M38_USB_RESULT && record.value==6 && record.error==0 && GetLastError()==4321);
 m38_status_record(second,55,TRUE,0,0x1234,0);end=InterlockedCompareExchange(&second->next,0,0);
 check(L"usb_error_preserved",m37_get(second,end,&record) && record.event==M38_USB_RESULT && record.error==0x1234);
 m38_status_record(second,55,FALSE,99,0,6);end=InterlockedCompareExchange(&second->next,0,0);
 check(L"failed_status_not_claimed",m37_get(second,end,&record) && record.event==M38_USB_RESULT_UNAVAILABLE && record.value==0 && record.error==6);
#endif
 if(joined && (!created || n==WAIT_OBJECT_0))m37_close(second,b);
done:
 n=wsprintfW(s,L"LIFE_UNIT CHECKS=%lu FAILURES=%lu COMPLETE\r\n",checks,failures);WriteFile(logFile,s,n*2,&bytes,NULL);CloseHandle(logFile);ExitProcess(failures?1:0);
}
