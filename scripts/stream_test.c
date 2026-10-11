#include <windows.h>
#undef wsprintfW
void mainCRTStartup(void)
{
    HANDLE log,port;DWORD i,j,bytes,n,error,total,attempt,passed=0;BOOL ok;WCHAR name[16],line[768];BYTE rx[2048];
    static const char *commands[]={"AT\r","ATI\r"};
    log=CreateFile(L"\\Temp\\ME3630-stream-test.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    n=wsprintfW(line,L"KEEP_AWAKE_DIAGNOSTIC start_tick=%lu wait=25s\r\n",GetTickCount());
    WriteFile(log,line,n*sizeof(WCHAR),&bytes,NULL);
    /* Temporary diagnostic only: no persistent power policy changes. */
    for(i=0;i<25;i++){SystemIdleTimerReset();Sleep(1000);}
    for(i=1;i<=2;i++) {
        wsprintfW(name,L"M36%lu:",i);
        SetLastError(0);port=CreateFile(name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);error=GetLastError();
        n=wsprintfW(line,L"OPEN %s success=%lu error=%lu\r\n",name,port!=INVALID_HANDLE_VALUE,error);
        WriteFile(log,line,n*sizeof(WCHAR),&bytes,NULL);
        if(port==INVALID_HANDLE_VALUE)continue;
        for(j=0;j<2;j++) {
            SystemIdleTimerReset();
            SetLastError(0);ok=WriteFile(port,commands[j],j?4:3,&bytes,NULL);error=GetLastError();
            n=wsprintfW(line,L"WRITE cmd=%lu success=%lu bytes=%lu error=%lu\r\n",j,ok,bytes,error);
            WriteFile(log,line,n*sizeof(WCHAR),&bytes,NULL);
            if(!ok)break;
            memset(rx,0,sizeof(rx));total=0;
            for(attempt=0;attempt<4;attempt++) {
                DWORD k;BOOL hasOK=FALSE;
                SetLastError(0);ok=ReadFile(port,rx+total,512,&bytes,NULL);error=GetLastError();
                n=wsprintfW(line,L"READ success=%lu bytes=%lu error=%lu\r\n",ok,bytes,error);
                WriteFile(log,line,n*sizeof(WCHAR),&n,NULL);
                if(!ok || bytes>512)break;
                for(k=0;k<bytes;k++)line[k]=(WCHAR)rx[total+k];
                WriteFile(log,line,bytes*sizeof(WCHAR),&n,NULL);
                total+=bytes;
                for(k=0;k+5<total;k++)if(rx[k]=='\r' && rx[k+1]=='\n' && rx[k+2]=='O' && rx[k+3]=='K' && rx[k+4]=='\r' && rx[k+5]=='\n')hasOK=TRUE;
                if(hasOK){passed++;break;}
            }
        }
        CloseHandle(port);
    }
    n=wsprintfW(line,L"\r\nCOMMANDS_PASSED=%lu/4 COMPLETE\r\n",passed);
    WriteFile(log,line,n*sizeof(WCHAR),&bytes,NULL);CloseHandle(log);ExitProcess(passed==4?0:1);
}
