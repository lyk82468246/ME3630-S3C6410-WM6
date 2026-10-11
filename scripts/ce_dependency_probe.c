#include <windows.h>
#undef wsprintf
#undef wsprintfW
#define wsprintf wsprintfW

static HANDLE logFile;
static void line(LPCWSTR module, DWORD ordinal, DWORD present, DWORD error)
{
    WCHAR text[256];
    DWORD written;
    int length=wsprintf(text,L"%s ordinal=%lu present=%lu error=%lu\r\n",module,ordinal,present,error);
    WriteFile(logFile,text,length*sizeof(WCHAR),&written,NULL);
}
static void check(LPCWSTR dll, const DWORD *ords, DWORD count)
{
    HMODULE module=LoadLibrary(dll);
    DWORD i, error=module ? 0 : GetLastError();
    line(dll,0,module!=NULL,error);
    if(!module)return;
    for(i=0;i<count;i++) {
        FARPROC p=GetProcAddressW(module,(LPCWSTR)ords[i]);
        line(dll,ords[i],p!=NULL,p ? 0 : GetLastError());
    }
    FreeLibrary(module);
}
void mainCRTStartup(void)
{
    static const DWORD core[]={2,3,4,5,6,10,11,12,23,33,36,56,60,61,63,73,75,87,168,171,173,175,455,456,458,461,463,464,479,491,492,494,495,496,497,498,499,500,514,516,517,528,529,530,535,544,545,553,621,622,627,628,629,719,1018,1044,1046,1047,1063,1068,1092,1094,1095,1097,1132,1179,1180,1232,1396,1492,1645,1689,1693,1748,1876,2008,2508,2571,2572,2696};
    static const DWORD ddk[]={2,3,37,38,39,51,52,53,54,55,56};
    HMODULE shell;
    WCHAR text[256];
    DWORD written;
    int length;
    logFile=CreateFile(L"\\Temp\\ME3630-dependency-probe.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(logFile==INVALID_HANDLE_VALUE)ExitProcess(1);
    check(L"coredll.dll",core,sizeof(core)/sizeof(core[0]));
    check(L"ceddk.dll",ddk,sizeof(ddk)/sizeof(ddk[0]));
    shell=LoadLibrary(L"aygshell.dll");
    length=wsprintf(text,L"aygshell RegistryNotifyCallback=%lu RegistryCloseNotification=%lu\r\n",shell && GetProcAddressW(shell,L"RegistryNotifyCallback")!=NULL,shell && GetProcAddressW(shell,L"RegistryCloseNotification")!=NULL);
    WriteFile(logFile,text,length*sizeof(WCHAR),&written,NULL);
    if(shell)FreeLibrary(shell);
    length=wsprintf(text,L"COMPLETE\r\n");
    WriteFile(logFile,text,length*sizeof(WCHAR),&written,NULL);
    CloseHandle(logFile);
    ExitProcess(0);
}
