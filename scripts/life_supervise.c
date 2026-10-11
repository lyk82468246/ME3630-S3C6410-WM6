/* Hold the recorder map and real child process handles across one test.
 * No process termination, resets, USB calls or saved-connection changes. */
#include <windows.h>
#include "me3630_life.h"
#undef wsprintfW
static HANDLE logFile;
static void child_result(LPCWSTR name,PROCESS_INFORMATION *child,BOOL created)
{
 WCHAR text[240];DWORD n,bytes,wait,code=0xdeadbeef,error;BOOL queried;
 if(!created)return;
 wait=WaitForSingleObject(child->hProcess,0);SetLastError(0);
 queried=GetExitCodeProcess(child->hProcess,&code);error=GetLastError();
 n=wsprintfW(text,L"CHILD name=%s pid=%lu wait=%lu exit_query=%lu exit_code=%08lx api_error=%lu\r\n",name,child->dwProcessId,wait,queried!=0,code,error);WriteFile(logFile,text,n*2,&bytes,NULL);
}
void mainCRTStartup(void)
{
 PROCESS_INFORMATION capture,dial;BOOL c,d;DWORD start=GetTickCount(),bytes,n;WCHAR text[240];HANDLE handle=NULL;M37_MAP *map;
 logFile=CreateFile(L"\\Temp\\ME3630-supervise-d39.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(logFile==INVALID_HANDLE_VALUE)ExitProcess(2);
 n=wsprintfW(text,L"SUPERVISE_START tick=%lu\r\n",start);WriteFile(logFile,text,n*2,&bytes,NULL);
 map=m37_open(M37_NAME,&handle);if(!map){CloseHandle(logFile);ExitProcess(3);}
 memset(&capture,0,sizeof(capture));memset(&dial,0,sizeof(dial));
 c=CreateProcess(L"\\Temp\\ME3630-supervised-capture-d39.exe",L"--watch",NULL,NULL,FALSE,0,NULL,NULL,NULL,&capture);
 n=wsprintfW(text,L"CAPTURE_CREATE ok=%lu pid=%lu error=%lu\r\n",c!=0,capture.dwProcessId,c?0:GetLastError());WriteFile(logFile,text,n*2,&bytes,NULL);
 d=CreateProcess(L"\\Temp\\ME3630-supervised-ras-d39.exe",L"--dial --require-d39",NULL,NULL,FALSE,0,NULL,NULL,NULL,&dial);
 n=wsprintfW(text,L"DIAL_CREATE ok=%lu pid=%lu error=%lu\r\n",d!=0,dial.dwProcessId,d?0:GetLastError());WriteFile(logFile,text,n*2,&bytes,NULL);FlushFileBuffers(logFile);
 do{
  if((!c || WaitForSingleObject(capture.hProcess,0)==WAIT_OBJECT_0) && (!d || WaitForSingleObject(dial.hProcess,0)==WAIT_OBJECT_0))break;
  Sleep(100);
 }while(GetTickCount()-start<110000);
 child_result(L"capture",&capture,c);child_result(L"dial",&dial,d);
 n=wsprintfW(text,L"SUPERVISE_MAP next=%ld dropped=%ld\r\n",InterlockedCompareExchange(&map->next,0,0),InterlockedCompareExchange(&map->dropped,0,0));WriteFile(logFile,text,n*2,&bytes,NULL);
 if(c){CloseHandle(capture.hThread);CloseHandle(capture.hProcess);}if(d){CloseHandle(dial.hThread);CloseHandle(dial.hProcess);}
 m37_close(map,handle);n=wsprintfW(text,L"SUPERVISE_COMPLETE tick=%lu elapsed=%lu\r\n",GetTickCount(),GetTickCount()-start);WriteFile(logFile,text,n*2,&bytes,NULL);CloseHandle(logFile);ExitProcess(0);
}
