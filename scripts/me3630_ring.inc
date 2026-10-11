/* Caller owns queue lock. Shared verbatim by native test and USB driver. */
static BOOL ring_push(PORT *p,const BYTE *data,DWORD count)
{
 DWORD tail,n;
 if(count>sizeof(p->queue)-p->qCount)return FALSE;
 tail=(p->qHead+p->qCount)%sizeof(p->queue);n=sizeof(p->queue)-tail;if(n>count)n=count;
 memcpy(p->queue+tail,data,n);memcpy(p->queue,data+n,count-n);p->qCount+=count;return TRUE;
}
static DWORD ring_pop(PORT *p,BYTE *data,DWORD count)
{
 DWORD n;if(count>p->qCount)count=p->qCount;
 n=sizeof(p->queue)-p->qHead;if(n>count)n=count;
 memcpy(data,p->queue+p->qHead,n);memcpy(data+n,p->queue,count-n);
 p->qHead=(p->qHead+count)%sizeof(p->queue);p->qCount-=count;return count;
}
