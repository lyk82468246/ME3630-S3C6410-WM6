/* Independent strict parser for the 10-byte A1/20 notification observed
 * on ME3630 MI1/MI2. DWORD-only private snapshot ABI; CTS is never known. */
typedef struct {
    DWORD size,known,status,raw,accepted,rejected,tick,error;
} M36_NOTIFY_STATE;
#define M36_NOTIFY_QUERY 0x00223ff8
#define M36_NOTIFY_KNOWN (MS_DSR_ON|MS_RING_ON|MS_RLSD_ON)
#define M36_LINE_EVENTS (EV_DSR|EV_RLSD|EV_RING)
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
#ifdef COM_V18
/* Caller holds the port lock. First known sample is a baseline, not an edge.
 * Pending events coalesce; they report occurrence, not transition counts. */
static BOOL m36_update_notify(M36_NOTIFY_STATE *s,const BYTE *data,DWORD bytes,DWORD iface,DWORD tick,DWORD mask,PDWORD pending)
{
    DWORD raw,status,changed,events=0;
    if(!m36_parse_notify(data,bytes,iface,&raw,&status)){s->rejected++;return FALSE;}
    changed=(s->status^status)&s->known&M36_NOTIFY_KNOWN;
    if(changed&MS_DSR_ON)events|=EV_DSR;
    if(changed&MS_RLSD_ON)events|=EV_RLSD;
    if((changed&MS_RING_ON) && (status&MS_RING_ON))events|=EV_RING;
#ifdef COM_DIAG35
    if((raw&4) && (!(s->raw&4) || !s->accepted))events|=EV_BREAK;
    if(raw&0x70)events|=EV_ERR;
#endif
    s->raw=raw;s->status=status;s->known=M36_NOTIFY_KNOWN;
    s->accepted++;s->tick=tick;s->error=0;
    *pending|=events&mask;return TRUE;
}
#ifdef COM_DIAG35
static BOOL m36_event_mask_supported(DWORD mask)
{return !(mask&~(EV_RXCHAR|M36_LINE_EVENTS|EV_CTS|EV_BREAK|EV_ERR));}
static DWORD m36_notify_errors(DWORD raw)
{return ((raw&4)?CE_BREAK:0)|((raw&0x10)?CE_FRAME:0)|((raw&0x20)?CE_RXPARITY:0)|((raw&0x40)?CE_OVERRUN:0);}
#endif
#endif
