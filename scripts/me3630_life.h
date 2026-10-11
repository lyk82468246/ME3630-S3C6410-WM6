/* CE5 named, page-backed flight recorder. No waits/file I/O in record(). */
#define M37_CAP 256
#define M37_MAGIC 0x37334d36
#ifdef COM_DIAG38
#define M37_NAME L"ME3630D38Lifecycle"
#else
#define M37_NAME L"ME3630D37Lifecycle"
#endif
enum {M37_ATTACH_ENTER=1,M37_DEVICE_INFO,M37_REGISTER_BEGIN,M37_REGISTER_END,
 M37_READY,M37_DETACH_ENTER,M37_WORKER_JOIN_BEGIN,M37_WORKER_JOIN_END,
 M37_UNREGISTER_BEGIN,M37_UNREGISTER_END,M37_NOTIFY_JOIN_BEGIN,M37_NOTIFY_JOIN_END,
 M37_RX_JOIN_BEGIN,M37_RX_JOIN_END,M37_DEACTIVATE_BEGIN,M37_DEACTIVATE_END,
 M37_LOCK_RX_BEGIN,M37_LOCK_RX_END,M37_LOCK_TX_BEGIN,M37_LOCK_TX_END,
 M37_LOCK_IO_BEGIN,M37_LOCK_IO_END,M37_PIPE_CLOSE_BEGIN,M37_PIPE_CLOSE_END,
 M37_PORT_RELEASE_BEGIN,M37_PORT_RELEASE_END,M37_CLEANUP_END,
 M37_PARENT_RELEASE_BEGIN,M37_PARENT_RELEASE_END,M37_DETACH_RETURN_INTENT,
 M37_ABORT_BEGIN,M37_ABORT_END,M37_STATUS,M37_TRANSFER_CLOSE_BEGIN,M37_TRANSFER_CLOSE_END,
 M37_RX_EXIT,M37_NOTIFY_EXIT,M37_RETAIN,M37_ACTIVATE_BEGIN,M37_ACTIVATE_END,
 M38_USB_RESULT,M38_USB_RESULT_UNAVAILABLE,M38_OPEN_BEGIN,M38_OPEN_END,
 M38_CLOSE_BEGIN,M38_CLOSE_END,M38_READ_BEGIN,M38_READ_END,M38_WRITE_BEGIN,M38_WRITE_END,
 M38_IOCTL_BEGIN,M38_IOCTL_END,M38_RX_PUSH,M38_RX_EVENTS,M38_MASK_STATE,
 M38_WAIT_BEGIN,M38_WAIT_END,M38_WAIT_MASK,M38_TRANSFER_ISSUE,M38_TRANSFER_SUBMITTED};
