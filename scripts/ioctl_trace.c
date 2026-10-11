/* Measure this ROM's serial API -> stream IOCTL ABI, without touching USB/COM. */
#include <windows.h>
#undef wsprintfW
static void emit(LPCWSTR text)
{
 HANDLE f;DWORD n;f=CreateFile(L"\\Temp\\ME3630-ioctl-abi.txt",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,0,NULL);
 if(f!=INVALID_HANDLE_VALUE){SetFilePointer(f,0,NULL,FILE_END);WriteFile(f,text,lstrlenW(text)*2,&n,NULL);CloseHandle(f);}
}
#ifdef TRACE_DLL
DWORD TRC_Init(DWORD ctx){return 1;}
BOOL TRC_Deinit(DWORD ctx){return TRUE;}
DWORD TRC_Open(DWORD ctx,DWORD access,DWORD share){return 1;}
BOOL TRC_Close(DWORD ctx){return TRUE;}
DWORD TRC_Read(DWORD ctx,LPVOID buf,DWORD len){return 0;}
DWORD TRC_Write(DWORD ctx,LPCVOID buf,DWORD len){return 0;}
DWORD TRC_Seek(DWORD ctx,LONG dist,DWORD method){return (DWORD)-1;}
BOOL TRC_IOControl(DWORD ctx,DWORD code,PBYTE in,DWORD inSize,PBYTE out,DWORD outSize,PDWORD actual)
{
 WCHAR line[256];DWORD i,n;
 n=wsprintfW(line,L" IOCTL=%08lx in=%lu out=%lu DATA=",code,inSize,outSize);
 for(i=0;in && i<inSize && i<48;i++)n+=wsprintfW(line+n,L"%02x",in[i]);
 wsprintfW(line+n,L"\r\n");emit(line);
 if(actual)*actual=0;SetLastError(ERROR_NOT_SUPPORTED);return FALSE;
}
void TRC_PowerUp(DWORD ctx){}
void TRC_PowerDown(DWORD ctx){}
BOOL WINAPI DllMain(HANDLE mod,DWORD reason,LPVOID reserved){return TRUE;}
#else
#define CALL(label,expr) do{emit(L"API " label L"\r\n");SetLastError(0);ok=(expr);wsprintfW(line,L" RESULT=%lu error=%lu\r\n",ok,GetLastError());emit(line);}while(0)
void mainCRTStartup(void)
{
 HANDLE dev,port,f;DWORD n,status;BOOL ok;WCHAR line[128];DCB dcb;COMMTIMEOUTS timeouts;COMSTAT stat;COMMPROP prop;
 f=CreateFile(L"\\Temp\\ME3630-ioctl-abi.txt",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,CREATE_ALWAYS,0,NULL);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
 dev=RegisterDevice(L"TRC",1,L"ME3630IoctlTrace.dll",0);
 wsprintfW(line,L"REGISTER=%08lx error=%lu\r\n",(DWORD)dev,GetLastError());emit(line);
 if(!dev || dev==INVALID_HANDLE_VALUE)ExitProcess(1);
 port=CreateFile(L"TRC1:",GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
 if(port==INVALID_HANDLE_VALUE){emit(L"OPEN_FAILED\r\n");DeregisterDevice(dev);ExitProcess(1);}
 memset(&dcb,0,sizeof(dcb));dcb.DCBlength=sizeof(dcb);dcb.BaudRate=115200;dcb.ByteSize=8;dcb.fBinary=TRUE;dcb.fDtrControl=DTR_CONTROL_ENABLE;dcb.fRtsControl=RTS_CONTROL_ENABLE;
 memset(&timeouts,0,sizeof(timeouts));timeouts.ReadIntervalTimeout=MAXDWORD;timeouts.ReadTotalTimeoutConstant=700;timeouts.WriteTotalTimeoutConstant=2000;
 CALL(L"GetCommState",GetCommState(port,&dcb));
 CALL(L"SetCommState",SetCommState(port,&dcb));
 CALL(L"GetCommTimeouts",GetCommTimeouts(port,&timeouts));
 CALL(L"SetCommTimeouts",SetCommTimeouts(port,&timeouts));
 CALL(L"SETDTR",EscapeCommFunction(port,SETDTR));
 CALL(L"CLRDTR",EscapeCommFunction(port,CLRDTR));
 CALL(L"SETRTS",EscapeCommFunction(port,SETRTS));
 CALL(L"CLRRTS",EscapeCommFunction(port,CLRRTS));
 CALL(L"GetCommModemStatus",GetCommModemStatus(port,&status));
 CALL(L"ClearCommError",ClearCommError(port,&status,&stat));
 CALL(L"GetCommProperties",GetCommProperties(port,&prop));
 CALL(L"SetCommMask",SetCommMask(port,EV_RXCHAR));
 CALL(L"GetCommMask",GetCommMask(port,&status));
 CALL(L"WaitCommEvent",WaitCommEvent(port,&status,NULL));
 CALL(L"PurgeComm",PurgeComm(port,PURGE_RXCLEAR|PURGE_TXCLEAR));
 CALL(L"SetupComm",SetupComm(port,4096,4096));
 CALL(L"SetCommBreak",SetCommBreak(port));
 CALL(L"ClearCommBreak",ClearCommBreak(port));
 CloseHandle(port);DeregisterDevice(dev);emit(L"COMPLETE\r\n");ExitProcess(0);
}
#endif
