#include <windows.h>
#ifdef COM_V4
#include <pm.h>
#endif
#undef wsprintfW
#ifdef COM_V4
static DWORD checks=0,failures=0;
static void check(HANDLE log,LPCWSTR label,BOOL success)
{
 WCHAR line[160];DWORD n,bytes,error=GetLastError();
 checks++;if(!success)failures++;
 n=wsprintfW(line,L"CHECK %s pass=%lu error=%lu\r\n",label,success,error);WriteFile(log,line,n*2,&bytes,NULL);
}
static void control_test(HANDLE log,HANDLE port,LPCWSTR name)
{
 DCB dcb,got;COMMTIMEOUTS timeouts,gotTimeouts,bad;HANDLE other;BOOL ok;DWORD bytes,start,elapsed;BYTE one;
 memset(&dcb,0,sizeof(dcb));dcb.DCBlength=sizeof(dcb);
 check(log,L"GetCommState",GetCommState(port,&dcb));
 dcb.BaudRate=115200;dcb.ByteSize=8;dcb.Parity=NOPARITY;dcb.StopBits=ONESTOPBIT;dcb.fBinary=TRUE;
 dcb.fDtrControl=DTR_CONTROL_ENABLE;dcb.fRtsControl=RTS_CONTROL_ENABLE;
 check(log,L"SetCommState 115200 8N1",SetCommState(port,&dcb));
 memset(&got,0,sizeof(got));got.DCBlength=sizeof(got);
 ok=GetCommState(port,&got);check(log,L"DCB roundtrip",ok && memcmp(&dcb,&got,sizeof(dcb))==0);
 dcb.BaudRate=9600;ok=SetCommState(port,&dcb);check(log,L"Unsupported baud rejected",!ok && GetLastError()==ERROR_NOT_SUPPORTED);
 check(log,L"CLRDTR",EscapeCommFunction(port,CLRDTR));
 check(log,L"CLRRTS",EscapeCommFunction(port,CLRRTS));
 got.DCBlength=sizeof(got);ok=GetCommState(port,&got);check(log,L"Control lines low",ok && got.fDtrControl==DTR_CONTROL_DISABLE && got.fRtsControl==RTS_CONTROL_DISABLE);
 check(log,L"SETDTR",EscapeCommFunction(port,SETDTR));
 check(log,L"SETRTS",EscapeCommFunction(port,SETRTS));
 got.DCBlength=sizeof(got);ok=GetCommState(port,&got);check(log,L"Control lines high",ok && got.fDtrControl==DTR_CONTROL_ENABLE && got.fRtsControl==RTS_CONTROL_ENABLE);
 memset(&timeouts,0,sizeof(timeouts));timeouts.ReadTotalTimeoutConstant=350;timeouts.WriteTotalTimeoutConstant=2000;
 check(log,L"Set total-only timeout",SetCommTimeouts(port,&timeouts));
 start=GetTickCount();ok=ReadFile(port,&one,1,&bytes,NULL);elapsed=GetTickCount()-start;
 check(log,L"Empty read succeeds with zero bytes at deadline",ok && bytes==0 && elapsed>=250 && elapsed<1500);
 {WCHAR line[96];DWORD n;n=wsprintfW(line,L"EMPTY_READ elapsed=%lu bytes=%lu\r\n",elapsed,bytes);WriteFile(log,line,n*2,&n,NULL);}
 timeouts.ReadIntervalTimeout=MAXDWORD;timeouts.ReadTotalTimeoutMultiplier=MAXDWORD;timeouts.ReadTotalTimeoutConstant=2000;
 check(log,L"Set first-byte timeout",SetCommTimeouts(port,&timeouts));
 ok=GetCommTimeouts(port,&gotTimeouts);check(log,L"Timeouts roundtrip",ok && memcmp(&timeouts,&gotTimeouts,sizeof(timeouts))==0);
 bad=timeouts;bad.ReadTotalTimeoutConstant=0;ok=SetCommTimeouts(port,&bad);check(log,L"Unbounded timeout rejected",!ok && GetLastError()==ERROR_NOT_SUPPORTED);
 other=CreateFile(name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
 /* This ROM maps XXX_Open returning zero to ERROR_DEV_NOT_EXIST (55), even
    when our stream entry sets ERROR_SHARING_VIOLATION. Original handle is
    subsequently used for AT/ATI, distinguishing exclusivity from removal. */
 check(log,L"Second opener rejected",other==INVALID_HANDLE_VALUE && (GetLastError()==ERROR_SHARING_VIOLATION || GetLastError()==ERROR_DEV_NOT_EXIST));
 if(other!=INVALID_HANDLE_VALUE)CloseHandle(other);
}
#endif
#ifdef COM_V7
#include "duplex_test.inc"
#endif
#ifdef COM_V8
#include "event_test.inc"
#endif
#ifdef COM_V9
#include "queue_test.inc"
#endif
void mainCRTStartup(void)
{
    HANDLE log,port;DWORD i,j,bytes,n,error,total,attempt,passed=0;BOOL ok;WCHAR name[16],line[768];BYTE rx[2048];
#ifdef COM_V4
    DWORD availabilityStart=GetTickCount(),sample=0,flags,powerError;CEDEVICE_POWER_STATE state;WCHAR powerName[64];
#endif
    static const char *commands[]={"AT\r","ATI\r"};
#ifdef COM_V4
#ifdef COM_V5
#ifdef COM_V6
#ifdef COM_V7
#ifdef COM_V8
#ifdef COM_V9
#ifdef COM_V10
    log=CreateFile(L"\\Temp\\ME3630-com-test-v10.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
#else
    log=CreateFile(L"\\Temp\\ME3630-com-test-v9.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
#endif
#else
    log=CreateFile(L"\\Temp\\ME3630-com-test-v8.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
#endif
#else
    log=CreateFile(L"\\Temp\\ME3630-com-test-v7.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
#endif
#else
    log=CreateFile(L"\\Temp\\ME3630-com-test-v6.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
#endif
#else
    log=CreateFile(L"\\Temp\\ME3630-com-test-v5.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
#endif
#else
    log=CreateFile(L"\\Temp\\ME3630-com-test.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
#endif
#else
    log=CreateFile(L"\\Temp\\ME3630-stream-test.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
#endif
#ifdef COM_V4
    n=wsprintfW(line,L"WAIT_FOR_PORTS_DIAGNOSTIC start_tick=%lu deadline=40s\r\n",GetTickCount());
#else
    n=wsprintfW(line,L"KEEP_AWAKE_DIAGNOSTIC start_tick=%lu wait=25s\r\n",GetTickCount());
#endif
    WriteFile(log,line,n*sizeof(WCHAR),&bytes,NULL);
#ifdef COM_V9
    ring_test(log);
#endif
    /* Temporary diagnostic only: no persistent power policy changes. */
#ifndef COM_V4
    for(i=0;i<25;i++){SystemIdleTimerReset();Sleep(1000);}
#endif
    for(i=1;i<=2;i++) {
#ifdef COM_V4
        wsprintfW(name,L"COM%lu:",i==1?1:5);
#else
        wsprintfW(name,L"M36%lu:",i);
#endif
#ifdef COM_V4
        do {
            SystemIdleTimerReset();
            if(GetTickCount()-sample>=5000) {
                DWORD sysError;sample=GetTickCount();flags=0;powerName[0]=0;state=PwrDeviceUnspecified;
                sysError=GetSystemPowerState(powerName,64,&flags);
                powerError=GetDevicePower(L"HCD1:",POWER_NAME,&state);
                n=wsprintfW(line,L"POWER tick=%lu system=%s flags=%08lx sys_error=%lu HCD1_state=%ld error=%lu\r\n",sample,powerName,flags,sysError,state,powerError);
                WriteFile(log,line,n*2,&bytes,NULL);
            }
            SetLastError(0);port=CreateFile(name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);error=GetLastError();
#ifdef COM_V8
            if(port!=INVALID_HANDLE_VALUE) {
                DWORD version=0,actual=0;
                DWORD expectedVersion=8;
#ifdef COM_V9
                expectedVersion=9;
#ifdef COM_V10
                expectedVersion=10;
#endif
#endif
                if(!DeviceIoControl(port,0x00223ffc,NULL,0,&version,sizeof(version),&actual,NULL) || version!=expectedVersion || actual!=4){CloseHandle(port);port=INVALID_HANDLE_VALUE;error=ERROR_REVISION_MISMATCH;}
                else {n=wsprintfW(line,L"INSTANCE_VERSION port=%s version=%lu bytes=%lu\r\n",name,version,actual);WriteFile(log,line,n*2,&bytes,NULL);}
            }
#endif
            if(port!=INVALID_HANDLE_VALUE || GetTickCount()-availabilityStart>=40000)break;
            Sleep(100);
        } while(TRUE);
#else
        SetLastError(0);port=CreateFile(name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);error=GetLastError();
#endif
        n=wsprintfW(line,L"OPEN %s success=%lu error=%lu\r\n",name,port!=INVALID_HANDLE_VALUE,error);
        WriteFile(log,line,n*sizeof(WCHAR),&bytes,NULL);
        if(port==INVALID_HANDLE_VALUE)continue;
#ifdef COM_V4
        control_test(log,port,name);
#endif
        for(j=0;j<2;j++) {
            SystemIdleTimerReset();
            SetLastError(0);ok=WriteFile(port,commands[j],j?4:3,&bytes,NULL);error=GetLastError();
            n=wsprintfW(line,L"WRITE cmd=%lu success=%lu bytes=%lu error=%lu\r\n",j,ok,bytes,error);
            WriteFile(log,line,n*sizeof(WCHAR),&bytes,NULL);
            if(!ok)break;
            memset(rx,0,sizeof(rx));total=0;
            for(attempt=0;attempt<4;attempt++) {
                DWORD k,requested=512;BOOL hasOK=FALSE;
#ifdef COM_V5
                /* Exercise pending-buffer preservation, not just full USB packets. */
                if(attempt==0)requested=1;
#endif
                SetLastError(0);ok=ReadFile(port,rx+total,requested,&bytes,NULL);error=GetLastError();
                n=wsprintfW(line,L"READ requested=%lu success=%lu bytes=%lu error=%lu\r\n",requested,ok,bytes,error);
                WriteFile(log,line,n*sizeof(WCHAR),&n,NULL);
                if(!ok || bytes>requested)break;
                for(k=0;k<bytes;k++)line[k]=(WCHAR)rx[total+k];
                WriteFile(log,line,bytes*sizeof(WCHAR),&n,NULL);
                total+=bytes;
                for(k=0;k+5<total;k++)if(rx[k]=='\r' && rx[k+1]=='\n' && rx[k+2]=='O' && rx[k+3]=='K' && rx[k+4]=='\r' && rx[k+5]=='\n')hasOK=TRUE;
                if(hasOK){passed++;break;}
            }
        }
#ifdef COM_V7
        duplex_test(log,port);
#endif
#ifdef COM_V8
        event_test(log,port);
#endif
#ifdef COM_V9
        queue_test(log,port);
#endif
        CloseHandle(port);
    }
    n=wsprintfW(line,L"\r\nCOMMANDS_PASSED=%lu/4 COMPLETE\r\n",passed);
#ifdef COM_V4
    n=wsprintfW(line,L"\r\nCOMMANDS_PASSED=%lu/4 CONTROL_CHECKS=%lu FAILURES=%lu COMPLETE\r\n",passed,checks,failures);
#endif
    WriteFile(log,line,n*sizeof(WCHAR),&bytes,NULL);CloseHandle(log);
#ifdef COM_V4
#ifdef COM_V7
#ifdef COM_V8
#ifdef COM_V9
    ExitProcess(passed==4 && failures==0 && checks==102?0:1);
#else
    ExitProcess(passed==4 && failures==0 && checks==68?0:1);
#endif
#else
    ExitProcess(passed==4 && failures==0 && checks==44?0:1);
#endif
#else
    ExitProcess(passed==4 && failures==0 && checks==32?0:1);
#endif
#else
    ExitProcess(passed==4?0:1);
#endif
}
