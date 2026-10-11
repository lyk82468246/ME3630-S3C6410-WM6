/* DWORD-only overflow-safe serial deadline policy. INFINITE means disabled. */
static BOOL m36_timeout_valid(const COMMTIMEOUTS *t)
{
 BOOL special=t->ReadIntervalTimeout==MAXDWORD && t->ReadTotalTimeoutMultiplier==MAXDWORD && t->ReadTotalTimeoutConstant && t->ReadTotalTimeoutConstant<=10000;
 BOOL immediate=t->ReadIntervalTimeout==MAXDWORD && !t->ReadTotalTimeoutMultiplier && !t->ReadTotalTimeoutConstant;
 BOOL normal=t->ReadIntervalTimeout<=10000 && t->ReadTotalTimeoutMultiplier<=10000 && t->ReadTotalTimeoutConstant<=10000;
 return (special || immediate || normal) && t->WriteTotalTimeoutMultiplier<=10000 && t->WriteTotalTimeoutConstant<=10000;
}
static BOOL m36_timeout_total(DWORD mult,DWORD constant,DWORD count,PDWORD total)
{
 if(!mult && !constant){*total=INFINITE;return TRUE;}
 if(mult && count>(0x7ffffffe-constant)/mult)return FALSE;
 *total=count*mult+constant;return TRUE;
}
