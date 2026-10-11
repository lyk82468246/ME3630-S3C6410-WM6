/* Minimal legacy ADB diagnostics per AOSP android-4.4_r1/adb/protocol.txt.
 * Fixed read-only /proc shell command only; AUTH stops without signing keys,
 * approval bypass, pairing, writes, or other shell services. Banner not logged.
 * USB receives into 4096-byte local storage, even for 24-byte headers. */
#define ADB_CNXN 0x4e584e43
#define ADB_AUTH 0x48545541
#define ADB_OPEN 0x4e45504f
#define ADB_OKAY 0x59414b4f
#define ADB_CLSE 0x45534c43
#define ADB_WRTE 0x45545257
typedef struct {DWORD command,arg0,arg1,length,checksum,magic;} ADB_HEADER;
typedef char adb_header_must_be_24[(sizeof(ADB_HEADER)==24)?1:-1];
static DWORD adb_sum(const BYTE *data,DWORD length)
{DWORD i,sum=0;for(i=0;i<length;i++)sum+=data[i];return sum;}
static BOOL adb_send(PROBE *p,USB_PIPE out,DWORD command,DWORD a,DWORD b,const BYTE *data,DWORD length)
{
 ADB_HEADER h;USB_TRANSFER t;DWORD bytes;
 h.command=command;h.arg0=a;h.arg1=b;h.length=length;h.checksum=adb_sum(data,length);h.magic=command^MAXDWORD;
 t=p->usb->lpIssueBulkTransfer(out,NULL,NULL,USB_NO_WAIT|USB_OUT_TRANSFER,sizeof(h),&h,0);
 if(!wait_transfer(p,t,1000,&bytes) || bytes!=sizeof(h))return FALSE;
 if(length){t=p->usb->lpIssueBulkTransfer(out,NULL,NULL,USB_NO_WAIT|USB_OUT_TRANSFER,length,(LPVOID)data,0);
  if(!wait_transfer(p,t,1000,&bytes) || bytes!=length)return FALSE;}
 return TRUE;
}
static BOOL adb_receive(PROBE *p,USB_PIPE in,ADB_HEADER *h,BYTE *data)
{
 USB_TRANSFER t;DWORD bytes;
 t=p->usb->lpIssueBulkTransfer(in,NULL,NULL,USB_NO_WAIT|USB_IN_TRANSFER|USB_SHORT_TRANSFER_OK,sizeof(*h),data,0);
 if(!wait_transfer(p,t,1500,&bytes) || bytes!=sizeof(*h))return FALSE;
 memcpy(h,data,sizeof(*h));
 if(h->magic!=(h->command^MAXDWORD) || h->length>4096){logline(L"ADB_INVALID_HEADER\r\n");return FALSE;}
 if(h->length) {
  t=p->usb->lpIssueBulkTransfer(in,NULL,NULL,USB_NO_WAIT|USB_IN_TRANSFER|USB_SHORT_TRANSFER_OK,h->length,data,0);
  if(!wait_transfer(p,t,1500,&bytes) || bytes!=h->length)return FALSE;
 }
 if(adb_sum(data,h->length)!=h->checksum){logline(L"ADB_INVALID_CHECKSUM\r\n");return FALSE;}
 return TRUE;
}
static void adb_boot_probe(PROBE *p)
{
 USB_PIPE in=NULL,out=NULL;ADB_HEADER h;BYTE *data;DWORD i,j,k,c,id,remote=0,start=GetTickCount();WCHAR text[257];BOOL established=FALSE;
 static const BYTE hello[]="host::";
 static const BYTE command[]="shell:cat /proc/uptime /proc/sys/kernel/random/boot_id";
 if(!p->adbFound){logline(L"ADB_INTERFACE_NOT_FOUND\r\n");return;}
 data=(BYTE*)LocalAlloc(LPTR,4096);if(!data)return;
 in=p->usb->lpOpenPipe(p->device,&p->adbIn);out=p->usb->lpOpenPipe(p->device,&p->adbOut);
 logline(L"ADB_START tick=%lu pipes=%lu/%lu\r\n",start,in!=NULL,out!=NULL);
 if(!in || !out)goto done;
 if(!adb_send(p,out,ADB_CNXN,0x01000000,4096,hello,sizeof(hello)) || !adb_receive(p,in,&h,data))goto done;
 if(h.command==ADB_AUTH){logline(L"ADB_AUTH_REQUIRED type=%lu length=%lu; STOP\r\n",h.arg0,h.length);goto done;}
 if(h.command!=ADB_CNXN || h.arg0!=0x01000000 || h.arg1<sizeof(command)){logline(L"ADB_UNSUPPORTED_HANDSHAKE code=%08lx version=%08lx max=%lu\r\n",h.command,h.arg0,h.arg1);goto done;}
 logline(L"ADB_CONNECTED version=%08lx max=%lu\r\n",h.arg0,h.arg1);
 /* Up to four samples per attachment; never hold shutdown indefinitely. */
 for(id=1;id<=4 && GetTickCount()-start<30000;id++) {
  if(WaitForSingleObject(p->stop,0)==WAIT_OBJECT_0)break;
  remote=0;established=FALSE;
  if(!adb_send(p,out,ADB_OPEN,id,0,command,sizeof(command)))break;
  for(i=0;i<16 && GetTickCount()-start<30000;i++) {
   if(!adb_receive(p,in,&h,data))goto done;
   if(h.command==ADB_AUTH){logline(L"ADB_AUTH_REQUIRED; STOP\r\n");goto done;}
   if(h.arg1!=id){logline(L"ADB_UNEXPECTED_STREAM\r\n");goto done;}
   if(h.command==ADB_OKAY){if(established && remote!=h.arg0)goto done;remote=h.arg0;established=remote!=0;}
   else if(h.command==ADB_WRTE && established && h.arg0==remote) {
    /* Only this fixed /proc command's output is logged, capped in short chunks. */
    for(j=0;j<h.length;j+=256){k=h.length-j;if(k>256)k=256;for(c=0;c<k;c++)text[c]=(WCHAR)data[j+c];text[k]=0;
     logline(L"ADB_BOOTINFO sample=%lu tick=%lu [%s]\r\n",id,GetTickCount(),text);}
    if(!adb_send(p,out,ADB_OKAY,id,remote,NULL,0))goto done;
   } else if(h.command==ADB_CLSE){logline(L"ADB_SHELL_CLOSED sample=%lu\r\n",id);established=FALSE;break;}
   else {logline(L"ADB_UNEXPECTED_COMMAND code=%08lx\r\n",h.command);goto done;}
  }
  if(established){adb_send(p,out,ADB_CLSE,id,remote,NULL,0);established=FALSE;break;}
  if(id<4 && WaitForSingleObject(p->stop,4000)==WAIT_OBJECT_0)break;
 }
done:
 if(in)p->usb->lpClosePipe(in);if(out)p->usb->lpClosePipe(out);LocalFree(data);
 logline(L"ADB_END elapsed=%lu\r\n",GetTickCount()-start);
}
