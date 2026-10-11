/* 18 checks per port: native status ABI, RX clear, rejection atomicity,
 * pending synchronous read cancellation and subsequent real AT recovery. */
static BOOL queue_status(HANDLE port,COMSTAT *stat,DWORD *errors)
{
 memset(stat,0,sizeof(*stat));*errors=MAXDWORD;
 return ClearCommError(port,errors,stat);
}
static void status_test(HANDLE log,HANDLE port)
{
 COMSTAT stat;COMMTIMEOUTS old,t;DWORD errors,bytes,begin,elapsed,code;BOOL ok;
 BYTE response[8];DUPLEX job;HANDLE thread;WCHAR line[160];DWORD n;
 check(log,L"COMSTAT ABI is 12 bytes",sizeof(COMSTAT)==12);
 check(log,L"Initial RXCLEAR",PurgeComm(port,PURGE_RXCLEAR));
 ok=queue_status(port,&stat,&errors);check(log,L"Empty status has zero queues and errors",ok && errors==0 && stat.cbInQue==0 && stat.cbOutQue==0);
 ok=WriteFile(port,"AT\r",3,&bytes,NULL);check(log,L"Status AT write",ok && bytes==3);
 begin=GetTickCount();do {ok=queue_status(port,&stat,&errors);if(!ok || stat.cbInQue>=6)break;Sleep(20);}while(GetTickCount()-begin<2000);
 check(log,L"Status counts full six-byte reply",ok && errors==0 && stat.cbInQue==6 && stat.cbOutQue==0);
 memset(response,0,sizeof(response));ok=ReadFile(port,response,1,&bytes,NULL);
 check(log,L"Status partial read gets first byte",ok && bytes==1 && response[0]=='\r');
 ok=queue_status(port,&stat,&errors);check(log,L"Status retains five unread bytes",ok && errors==0 && stat.cbInQue==5);
 ok=PurgeComm(port,PURGE_RXCLEAR|PURGE_TXCLEAR);code=GetLastError();
 check(log,L"Mixed unsupported purge preserves queue",!ok && code==ERROR_NOT_SUPPORTED && queue_status(port,&stat,&errors) && stat.cbInQue==5);
 ok=PurgeComm(port,0);code=GetLastError();
 check(log,L"Zero purge rejected without discarding",!ok && code==ERROR_INVALID_PARAMETER && queue_status(port,&stat,&errors) && stat.cbInQue==5);
 check(log,L"RXCLEAR discards unread bytes",PurgeComm(port,PURGE_RXCLEAR));
 ok=queue_status(port,&stat,&errors);check(log,L"Status empty after RXCLEAR",ok && errors==0 && stat.cbInQue==0 && stat.cbOutQue==0);
 memset(&old,0,sizeof(old));ok=GetCommTimeouts(port,&old);t=old;t.ReadTotalTimeoutConstant=200;
 if(ok)ok=SetCommTimeouts(port,&t);begin=GetTickCount();bytes=MAXDWORD;
 if(ok)ok=ReadFile(port,response,1,&bytes,NULL);elapsed=GetTickCount()-begin;
 check(log,L"Cleared response cannot be read",ok && bytes==0 && elapsed>=150 && elapsed<1500);
 check(log,L"Status test restores read timeout",SetCommTimeouts(port,&old));
 memset(&job,0,sizeof(job));job.port=port;job.ready=CreateEvent(NULL,TRUE,FALSE,NULL);
 thread=job.ready?CreateThread(NULL,0,duplex_reader,&job,0,NULL):NULL;
 check(log,L"Purge pending reader starts",thread!=NULL);
 if(!thread){if(job.ready)CloseHandle(job.ready);return;}
 WaitForSingleObject(job.ready,2000);Sleep(100);begin=GetTickCount();ok=PurgeComm(port,PURGE_RXABORT);elapsed=GetTickCount()-begin;
 check(log,L"RXABORT returns promptly",ok && elapsed<500);
 if(WaitForSingleObject(thread,3000)!=WAIT_OBJECT_0){check(log,L"Purge reader stuck",FALSE);FlushFileBuffers(log);ExitProcess(2);}
 check(log,L"RXABORT releases pending read",!job.ok && job.count==0 && job.error==ERROR_OPERATION_ABORTED && job.elapsed<1500);
 n=wsprintfW(line,L"PURGE_ABORT call_ms=%lu read_ms=%lu bytes=%lu error=%lu\r\n",elapsed,job.elapsed,job.count,job.error);WriteFile(log,line,n*2,&bytes,NULL);
 CloseHandle(thread);CloseHandle(job.ready);
 ok=WriteFile(port,"AT\r",3,&bytes,NULL);check(log,L"Post-abort AT write",ok && bytes==3);
 memset(response,0,sizeof(response));ok=ReadFile(port,response,6,&bytes,NULL);
 check(log,L"Post-abort full AT response",ok && bytes==6 && memcmp(response,"\r\nOK\r\n",6)==0);
}
