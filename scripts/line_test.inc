static void line_test(HANDLE log,HANDLE port)
{
    DWORD mask=0,start,elapsed,n;BOOL ok;EVENTJOB j;HANDLE thread;WCHAR text[128];
    check(log,L"Set real line event mask",SetCommMask(port,M36_LINE_EVENTS));
    ok=GetCommMask(port,&mask);check(log,L"Line event mask roundtrip",ok && mask==M36_LINE_EVENTS);
    ok=SetCommMask(port,M36_LINE_EVENTS|EV_CTS);
    {DWORD error=GetLastError();BOOL got=GetCommMask(port,&mask);check(log,L"CTS mask rejected atomically",!ok && error==ERROR_NOT_SUPPORTED && got && mask==M36_LINE_EVENTS);}
    thread=event_start(&j,port);check(log,L"Line waiter starts",thread!=NULL);
    if(!thread)return;
    start=GetTickCount();ok=SetCommMask(port,0);elapsed=GetTickCount()-start;
    check(log,L"Line mask cancellation returns promptly",ok && elapsed<500);
    event_join(log,thread,&j);
    /* A genuine line edge may win cancellation; either result is legitimate.
       Positive event delivery is verified separately with fake notifications. */
    check(log,L"Line waiter completes or reports genuine event",j.ok && !(j.mask&~M36_LINE_EVENTS) && j.elapsed<2000);
    n=wsprintfW(text,L"LINE_CANCEL mask=%08lx elapsed=%lu cancel_ms=%lu error=%lu\r\n",j.mask,j.elapsed,elapsed,j.error);WriteFile(log,text,n*2,&n,NULL);
}
