/* Independent strict parser for the 10-byte A1/20 notification observed
 * on ME3630 MI1/MI2. DWORD-only private snapshot ABI; CTS is never known. */
typedef struct {
    DWORD size,known,status,raw,accepted,rejected,tick,error;
} M36_NOTIFY_STATE;
#define M36_NOTIFY_QUERY 0x00223ff8
#define M36_NOTIFY_KNOWN (MS_DSR_ON|MS_RING_ON|MS_RLSD_ON)
static BOOL m36_parse_notify(const BYTE *data,DWORD bytes,DWORD iface,PDWORD raw,PDWORD status)
{
    DWORD signals,result=0;
    if(!data || !raw || !status || bytes!=10 || (iface!=1 && iface!=2))return FALSE;
    if(data[0]!=0xa1 || data[1]!=0x20 || data[2] || data[3] ||
       data[4]!=iface || data[5] || data[6]!=2 || data[7] || data[9] || (data[8]&0x80))return FALSE;
    signals=data[8];
    if(signals&1)result|=MS_RLSD_ON;
    if(signals&2)result|=MS_DSR_ON;
    if(signals&8)result|=MS_RING_ON;
    *raw=signals;*status=result;return TRUE;
}