/* Shared control fields are accessed through Interlocked APIs, not plain reads. */
typedef struct {LONG busy,commit;DWORD tick,pid,tid,event,object,value,error;} M37_RECORD;
typedef struct {LONG ready,next,dropped;DWORD magic,size;M37_RECORD slots[M37_CAP];} M37_MAP;
static M37_MAP *m37_open(LPCWSTR name,HANDLE *handle)
{
 M37_MAP *m;LONG ready;
 *handle=CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(M37_MAP),name);
 if(!*handle)return NULL;
 m=(M37_MAP*)MapViewOfFile(*handle,FILE_MAP_ALL_ACCESS,0,0,sizeof(M37_MAP));
 if(!m){CloseHandle(*handle);*handle=NULL;return NULL;}
 ready=InterlockedCompareExchange(&m->ready,1,0);
 if(!ready){m->magic=M37_MAGIC;m->size=sizeof(M37_MAP);InterlockedExchange(&m->ready,2);}
 else if(ready!=2){UnmapViewOfFile(m);CloseHandle(*handle);*handle=NULL;return NULL;}
 if(m->magic!=M37_MAGIC || m->size!=sizeof(M37_MAP)){UnmapViewOfFile(m);CloseHandle(*handle);*handle=NULL;return NULL;}
 return m;
}
static void m37_close(M37_MAP *m,HANDLE handle){if(m)UnmapViewOfFile(m);if(handle)CloseHandle(handle);}
static void m37_put(M37_MAP *m,DWORD event,DWORD object,DWORD value,DWORD error)
{
 LONG seq,old;DWORD saved=GetLastError();M37_RECORD *r;
 if(!m)return;
 seq=InterlockedIncrement(&m->next);
 if(seq<=0){InterlockedIncrement(&m->dropped);SetLastError(saved);return;}
 r=&m->slots[(seq-1)%M37_CAP];
 if(InterlockedCompareExchange(&r->busy,1,0)){InterlockedIncrement(&m->dropped);SetLastError(saved);return;}
 old=InterlockedCompareExchange(&r->commit,0,0);
 if(old>seq){InterlockedIncrement(&m->dropped);InterlockedExchange(&r->busy,0);SetLastError(saved);return;}
 InterlockedExchange(&r->commit,0);
 r->tick=GetTickCount();r->pid=GetCurrentProcessId();r->tid=GetCurrentThreadId();
 r->event=event;r->object=object;r->value=value;r->error=error;
 InterlockedExchange(&r->commit,seq);InterlockedExchange(&r->busy,0);SetLastError(saved);
}
static BOOL m37_get(M37_MAP *m,LONG seq,M37_RECORD *out)
{
 M37_RECORD *r;LONG first;
 if(!m || seq<=0)return FALSE;r=&m->slots[(seq-1)%M37_CAP];
 if(InterlockedCompareExchange(&r->busy,0,0))return FALSE;
 first=InterlockedCompareExchange(&r->commit,0,0);if(first!=seq)return FALSE;
 memcpy(out,r,sizeof(*out));
 return !InterlockedCompareExchange(&r->busy,0,0) && InterlockedCompareExchange(&r->commit,0,0)==first;
}
#ifdef COM_DIAG38
/* API failure makes output parameters untrusted: record no claimed bytes/status. */
static void m38_status_record(M37_MAP *m,DWORD transfer,BOOL ok,DWORD bytes,DWORD usbError,DWORD apiError)
{
 m37_put(m,M37_STATUS,transfer,ok,apiError);
 if(ok)m37_put(m,M38_USB_RESULT,transfer,bytes,usbError);
 else m37_put(m,M38_USB_RESULT_UNAVAILABLE,transfer,0,apiError);
}
#endif
static LPCWSTR m37_event_name(DWORD event)
{
 static LPCWSTR names[]={L"UNKNOWN",L"ATTACH_ENTER",L"DEVICE_INFO",L"REGISTER_BEGIN",L"REGISTER_END",L"READY",L"DETACH_ENTER",L"WORKER_JOIN_BEGIN",L"WORKER_JOIN_END",L"UNREGISTER_BEGIN",L"UNREGISTER_END",L"NOTIFY_JOIN_BEGIN",L"NOTIFY_JOIN_END",L"RX_JOIN_BEGIN",L"RX_JOIN_END",L"DEACTIVATE_BEGIN",L"DEACTIVATE_END",L"LOCK_RX_BEGIN",L"LOCK_RX_END",L"LOCK_TX_BEGIN",L"LOCK_TX_END",L"LOCK_IO_BEGIN",L"LOCK_IO_END",L"PIPE_CLOSE_BEGIN",L"PIPE_CLOSE_END",L"PORT_RELEASE_BEGIN",L"PORT_RELEASE_END",L"CLEANUP_END",L"PARENT_RELEASE_BEGIN",L"PARENT_RELEASE_END",L"DETACH_RETURN_INTENT",L"ABORT_BEGIN",L"ABORT_END",L"STATUS",L"TRANSFER_CLOSE_BEGIN",L"TRANSFER_CLOSE_END",L"RX_EXIT",L"NOTIFY_EXIT",L"RETAIN",L"ACTIVATE_BEGIN",L"ACTIVATE_END"};
 static LPCWSTR extra[]={L"USB_RESULT",L"USB_RESULT_UNAVAILABLE",L"OPEN_BEGIN",L"OPEN_END",L"CLOSE_BEGIN",L"CLOSE_END",L"READ_BEGIN",L"READ_END",L"WRITE_BEGIN",L"WRITE_END",L"IOCTL_BEGIN",L"IOCTL_END",L"RX_PUSH",L"RX_EVENTS",L"MASK_STATE",L"WAIT_BEGIN",L"WAIT_END",L"WAIT_MASK",L"TRANSFER_ISSUE",L"TRANSFER_SUBMITTED"};
 if(event>=M38_USB_RESULT && event<=M38_TRANSFER_SUBMITTED)return extra[event-M38_USB_RESULT];
 return event<sizeof(names)/sizeof(names[0])?names[event]:names[0];
}
