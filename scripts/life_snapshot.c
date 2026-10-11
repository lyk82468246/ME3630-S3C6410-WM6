/* Independent one-shot reader; never truncates the active observer's file. */
#include <windows.h>
#include <tlhelp32.h>
#include "me3630_life.h"
#undef wsprintfW
void mainCRTStartup(void)
{
 HANDLE handle=NULL,file;M37_MAP *map;M37_RECORD record;LONG seq,end,lower;DWORD bytes,n;WCHAR text[400];
 file=CreateFile(L"\\Temp\\ME3630-life-snapshot-d39.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 if(file==INVALID_HANDLE_VALUE)ExitProcess(2);
 n=wsprintfW(text,L"SNAPSHOT_START tick=%lu\r\n",GetTickCount());WriteFile(file,text,n*2,&bytes,NULL);
 {HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS|TH32CS_SNAPNOHEAPS,0);PROCESSENTRY32 entry;DWORD own=0,total=0;
  if(snapshot!=INVALID_HANDLE_VALUE){
   memset(&entry,0,sizeof(entry));entry.dwSize=sizeof(entry);
   if(Process32First(snapshot,&entry))do{
    total++;if(wcsstr(entry.szExeFile,L"ME3630")){
     own++;n=wsprintfW(text,L"OWN_PROCESS pid=%lu threads=%lu name=%.100s\r\n",entry.th32ProcessID,entry.cntThreads,entry.szExeFile);WriteFile(file,text,n*2,&bytes,NULL);
    }
   }while(Process32Next(snapshot,&entry));
   CloseToolhelp32Snapshot(snapshot);
  }
  n=wsprintfW(text,L"PROCESS_SCAN total=%lu own=%lu\r\n",total,own);WriteFile(file,text,n*2,&bytes,NULL);
 }
 map=m37_open(M37_NAME,&handle);
 if(map){
  end=InterlockedCompareExchange(&map->next,0,0);lower=end>M37_CAP?end-M37_CAP+1:1;
  n=wsprintfW(text,L"SNAPSHOT_MAP next=%ld dropped=%ld\r\n",end,InterlockedCompareExchange(&map->dropped,0,0));WriteFile(file,text,n*2,&bytes,NULL);
  for(seq=lower;seq<=end;seq++){
   if(m37_get(map,seq,&record))n=wsprintfW(text,L"seq=%ld tick=%lu pid=%lu tid=%lu event=%s object=%08lx value=%08lx error=%lu\r\n",seq,record.tick,record.pid,record.tid,m37_event_name(record.event),record.object,record.value,record.error);
   else n=wsprintfW(text,L"SKIP seq=%ld\r\n",seq);
   WriteFile(file,text,n*2,&bytes,NULL);
  }
  m37_close(map,handle);
 }else{n=wsprintfW(text,L"MAP_ERROR error=%lu\r\n",GetLastError());WriteFile(file,text,n*2,&bytes,NULL);}
 n=wsprintfW(text,L"SNAPSHOT_COMPLETE tick=%lu\r\n",GetTickCount());WriteFile(file,text,n*2,&bytes,NULL);CloseHandle(file);ExitProcess(0);
}
