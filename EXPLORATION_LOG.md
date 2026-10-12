# 探索记录

## 2026-10-12：外挂5V扩展坞 A/B 尚未产生新的 USB 枚举

用户将模块改接到支持外挂5V供电的扩展坞后，仍保持 `x+n`，未重启WM6、切模式、改SIM/APN或复位。以Joe的32位RAPI先等待15秒运行 `scripts/probe_active.ps1 -UsbState -OutDir evidence/2026-10-12-d40-powered-attach`：`ACTIVE_ME3630_PORT_BLOCKS=0`，精确匹配仍指向 `ME3630ComV18D40.dll`。随后约90秒内每10秒采样一次，共6次，全部为0；D40累积日志重新读取后ATTACH事件计数未增加，也没有观察到这个外挂供电时间段产生新的 `19d2:1432` ATTACH。结果见 `evidence/2026-10-12-d40-powered-observation/observation.txt`，完整注册表快照仍为本地/忽略证据。

当前USB注册快照仍有HCD1/S3C6410 OHCI及原有LeadCore/WMDC客户端，但没有Hub/USBHUB客户端映射。这个结果不能证明扩展坞没有给下游供电，却说明“扩展坞已经让WM6看到ME3630”尚未成立；可能是老WM6 USB Host不支持该Hub数据路径、上游VBUS检测不满足，或模块没有在扩展坞下完成下游枚举。因此本轮没有运行无线探针或RAS：没有新COM端口，继续拨号不会增加信息量。

下一步受控对照应使用转接板明确支持的外部输入，或确认扩展坞在上游仍提供WM6所需的USB数据/VBUS信号；若扩展坞只是供电Hub而WM6不支持Hub，应改用不经过Hub的数据连接并单独给转接板合规供电。仍不能把原始5V直接送入ME3630的3.0--4.0V V_MAIN。只有再次看到新的 `19d2:1432 ATTACH` 后，才重跑D40无线状态和保存RAS测试。

## 2026-10-12：D40 在 x+n 下能枚举并激活，但本轮 USB 链路无法保持

用户将模块切回 WM6 使用的 `x+n`（19d2:1432）并重新插入。使用 Joe 的 32 位 WMDC/RAPI 会话运行 `scripts/probe_active.ps1 -UsbState`：初始快照和等待25秒后的重试均为 `ACTIVE_ME3630_PORT_BLOCKS=0`；随后约60秒、每5秒一次的12个采样也全部为0。注册表匹配仍明确指向 `ME3630ComV18D40.dll`，所以这些0只能表示采样时没有活动端口，不能单独证明模块从未枚举。

重新读取驱动日志后得到更有区分度的当前设备证据：D40安装后确实收到 `ATTACH vid=19d2 pid=1432`，随后多次成功激活 `COM1`、`COM5` 并记录 `V18 READY`；激活期间还出现版本握手 `00223ffc` 和 `GetCommModemStatus` 的 `001b0034` 请求。D40这几次状态请求没有再出现旧D39的 `error=50 / ERROR_NOT_SUPPORTED` 返回。随后各轮出现 `RX_EXIT error=1117` 或 `1167`、部分轮次 `usb_error=257`，最后 `DETACH`，并开始下一次ATTACH。结论是D40不是“没有匹配/完全不能加载”，而是USB设备在激活后反复失联；完整累积日志仍按本地证据保留，聚焦摘要见 `evidence/2026-10-12-d40-usb-observation/observation.txt`。

使用 `scripts/run_networkprobe.ps1 -OutDir evidence/2026-10-12-d40-radio` 做180秒只读探针，结果 `generations=0 all_queries=0`，期间没有一次COM1通过身份检查并保持到发AT；RAS枚举只看到既有的 `Cellular Line`。随后用 `scripts/run_rassaved.ps1 -Dial -ExpectedDiagnostic 39 -OutDir evidence/2026-10-12-d40-ras-dial` 做一次有界拨号：保存条目唯一匹配 `ME3630 Test COM5`、`*99***2#`、PPP/IPv4，但等待30秒后 `PORT_UNAVAILABLE error=55`，因此 `RasDial` 实际没有被调用，没有修改连接条目。

本轮已验证：D40的调制解调器状态接口修复不是当前首要阻塞；当前阻塞发生在COM5/RAS交接之前的USB端口保持。结合用户提供的同一模块在Win11 MBIM下可上网的对照，下一次最高价值的A/B是给WM6 Host/转接板提供合规的外部供电或有源USB路径，再重复同一只读无线探针和保存RAS条目。ME3630主电源仍必须是模块要求的3.0--4.0V，不能把USB 5V直接送入模块主电源。若供电改善后能稳定得到 `CPIN/CSQ/CREG` 与COM5，再继续PPP/Unimodem握手分类；在当前没有稳定端口的情况下继续改RAS代码没有诊断价值。

## 2026-10-12：整机 PPP 路线已进入 RAS，当前阻塞收敛为无线未注册

用户要求目标是“整个 WM6 上网”，因此本轮优先验证系统级 PPP/RAS，而不是继续把应用级 socket 封装成虚拟网卡。WMDC 已恢复，使用 Joe 身份的 32 位 RAPI（`C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe`）只读检查活动端口：本项目自己的 ME3630 COM1/COM5 仍由 `ME3630ComV18D39.dll` 提供，HCD1 为 S3C6410 OHCI；没有把 WMDC Function 侧 `rndisfn.dll` 或 LeadCore 调制解调器当作 ME3630 主机网络通道。

使用的命令/工具与证据：

- `scripts/run_rassaved.ps1 -ExpectedDiagnostic 39 -OutDir evidence/2026-10-12-ras-inspect`：`TAPI_INIT=0`，枚举到本项目 `ME3630 Test COM5`；保存条目“ME3630 电信”唯一匹配，设备名、`*99***2#`、PPP/IPv4 参数均正确。`TAPI_OPEN_MONITOR=0`；仅 `TAPI_PORTNAME` 查询返回 `0x80000048`，没有阻止 RAS 使用该端口。证据见 `evidence/2026-10-12-ras-inspect/inspect.txt`。
- `scripts/run_rassaved.ps1 -Dial -ExpectedDiagnostic 39 -OutDir evidence/2026-10-12-ras-dial`：`PORT_DIAGNOSTIC build=39`、`PORT_VERIFIED version=18`、`DIAL_START error=0`，随后状态从 0/1/3 进入 8193，最终 `state_error=619`；程序只挂断自己的句柄，未修改保存条目。说明 TAPI/RAS 已经真正到达 ME3630 调制解调器，但会话未进入 PPP/数据连接。证据见 `evidence/2026-10-12-ras-dial/dial.txt`。
- 扩展只读 `scripts/network_probe.c` 查询并运行 `scripts/run_networkprobe.ps1 -OutDir evidence/2026-10-12-radio-probe`：`CPIN READY`，但 `CSQ 99,99`、`CREG/CEREG 0,2`、`CGATT 0`、`ZPAS: "NO SERVICE"`，`^SYSINFO: 0,0,0,0,1`；当前 `ZSWITCH:x`、`ZADSET:n`，CID1/CID2 的 `ctnet` 均未激活。`ZCHIPTEMP:320,320,342` 也没有显示出明显过热。所有查询均为只读，没有改 APN、模式、IMEI、SIM 或复位。证据见 `evidence/2026-10-12-radio-probe/test.txt`。

已验证结论：本轮 `619` 不是“没有 COM5”、不是“找不到 RAS 条目”、也不是当前驱动未注册到 TAPI；首要失败条件是 ME3630 尚未注册运营商网络。只要 `CREG/CEREG` 仍为 0,2、`CSQ` 仍为 99,99、`CGATT` 为 0，继续重写 COM/Unimodem 驱动不会产生 IP。下一次成功判据应先达到 `CREG/CEREG=1` 或 `5`、有数值 `CSQ`、`CGATT=1`，再复用同一个已保存 RAS 条目重试，并检查 WM6 是否获得 PPP 地址/默认路由。

并行 NDIS/RNDIS 路线仍未选作当前主线：本机 Windows CE SDK/下载资料中只找到 `rndismini.h`，没有可直接部署的 CE 主机侧 `USB8023.DLL`/`RNDISMP.DLL` 及其库；`rndisfn.dll` 是 WMDC 设备功能侧组件。即使补齐主机协议驱动，也不能绕过当前 `NO SERVICE`。因此本轮没有切换 `x+r`/ECM/MBIM、没有重启 WM6、没有改变保存连接，也没有写入模块配置。

## 2026-10-12：运营商扫描被 SIM busy 阻断，补充 C1A 射频/供电前置条件

为区分“无覆盖/频段不匹配”和“SIM/射频子系统尚未就绪”，扩展只读 `network_probe.c` 加入 `AT+ZBAND=?`、`AT+ZCDS?`、`AT+ZMONI?` 和 `AT+COPS=?`；普通查询预算仍为约2.2秒，运营商扫描预算为120秒，runner总等待上限为210秒。第一次扫描从tick1663509开始，第二次在等待30秒后从tick1717937开始；两次均能返回`AT`/`OK`和`AT+CFUN:1`，但`AT+CPIN?`返回`+CME ERROR: SIM busy`，之后`CSQ`、`COPS`、`CREG/CEREG`、`CGATT`、`ZBAND`、`ZMONI`、`ZCDS`等查询均为`ERROR`，`^SYSINFO`仍为`0,0,0,0,1`。`AT+COPS=?`没有返回运营商列表，不能据此断言现场无电信信号，也不能据此断言SIM损坏；当前更准确的结论是SIM/网络服务层在这两个采样点未就绪。聚焦证据见`evidence/2026-10-12-radio-scan/test.txt`和`evidence/2026-10-12-radio-scan-retry/test.txt`。

联网核对官方硬件开发指南：`ME3630 C1A_MP0`列出的LTE FDD为B1/B3，LTE TDD为B38/B39/B40/B41，并列出CDMA BC0；C1A的LTE B1工作电流表为约550--600mA、B3约500--590mA，模块主电源要求3.0--4.0V且主机必须持续、安全供电。工信部公开的中国电信许可频率包含1700/1800MHz和1900/2100MHz等范围。由此只能推断C1A与中国电信在B1/B3存在频段重叠，不能推断当前地点一定有可用小区；是否能注册仍取决于实际部署频段、天线、SIM业务状态和供电。来源：GOSUNCN《Hardware Development Guide of ME3630 mini-PCIE module》 https://welinkjapan.co.jp/pdf/GOSUNCN%20Hardware%20Development%20Guide%20of%20ME3630%20mini-PCIE%20module_V1.5.pdf ，工信部频率统计 https://www.miit.gov.cn/jgsj/wgj/kpzs/art/2021/art_9105abf80949d49bd9065c1b6ebb06d2a.html 。

下一步不需要先重写WM6驱动：应先确认转接板在模块端实际提供约3.0--4.0V，并能承受C1A发射/搜网时约600mA的瞬态与持续需求；确认主天线已接、SIM在手机中能注册中国电信，再保持模块上电等待`CPIN READY`后重跑只读探测。只有得到`CREG/CEREG=1/5`、数值`CSQ`和`CGATT=1`，才继续同一保存RAS条目的系统拨号；若这些条件成立而RAS仍失败，再回到PPP交接代码。

## 2026-10-12：用户提供 Win11 MBIM 成功基线，重新区分驱动与供电

用户报告同一模块切到MBIM后接入Win11可以正常上网；这是用户实测的对照基线，不是本任务通过RAPI采集的当前设备证据。它显著降低“SIM失效/运营商业务完全不可用/模块RF永久损坏”的可能性，但不能排除WM6 USB Host供电不足，因为Win11主机、USB口、线材和转接板供电条件可能不同，也不能证明`x+n`串口/NDIS组合在WM6上等价于MBIM。

本机随后只读检查`usbipd list`，当前没有ME3630枚举，只有WMDC Remote Adapter；没有趁模块在Win11上再切换或写配置。历史WM6证据显示两个独立方向：D38第一次RAS `619`时，USB入队、`EV_RXCHAR`和多次系统Read仍成功，RAS失败前没有DETACH，说明那一轮自研串口传输至少活着到达RAS交接；D38第二次RAS `633`时出现USB完成状态5、Read错误1117、Write失败及随后DETACH/重新ATTACH，说明另一轮确有USB设备/物理链路级失效。两者不能合并成单一根因。聚焦摘要见`evidence/2026-10-11-diag38/state-summary.txt`。

下一轮判别顺序：模块切回WinCE用的`x+n`并插回WM6后，先用现有D39只读采集确认是否能稳定保持`CPIN READY`/`CSQ`/注册状态；同时观察是否出现USB完成错误、DETACH或boot变化。若换成可持续供电的USB Host/外部合规3.0--4.0V电源后状态恢复，根因偏向供电/物理链路；若无线状态正常且USB无错误但同一RAS仍为619，再增加PPP握手分类日志，检查Unimodem实际发出的拨号命令和模块是否返回`CONNECT`，届时才继续改驱动的RAS交接。当前不因Win11的MBIM成功而把WM6改成MBIM，WM6仍缺主机MBIM协议栈。

静态审计历史D38/D39的RAS交接日志又发现一个可直接修复的驱动缺口：Unimodem请求`GetCommModemStatus`（IOCTL `0x001b0034`）时旧实现返回`ERROR_NOT_SUPPORTED=50`。V18/D39源码现改为成功返回通知端点实际已知的`MS_DSR_ON`、`MS_RLSD_ON`、`MS_RING_ON`，未知CTS不置位；在没有任何有效通知前返回零。此改动不会伪造载波，目标是避免Unimodem因“接口不支持”提前终止RAS；尚未在本轮实机上验证，必须等模块回到WM6后用新构建重新观察`619`与PPP交接。

旧D39同名DLL因仍被WM6加载而无法覆盖（RAPI设备错误5），所以生成唯一文件名`ME3630ComV18D40.dll`并只切换本项目精确匹配项。以Joe 32位RAPI完成15/15 ownership、36/36 compatibility、29/29 lifecycle隔离测试，`D40_INSTALL result=0 next_attach_only=1`；随后只读快照显示`ME3630Probe -> ME3630ComV18D40.dll`、`ACTIVE_ME3630_PORT_BLOCKS=0`。这是“候选修复已准备并预装”的证据，不是模块已在D40上成功枚举或已联网；实际硬件验证留待模块从Win11回到WM6并切回`x+n`。

## 2026-10-12：评估用内部socket封装WM6虚拟网卡

用户提出把ME3630应用级联网封装成虚拟网卡以获得系统级联网。核对Windows CE 5.0 NDIS `MiniportInitialize`文档：CE API层面支持初始化物理或虚拟网络适配器；NDIS intermediate driver可以向上暴露virtual adapter。结论是“可行但不是简单包装”：系统TCP/IP发给miniport的是IP/Ethernet包，而ME3630 `ZIPCALL`/`ZIPOPEN`提供的是已建立的TCP/UDP socket。若以内部socket为后端，必须再写内核态NDIS virtual miniport、用户态IPC/网关、流到socket的NAT/代理、返回包重封装、DNS/MTU/超时/校验和/断线清理；普通应用或`NDISUIO`不能直接创建完整系统网卡。

路线比较：`ZIPCALL`直连适合先验证应用级数据但不是系统级；`ZIPCALL`+虚拟miniport理论上能覆盖系统TCP/IP但工作量很高；让模块通过PPP/NDIS/RNDIS/ECM提供原始IP包，再接标准CE网络层，更符合系统模型，优先于socket虚拟网卡。当前没有开始写虚拟网卡、没有切模式、没有配置APN或拨号。建议先用已存在的COM1只读通路验证`ZIPCALL`，验证成功后再决定是否投入NDIS miniport；若目标只是系统上网，继续补真正的NDIS主机数据通道或PPP仍是更短路线。

主资料：Windows CE 5.0 MiniportInitialize https://learn.microsoft.com/en-us/previous-versions/windows/embedded/aa447847%28v=msdn.10%29；Microsoft NDIS intermediate drivers https://learn.microsoft.com/en-us/windows-hardware/drivers/network/ndis-intermediate-drivers 。

## 2026-10-12：核验固件现成联网模式与自动拨号能力

用户提出可能无需在WM6上把ME3630配置成复杂通用Modem，而直接利用固件网络模式。联网核对GOSUNCN ME3630 AT Command Reference Guide第12.8节和第10.2节，并检查本地原始WinCE6 `USBManager.reg`：固件定义了四类USB组合。`x+n`/PID 19d2:1432是Windows XP/7/Vista/WinCE NDIS（DIAG、AT、MODEM、NDIS、ADB）；`x+r`是RNDIS；`l+e`/PID 19d2:1476是Linux/Android ECM；`8`/PID 0602是MBIM。用户提供的WinCE6包对1432只注册AT/MODEM/DEBUG串口类，没有可用的ZTEUSBNDIS_CLASS项；这不能证明所有厂商版本都没有NDIS，但证明手头包不是现成的1432网络适配方案。WM6历史快照还未找到主机USB8023/RNDISMP，已有rndisfn.dll属于WMDC设备功能侧，不能当作主机RNDIS驱动。

WMDC恢复后，以Joe的32位RAPI只读采集：`scripts/probe_active.ps1 -UsbState -OutDir evidence/2026-10-12-restored-wmdc`确认本项目自己的COM1/COM5活动，HCD1仍为OHCI；随后扩展只读`network_probe.c`查询`ZSWITCH?`、`ZADSET?`、`ZECMCALL?`、`ZNVREAD=auto_dial_mode`、`ZNVREAD=ecmcall_urc`、`ZIPCFG?`、`ZIPCALL?`、`ZPAS?`、`ZSNT?`，运行`scripts/run_networkprobe.ps1 -OutDir evidence/2026-10-12-network-probe-restored`。实际返回`ZSWITCH:x`、`ZADSET:n`，确认当前为x+n/NDIS；两个auto-dial NV查询均ERROR，符合手册只在较新J2版本支持的限制，不把自动拨号能力归给当前C1AV2.0B01。`ZIPCFG?`为空、`ZIPCALL?`为0，表明内部PS/socket命令可查询但未配置/未启动；没有执行设置APN或拨号。当前仍为CPIN READY、CSQ99/99、CREG/CEREG0/2、CGATT0、ZPAS NO SERVICE，不能把未联网归因于USB模式。

结论：若目标是系统范围透明上网，优先保留官方WinCE标注的x+n并单独解决WM6主机侧USB NDIS miniport；只凭NDIS.DLL或WMDC的rndisfn.dll不够。x+r/RNDIS、l+e/ECM和8/MBIM都不能绕过主机协议驱动，MBIM尤其不适合作为WM6捷径。若目标只是自有应用访问服务器，可进一步在不经过PPP/RAS的前提下评估`ZIPCFG`/`ZIPCALL`/`ZIPOPEN`模块内部socket；它不会给WM6系统创建网卡。详细路线与限制见`scripts/MODE_AUTO_NETWORK.md`。本轮只读，未切模式、改APN、拨号、重启或更改保存的RAS条目。

主资料：GOSUNCN ME3630 AT Command Reference Guide https://welinkjapan.co.jp/pdf/GOSUNCN%20AT%20Command%20Reference%20Guide%20of%20Module%20Product%20ME3630.pdf 。

## 2026-10-12：评估先在Win11调通再移植WM6的可行性（仅方案，无设备变更）

用户询问调制解调器API是否高度一致、能否PC调通后微调重编译。核对ras_saved.c、me3630_com.def/open.inc及微软原始接口文档：AT/APN/拨号响应和协议状态机可设计为共享C核心；CreateFile/ReadFile/SetCommState等上层名称相近，但CE不支持FILE_FLAG_OVERLAPPED，RAS通知/配置也不能照搬。CE RasDial文档要求异步窗口通知NotifierType FFFFFFFF、连接条目存注册表；桌面文档支持RASDIALFUNC系列及PBK。此前桌面回调误用于CE返回87、改HWND后被接受的历史，正是具体反例，不是今日新硬件结果。

底层模型不同：当前CE DLL实现USBDeviceAttach/USB_FUNCS与COM_*流入口，NT桌面USB驱动采用其自身PnP/框架/安装模型。现有32位DWORD指针上下文、CE专用IOCTL、生命周期/同步取消亦不可视为x64源码原样可编译。Win11若使用厂商串口驱动联网，只能建立模块/SIM/APN/PPP的对照，不能证明自研CE驱动、S3C6410 OHCI/供电及热插拔问题已解决。Win11用MBIM联网也不能验证本项目串口PPP路线；ADB枚举不是COM调制解调器就绪的证据。未声称本模块当前vendor-specific串口会自动匹配Usbser.sys。

建议（未实施）：在相同模块模式下先建立串口PPP基准、记录AT往返/CONNECT/PPP阶段，抽出纯协议/队列/事件状态核心；分别保留Win11串口/测试适配层和WM6 USB/COM/RAS适配层。PC可加速核心回归和定位协议问题，CE实机仍必须验证Unimodem交接、取消/拔插与重新枚举；不承诺整个驱动仅需微调重编译，也不为此默认新写一套Win11驱动。此轮仅研究和记录，无模式切换/安装/拨号/服务或设备写动作。

主来源：CE5 RasDial https://learn.microsoft.com/en-us/previous-versions/windows/embedded/aa450834(v=msdn.10)；桌面RasDialW https://learn.microsoft.com/en-us/windows/win32/api/ras/nf-ras-rasdialw；CE5 CreateFile https://learn.microsoft.com/en-us/previous-versions/windows/embedded/aa517318(v=msdn.10)；CE5 USBDeviceAttach https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms923216(v=msdn.10)；桌面USB框架 https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/implement-driver-entry-for-a-usb-driver--umdf-；Usbser类匹配 https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-driver-installation-based-on-compatible-ids 。

## 2026-10-12：继续尝试被PC端WMDC会话阻断，未重启或改WM6

用户要求继续尝试，按Asia/Shanghai客户端日期新建evidence/2026-10-12-retry，先以Joe32位PowerShell运行probe_active -UsbState：CeRapiInitEx在30s内无就绪会话。本日没有读取到WM6注册表/模块端口状态，不把昨日Active0当今日实测。Windows只读Get-Service确认WcesComm和RapiMgr均Stopped；Get-PnpDevice -PresentOnly看到Microsoft Windows Mobile Remote Adapter状态OK，这仅是WMDC Function侧USB设备，不是ME3630已被WM6 Host枚举的证据，Bluetooth设备支持项亦无关。

尝试仅Start-Service启动两项既有WMDC通信服务，均因当前权限不能打开服务而失败；未改启动类型/注册表、未停止其他服务，最终只读确认仍Stopped。用户回复暂时无法恢复WMDC。本轮无任何WM6进程启动/驱动安装/拨号/软重启/USB Host复位，昨日单次重启许可未沿用；当前ME3630、COM5、SIM状态未知，不能声称D39新实机测试通过或失败。

离线补run_lifesnapshot.ps1及run_lifesupervise.ps1可选OutDir，默认行为保留，让恢复后的不同日期/轮次单独归档、不覆盖旧证据。test_rapi_snapshot主机假RAPI回归5/5零失败；两脚本AST无错误且OutDir参数存在，git diff检查通过；未重编译或变更安装中的D39。聚焦state-summary记录结果，完整本地清单依.gitignore保留。下一步前置条件是恢复WMDC/RAPI可访问；恢复后先核验实际枚举和D39身份，再采集拨号交接。本轮源码与记录按授权提交推送。

## 2026-10-11：D39补精确参数/载波诊断，修正RAPI失败读取破坏本地证据

D39继承D38/D36，不改串口结果/所有权/取消/清理策略，追加IOCTL实际in/out长度、SetMask/Purge DWORD、DCB长度/波特率/flags/格式、超时字段及真实通知known/status/raw/age/事件。请求记录在元数据拒绝之前，仅读精确已知配置结构，不录串口载荷/密码/IMEI；线路组在已有锁内，CTS仍未知，GetCommModemStatus仍不支持。独立ME3630ComV18D39.dll/public18/private39及命名映射；SHA256 E632C4A9FA44A43B387118332C663BD72DE7978ACDAD820C1B730F73EE28C37D，见scripts/DIAG39.md，不声称兼容性/重枚举修复。

Joe32位WMDC部署前，ownership15/15 tick9672278、compat33/33 tick9674520、life29/29 tick9675717均零失败，共77项隔离检查；新增10项测试覆盖请求精确编码、空/短/未知不读、DCB来源不修改和线路有效性/错误码保留。9678003 install result0 next_attach_only1，仅原精确USB匹配DLL更换。第一观察9700364..9790452 next0/dropped0、拨号检查9713734..9744220 error55未调用RAS；只读Active0/HCD1仍在、匹配D39。用户拔插等30s后，第二观察9806079..9896081亦next0，检查9819013..9849166 error55未拨号。

用户明确允许这一次WM6软重启，PID3437227770；首次重连0x8000ffff，第二次恢复，无第二次重启。观察54517和保存连接检查73066在PC采集期限内只有不完整本地日志，不能判定具体RAS错误或永久驱动卡住；回收73066仍无COMPLETE。只读Active随后出现自己的COM1/5。新增独立one-shot内存快照及本任务进程清单（不截断运行观察日志、无USB/COM写），实测D39在运行：257316快照next115，231598 READY2；聚焦保留的415069..415160快照next159/dropped0，374619 ATTACH、379664 READY2，380248双端口known=e0/status0/raw0/age0，是真实载波低样本，不是无线注册状态或PPP交接载波方向，CTS未知。映射跨寿命序号重置；不能据零记录/不完整历史数出全部断开轮次。历史PID OpenProcess/退出码探索值不可靠，最终改为Toolhelp列本任务进程；415069/661617仅快照进程在，无“永远卡住”的证据。转储目录0亦不能排除异常。

新增监护程序持真实子进程句柄/映射、不强杀、不复位；555460..645547 elapsed90087，capture732995454和dial1270102386实际wait0/exit0。关闭后重读完整日志：观察555490..645507 next0/dropped0，检查555533..585664 error55未拨号。这次晚期端口缺失是正常完成，不反推初期不完整运行亦正常。诊断过程中发现RAPI TryCopy返回false之前会截断/写一部分本地目标，使失败轮次破坏上一次成功快照；新增rapi_snapshot staging，仅成功发布，失败/异常保留原证据，仍核验新start/COMPLETE，不称无损采集。5项主机假RAPI回归零失败；实际WM6 tick911177 life29复测亦通过，所有PS语法通过。已安装DLL哈希未变。

证据evidence/2026-10-11-diag39：install/隔离测试、双空观察、snapshot-active、监护关闭后完整日志、state-summary；完整注册表和failed-copy staging仍本地。最终只读Active再次为自己的端口0。保存电信连接/LeadCore/WMDC未改，未获PPP/IP；精确交接参数尚未实测，后续仍需模块实际可打开。恢复枚举缺口未修好，本轮未再次擅自重启或强卸载Host。源码、自编工具与聚焦证据按授权提交推送。

## 2026-10-11：D38补串口/USB内存轨迹，自然加载，拨号进入不同失败阶段

继续开发，D38继承D37/D36，不改串口结果/所有权/取消/清理策略，不重启或强卸载；独立DLL和D38命名映射，追加Open/Close/Read/Write/IOCTL/事件等待、RX入队/订阅保留/交付，以及USB实际状态/字节数。status API失败不声称输出有效，API TRUE亦不等于USB无错；field语义随事件变化，无串口载荷/IMEI/密码，详见scripts/DIAG38.md。19项记录器、33项兼容、15项所有权原生隔离共67项零失败(tick703355/702162/699914)，不是完整USB并发验证。D38及旧D33构建通过，runner语法通过。705688安装result0 next_attach_only1，自然757394 ATTACH、762415/762439双COM激活READY2。SHA256 11FB62DB92B9651ED8DE953586796368013A88BACE9A0B3118DF969988C9FB3F。观察runner初次新增编译参数异常在设备启动前失败，完整参数数组修正后成功；未改已加载DLL。保存连接测试新增private38强校验，避免误测旧版；条目不修改。

第一拨号811637..822109 elapsed10472，确认private38/public18，RasDial0，最终619/connected0。USB字节成功入队、EV_RXCHAR实际交付、系统Read6，多轮往返已越过旧卡点；817294读21、817303..817308异线程写37，仅提示数据路径交接，因不录载荷不能称PPP/LCP已建立。交接中DCB/SetMask不支持50、RLSD事件20、GetCommModemStatus001b0034不支持50；元数据Open的变更/等待仍按设计拒绝5，不能仅据它解释619。双Close817749，物理DETACH825714晚于RAS失败，不能直接归因该619为物理断开。第一观察763658..853783 next1009/dropped0，60个SKIP，941有效记录，尾部8个新记录未扫；记录不完整，不据缺失断言未发生。证据capture-first/dial-first619。

仅改独立D38观察器为先批量快照再格式化、每批flush和期限后最后一轮扫描，减少慢写盘使环覆盖，仍保留丢失提示，不改驱动。第二拨号995167..1000413 elapsed5246同样确认38，返回633：999405真实USB完成5（设备不响应）先于Read失败1117，999406/407双RX退出，999408 Write亦失败1117，999409双Close，999473 DETACH/999476返回意图，1007380再接入1012426 READY2。这次633有实测传输失败证据，不应和第一619或旧事件丢失混为一因；不能由USB5直接断定电源是唯一原因。第二观察973188..1063242 next711/dropped0、711有效、零SKIP/覆盖；又捕获1049522..1049549清理27ms、1057172再接入1062218 READY2，无整机重启。命名映射最后持有者释放后序号重置，不将跨观察序号当持续历史。保存连接/LeadCore/WMDC均未改，未获PPP/IP。最终证据capture-second/dial-second633、DIAG38及state-summary；下一步补交接DCB/拒绝的精确mask参数和载波实际状态，不能伪造CTS或把未知IOCTL当成功。源码和聚焦证据按授权提交推送。

## 2026-10-11：D37内存生命周期记录器，63项隔离测试通过并安装

用户继续开发后，先补低扰动诊断而非盲改异步取消/释放。D37保留D36接口、private37/版本18；仅此分支关闭同步文件logline和每次自动ADB探针，其他版本不变。固定256槽命名内存记录，不等待槽锁，冲突丢弃计数，读前后校验commit；记录开始/结束/返回码，覆盖GetDeviceInfo之前入口、注册/注销、线程join、DeactivateDevice、方向/配置锁、关管道、释放、同步Abort/Status/CloseTransfer。结束意图不是USB主机已继续执行的证明，cleanup循环结束不是所有资源成功释放；同步取消仍可能阻塞，未称热插拔已修好。独立观察程序持映射跨DLL卸载，日志由该程序写，不在USB回调写；无串口内容/IMEI/密码。详见scripts/DIAG37.md。

Joe32位原生ownership15/15(1278399)、compat33/33(1280649)、life15/15(1281818)全部零失败，life复测15/15(1381349)亦通过；包括真实子进程共享命名内存、并发写、覆盖/未提交拒读、错误码保留和映射寿命，不是USB拔插/取消并发验证。D37/旧D33构建通过，安装1284123 result0 next_attach_only1仅精确匹配，未重启/强卸载/改LeadCore/WMDC/保存连接。DLL哈希93C12ED850AEF4F9DA76AA19F16ED4136BCA4F58E7015ADA6654621B82CE7485。观察程序单独重编译为90秒，未重编译已安装DLL。life_capture1385829启动后用户确认“已重新插拔”，1475988完成next0/dropped0，Active0且精确匹配D37；不能据零记录证明旧回调未进入或电气故障。聚焦证据capture-before-reset。

用户明确批准这一次软重启，run_softreset启动3431585634；首次RAPI重连30秒超时，第二次成功，没有第二次重启、拨号或Host复位。90秒观察46668..136751、211条、dropped0、无SKIP/覆盖：70612真实ATTACH，75643/75674两COM激活成功，READY2。112370双RX退出1117，112401 DETACH；线程join0、注销TRUE、双DeactivateTRUE、三种锁取得及释放均走完，112455返回意图，约54ms。六个ClosePipe均FALSE/error6（无效句柄），不能据此断言双重关闭、泄漏或此前死锁；部分传输API FALSE/error0仍是失败，error是采样LastError，不是USB完成状态。120113再次ATTACH（距返回意图7658ms），125134/125157双COM重新激活READY2，实际证明这轮可继续枚举。不同时间只读Active0快照不能推翻有时间戳的激活/移除记录；未称当前端口一直可用。这轮排除其自身永久卡清理，不能否定旧版历史卡住，也不能把恢复唯一归因于禁日志/禁ADB之一。证据capture-reset-baseline，详见DIAG37。

第二观察158618..248822 next579/dropped0，无SKIP；启动回读历史时旧5条被覆盖（已保存在第一份完整记录），不能把回读的一轮重复计数。新增160945..161004清理59ms、168628再接入173676 READY2；209873..209876清理3ms、217557再接入222605 READY2。合计三轮断开、六次Deactivate成功、18次ClosePipe FALSE/error6，未观察到join超时/RETAIN。用户确认软重启后无任何拔插或供电改动；这次已实测自动重枚举，不再以无证据的整机重启代替观察。仍未解决掉线根因或完成PPP，不能称长期稳定。证据capture-reset-followup和state-summary；后续应补USB完成状态/字节数与低扰动串口调用轨迹，再验证D36事件修复及PPP，不直接盲重试失效句柄。源码、诊断二进制和聚焦证据按授权提交推送，完整清单保留本地。

## 2026-10-11：用户质疑频繁重启，先只读审计热插拔生命周期

本轮不重启、不安装、不改驱动实现，仅核对源码/现有证据/微软CE5官方接口文档；详见scripts/HOTPLUG_AUDIT.md。确认严重架构缺口：USB_CLOSE_DEVICE回调内同步join/注销COM/关管道，无完整有界退出；lpAbortTransfer(t,0)官方语义为同步等待取消，外层轮询截止不能限制该调用；超时保留引用无退休回收机制、传输worker退出后缺代内恢复、PowerUp/Down为空。日志DETACH wait0在cleanup前，JOIN wait0在该端口DeactivateDevice/ClosePipe前，不能证明完整清理和回调返回；纠正此前“清理正常”的过强表述。

最新287355第二RX_JOIN后无新日志，可能清理后段阻塞，但也可能未记录的正常返回，不能直接认定死锁。早期连续重枚举曾成功，模块boot_id变化证明存在真实模块重启，原因仍可为电源/固件/看门狗/USB交互，不能把所有问题推给供电或全部认定驱动造成。633在DETACH前发生，事件丢失与后续无法枚举须分开。建议优先补生命周期边界证据和受CE句柄寿命约束的清理/恢复设计，而不是继续以整机重启掩盖；当前只是诊断，无新修复验证。

## 2026-10-11：授权软重启后D35实机推进到AT，定位事件丢失并安装D36

用户“可以”明确授权一次WM6软重启，run_softreset进程2331890758发出，未清数据或重启PC。首轮WMDC重连30秒超时，第二轮进程3418823510启动、tick56785；期间WMDC再次丢失，收集期限内仅不完整标记，没有误判成功。用户手动恢复后新增只读collect_saved_result，按原开始56785核验取回86996完整结束、elapsed30211、COM5 unavailable55且未RasDial，reset请求文件亦取回。Windows只读检查曾见WcesComm/RapiMgr停止，稍后自行Running；没有改服务或再次重启。随后实机245300 ATTACH，250755/250876 COM1/5激活，Active2。

新保存连接测试273324 private35确证，RasDial0，283920完成633/connected0/elapsed10596，无PPP。此前不支持的timeouts50/1/50/5/500、mask1f9、purge0c均成功，data/access0双Open成功；已出现AT三字节写和六字节响应读取，不记录响应内容或称已拨出数据号。最终279123写AT、279142六字节push先于279178再次设置同mask，D35源码无条件eventPending=0清掉历史RXCHAR；279231等待后不再读取，283071 mask0/双Close，287253才detach。结合时序/源码定位具体丢事件，不归因所有633为该问题。聚焦证据diag35/dial-active633、driver-event-race，完整累计日志只本地。

据微软官方serial sample waitmask.c保持HistoryMask&newMask的语义，D36生产helper仅保留仍订阅的待事件，过滤移除项、推进取消epoch并唤醒；不因未读FIFO凭空重造已消费事件。15所有权(408830)+33兼容/事件(411076)零失败，7新增测试覆盖保留/实际wait交付/不重放/过滤/zero/无效原子拒绝/同mask取消。隔离假端口不是USB并发全覆盖。独立D36安装412364 result0，仅既有精确匹配更换，不强卸载/不额外重启，private36身份必要。哈希和主资料见scripts/DIAG36.md。安装后430651..460871尝试COM5 absent55未RasDial；用户再次拔插等30秒，535531..565803 elapsed30272仍55。Active0、匹配D36且无新ATTACH，累计日志止287355；没有private36实机成功证据，不称修复已使633消失。证据diag36/dial-before-replug、dial-after-replug、state-summary。按要求记录推送，需要另一次软重启许可才继续恢复实机验证。

## 2026-10-11：D35补实测超时、清空及事件订阅，41项隔离测试通过并安装

依据D34R2实测参数50/1/50/5/500、mask1f9、purge0c，新增COM_DIAG35条件分支，保留D34每Open上下文。读总超时按请求长度计算，支持字节间隔/立即/首字节模式及0/0取消总截止，写总超时亦计算乘数；配置有界且拒绝预算溢出。RXCLEAR清主机FIFO，TXCLEAR增加代次并在实际USB提交锁内复核，不能撤回已提交USB数据；TXABORT仍整请求原子拒绝。BREAK/ERR按有效CDC通知映射、累计ClearCommError，CTS订阅仅被动接受，不伪造未知CTS状态或边沿，硬件CTS流控仍拒绝；TXEMPTY/RXFLAG不支持，DCB兼容范围未改变。细节及主资料见scripts/DIAG35.md，不宣称完整串口兼容。

Joe32位RAPI部署前，原生生产helper所有权15/15(tick9635798)、兼容26/26(9638099)零失败。后者隔离假端口验证实际读计时/超时预算/清空/事件，未访问USB/AT/SIM/注册表；通知正错误为人工有效包，不是真实线路错误，尚未覆盖实际在途USB清空并发或PPP。D35安装9639413 result0 next_attach_only1，仅既有精确19d2:1432匹配DLL改为ME3630ComV18D35.dll，不重启/强卸载，不改LeadCore/WMDC或用户保存条目。DLL哈希67678A0D7757B4105A950B43E9BBABDA302933CC9ECE5BD35283A8CFFD02613B，private身份35且版本仍18。旧D33分支构建亦通过。

首次采集遇临时IOException，runner改为原65秒期限内重取日志，不因采集失败再启动进程。新完整尝试9851236..9881395、elapsed30159，唯一正确电信条目保持，COM5不可用55，未调用RasDial；只读Active端口0且精确匹配D35。不能据安装称已激活或解决633，尚无PPP/互联网成功。证据diag35/open-unit、compat-unit、install、dial-port-unavailable，完整清单仍本地。用户随后确认已再次拔插并等30秒，复测9970118..10000290 elapsed30172仍error55且未调用RasDial；累计日志仍止于1719938双线程退出，没有新ATTACH。聚焦dial-after-replug和state-summary保存。恢复实机验证需要WM6重新枚举模块；未擅自重启设备，下一步请求一次软重启许可。

## 2026-10-11：D33实机定位零访问辅助打开，D34修复后推进到配置接口

用户同意后软重启进程2343937746。首次保存连接测试48172在启动窗口等30秒仍无端口，保留失败；第二轮109053确认V18并实际拨号633。D33日志109333正常数据打开、109364 access0且opened1，被原独占检查拒绝，109381数据Close；这是实际触发点之一，不是全部633根因。独立每Open上下文实现于me3630_open.inc，数据仍独占，元数据不占数据所有权，关闭元数据不清事件或释放数据owner；禁止其读写与变更控制。生产helper原生15/15单元三轮316439、364660、452564通过，非USB/并发脱离全覆盖。

D34安装318773成功；自然重枚举实际激活，首测392098 private34，数据和access0均ok，推进到Get/SetCommState/Timeouts/Mask/Purge调用，仍633。新构建补Open错误码跨日志保留时，复制同名已加载DLL失败device32，没有强制卸载；通过RAPI回取实际首版并保留deployed-first.dll/hash。改用独立D34R2名安装454879成功，private3402确认后499066拨号，512436完成633，无PPP。记录实测初始DCB19200 flags2015拒绝、fallback115200 flags1011成功；Timeouts50/1/50/5/500、mask1f9、purge0c均不支持(error50)。双Close正确，之后519090才DETACH，不能把配置失败都归因于掉线。

三轮独立测试、D34/D34R2/原V18分支构建通过；未放宽未知接口，未再未经授权重启，未改变保存连接/LeadCore/WMDC。证据diag33/driver-open-block/dial-633，diag34/dial-first/open-unit/实际DLL，diag34R2/dial/driver-parameters/open-unit/install。细节、哈希及下一步见scripts/DIAG34.md。下步按实测参数补超时、双FIFO清空和事件订阅兼容，尚不能称633已解决或联网成功。

## 2026-10-11：TAPI辨识成功，安装只增强日志的V18 D33变体

继续定位633，不以猜测占用/缺某IOCTL直接修改成功语义。ras_saved inspect增加限定ME3630名称的lineInitializeEx/lineGetDevCaps/lineGetDevConfig/lineOpen MONITOR/lineGetID portname，最后关闭监视并shutdown；不拨号、不修改配置、不读凭据。tick329898实测TAPI总返回9、自己的line8 ME3630 Test COM5，media10/DATAMODEM、features8、caps1e0；config684、monitor open0；portname查询80000048为LINEERR_OPERATIONFAILED。保存连接仍唯一正确。不能用监视打开成功证明实际COM5健康，也不能从portname失败单独解释633。

新增条件编译COM_DIAG33，只增加Open进入/Close/IOCTL进入与正常返回日志；维持V18接口限制，不伪造CTS或把未知请求返回成功。独立ME3630ComV18D33.dll，版本仍18，需D33日志及文件哈希确认诊断身份。Joe32位构建/安装443534 result0，只替换原精确19d2:1432匹配项DLL，保留原V18文件、Unimodem/IClass及保存连接。实际Active端口0且匹配D33，无激活/硬件测试成功证据，不擅自再软重启。新增runner语法通过，D33哈希和撤回方式见scripts/DIAG33.md；TAPI聚焦证据ras-saved/inspect.txt，安装证据diag33/install.txt，完整清单仅本地。

## 2026-10-11：再次授权软重启后，真正进入RAS拨号并返回633

用户同意第二次WM6软重启。run_softreset在Joe32位RAPI启动2899924650；启动期间首次RAPI Connect30秒超时，第二次成功，立即运行保存连接测试，无额外Host复位。新tick66656，COM5私有版本18确认；首次RasDial错误87且handle0，未建立会话。查微软CE5 RasDial官方文档后确认程序错误：CE仅支持notifierType FFFFFFFF+窗口句柄，不支持桌面RASDIALFUNC函数指针。ras_saved及旧ras_attempt均改为隐藏窗口、正确类型及消息泵，未改保存连接。

修正后实际tick119783运行，唯一“ME3630 电信”COM5/*99***2#/PPP/IP匹配、port18，RasDial返回0及handle00f90060；随后state0到8193/disconnected、state_error633、窗口error633。本次RasHangUp仅本次handle，error0；130491完成elapsed10708 connected0。这是系统接受拨号并在端口阶段失败，不是PPP建立、不是数据号已送达模块或联网成功。633为端口不可用于拨出/已占用类错误，不能据码断言其他应用占用。驱动129264有新的STREAM_OPEN，但采样中无后续串口IOCTL/read/write，具体失败点待Unimodem兼容性诊断；V18不记录Close，不据缺失日志推断泄漏。

证据ras-saved/dial-reset-api87.txt、dial-reset-633.txt、driver-dial633.txt；代码及聚焦证据及时推送。旧ras_attempt HWND修正仅构建，保存连接路线修正实机验证；不声称已解决驱动兼容性。API主来源：https://learn.microsoft.com/en-us/previous-versions/windows/embedded/aa450834(v=msdn.10)；633说明参阅微软Routing and Remote Access error codes及本地raserror.h。

## 2026-10-11：用户再次拔插后有界复测，仍无新USB枚举

用户回复“好了”后直接在Joe32位RAPI执行run_rassaved -Dial。新开始630128，唯一正确ME3630电信条目仍在，号码/COM5/PPP/IP不变。等待重开30秒后660368完成elapsed30240，PORT_UNAVAILABLE error55；仍未调用RasDial。只读Active_ME3630_PORT_BLOCKS=0，HCD1及19d2:1432匹配到ME3630ComV18.dll均在，累计驱动日志最后169078 DETACH、双RX/notify join0，无本次ATTACH。聚焦新失败保存为ras-saved/dial-after-replug.txt；完整注册表仅本地。没有擅自再软重启或改LeadCore/WMDC。下一步需用户授权再次软重启；恢复后应尽快触发已准备的保存连接测试，避免上一轮先行多项诊断占用接入窗口。

## 2026-10-11：核验用户向导保存的ME3630连接，串口缺失阻止本次拨号

用户反馈WM6已提示新调制解调器，向导可见COM1 Hayes与COM5 Test，并按指导保存“ME3630 电信”，尚未连接。不能把先前RasEnumDevices未找到精确名字直接推断为整个TAPI未识别。新增ras_saved.c/run_rassaved.ps1，通过RasEnumEntries和RasGetEntryProperties只检查ME3630名称条目，不读密码，不改变/删除条目。实际tick483355读到该连接：type modem、device ME3630 Test COM5、phone *99***2#、framing1(PPP)、protocols4(IP)、options00400208；系统保存的devconfig长度684，区别于原注册25字节，不假定同一种格式。枚举返回count10、bytes480，不据此声称10个真实连接。

单次拨号入口需该绑定/号码/PPP/IP精确匹配且唯一，并在调用RasDial前通过COM5私有版本18核验，防止误用LeadCore复用端口。首次tick500222、第二次520709均在该前置因COM5不可用error55退出；后者按用户要求自动重开等待30秒，550855完成elapsed30146，仍无设备。RasDial未调用，没有拨号失败码/PPP地址可供判断。驱动累计日志最新仍止于169078 DETACH，双RX/notify join0，无之后新ATTACH。保存连接已核验正确，当前阻塞仍是硬件未重新枚举，不要求用户重新建连接。此次未追加软重启。

实现提供单次有界异步拨号/状态/PPP投影检测，只挂断自己的会话，不删除用户连接；当前这部分因无端口尚未实测。循环45秒/前端65秒不是对单个OS调用的硬超时。证据evidence/2026-10-11-ras-saved/inspect.txt、dial-first.txt、dial.txt。主API资料：https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms897105(v=msdn.10) 与SDK ras.h；按用户要求记录并推送。

## 2026-10-11：用户授权软重启，USB枚举恢复，但尚未拨号

用户同意后新增soft_reset.c/run_softreset.ps1，在Joe32位RAPI发送一次SetSystemPowerState(NULL, POWER_STATE_RESET, POWER_FORCE)。先刷新注册表和请求日志，不设清除/冷启动标志、不恢复出厂、不复位Windows电脑。依据SDK pm.h及微软CE5文档 https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms920754(v=msdn.10)；远程进程201911462启动成功。重连早期Active暂为0，随后新设备tick72764 ATTACH，78236 COM1、78362 COM5、78379 V18 READY；新启动计时及WMDC重连证实软重启完成。再次掉线93341后101079重新ATTACH、106639 READY，已证明此窗口内驱动重新接入，而非持续稳定。

只读网络probe start82033，在V18句柄上10项全部完成，2248ms；CPIN READY、CFUN1、现有ctnet不变，但CSQ99/99、CREG/CEREG0/2、CGATT0，尚未注册。RAS仍仅既有Cellular Line。限定ME3630名称的RAS测试105798启动，3轮均未枚举出自己的modem，151442结束connected0、45644ms；没有调用RasDial，没有更改默认路由，也不借用LeadCore。下一步应核验Unimodem注册持久性、TAPI发现及实际串口接口需求，同时等待网络注册，不能将恢复AT等同于上网。证据evidence/2026-10-11-soft-reset，完整Active/USB清单不发布。

## 2026-10-11：按有效电信卡推进首次联网，不以长期稳定性为前置

实机RAS程序tick4125692完成3次枚举等待、45352ms，自己的ME3630 Test COM5均未出现，RAS_ATTEMPT_COMPLETE connected0。没有到达创建条目/RasDial调用，不声称已经发起数据会话，更不声称上网。未选既有蜂窝设备作为替代；保留该失败证据。两个前端均改为75秒内新开始标记+完成标记确认，已编译Native并通过脚本语法，尚需恢复USB Host枚举才能继续。所有修改与聚焦失败/成功查询均按用户要求及时推送。

用户明确暂不排查稳定性，断线后重试，并确认有效中国电信SIM。新增只读network_probe、受限Unimodem注册和实际RAS拨号重试程序，全部在Joe32位RAPI执行/构建，不替换V18或其他厂商驱动。首probe tick3688802在实例18上完整读取10项，2456ms完成：CFUN1 CPIN READY；CSQ99/99、COPS0、CREG/CEREG0/2、CGATT0；现有CID1 IPV4V6/ctnet、CID2 IP/ctnet均inactive。不改APN/RF/SIM/IMEI；未注册网络不能当成上网成功。

CE没有桌面rasapi32.dll（初测126），改coredll/ppp导出后RasEnumDevices成功，但仅Cellular Line，严禁误当ME3630拨号。限定自己的Drivers/ME3630ComV18/Port2注册IClass和Unimodem/Tsp/DeviceType1/FriendlyName ME3630 Test COM5/25字节DevConfig，仅将用户vendor格式baud改115200；tick3829231及补IClass后的3975231均result0，不改LeadCore/WMDC注册项。注册成功不是TAPI枚举成功，要等下一次ActivateDevice。

最新驱动日志止于detach3720349且双RX/notify join0；后续Active无ME3630、无新ATTACH，OHCI/匹配仍在。用户按请求重新拔插后probe3995093仍generations0/all_queries0/elapsed60276，软件等待不造假设备。一次早期probe取到未完成片段，保留失败；runner改为75秒内检查新start与COMPLETE，避免固定sleep误判。RAS尝试仅选精确自己的modem名，3x15秒枚举/60秒循环范围，若找到再创建唯一临时PPP/IP条目、*99***2#、观察IP投影，最后只挂断/删除本次条目；没有互联网成功证据。详细语义、主来源和后续条件见scripts/NETWORK_FIRST_LINK.md，聚焦证据2026-10-11-network-probe、unimodem-register、ras-attempt；完整Active/累计driver-local留本地。恢复Host枚举可能需WM6软重启，不能擅自结束设备应用/同步。

## 2026-10-11：V18 部分线路事件，模拟隔离21项通过

第二轮新进程tick3065370双句柄确认18、AT/ATI4/4、162项零失败。两端口line mask/CTS原子拒绝/取消均通过，等待225/219ms、取消调用17/16ms、mask0；notification accepted11/10 rejected0 knownE0 status00 raw00 error0，累计日志同一行多tick0。一个掉线中断失败、一个完整通过窗口，不称连续通过或长期稳定；真实正线路边沿未出现。源码/模拟21项/首轮失败已及时推送4a1eaab，追加第二轮聚焦脱敏证据和记录；保持原接线供电，无拨号/RF/APN/SIM改动。

首轮实机tick2982819双句柄确认18、AT/ATI4/4、162项FAILURES3 COMPLETE，不称完整通过。COM1事件mask/取消六项通过（wait227ms/cancel17ms、mask0）；COM5最后线路检查时RX_EXIT2993666 error1167，detach2993725，后续CTS拒绝原子性与取消等3项失败。双通知/RX join0。已保存driver-first/test-first，不在测试内重开坏句柄，进行第二次有界独立复测；DLL哈希/语义见COM_V18.md。旧V6/8/10/14/15/16/17/18源码分支和全部PowerShell语法通过。

按用户继续工作，在V17基础上独立V18加入DSR/DCD状态变化事件、RI已知低到高事件，不支持CTS/完整modem查询/流控/PPP。生产通知更新函数在同一端口锁内更新baseline/accepted/tick和pending，第一份状态仅建立基准，坏/重复报文不触发，mask过滤后合并且保留RXCHAR。SetMask代数取消沿用已有语义；通知线程失败清known并唤醒，要求线路事件的等待返回错误，纯RX等待独立；detach仍正常失败，不以未知状态伪造边沿。ring上升沿是明确的受限实现，不声称通用Unimodem兼容。API语义核对微软WaitCommEvent，链接见scripts/COM_V18.md。

line_unit直接复用生产更新/等待函数，通过Joe实际32位RAPI运行模拟通知，无USB/AT：18项初版通过后扩展覆盖，最终tick2946472为21/21，包括等待线程被样本唤醒、RX事件保留、重复拒绝、取消、通知失败隔离、disconnect与引用平衡；仅软件验证，不注入实机驱动。build -Version18编译成功，deploy -Version18安装dll_load1 installed1 error0。新增每端口6项实机mask/取消验证，预期162项，正在等待新实例回归；真实正线路变化/模块重启原因仍未解决。

## 2026-10-11：V17 严格解析、部分线路状态与日志串行化

最后一次有界复测tick2643684：双句柄实例17、AT/ATI4/4、150项零失败；两snapshot accepted11 rejected0 knownE0 status00 raw00 error0，累计66条解析且同一行多tick为0。首次和第三次完整通过、中间第二次47项掉线级联失败，并非连续通过或长期稳定。全部三次focused证据保留并脱敏发布。源码/首轮/解析器已及时推送2e87b0f，本次追加失败与第三轮证据及记录；模块重启、物理线路变化、标准线路事件/流控/PPP仍未解决。

第二轮tick2557919为AT/ATI4/4、150项FAILURES47 COMPLETE，不算通过。COM1在duplex开始时RX_EXIT2559397、detach2559571，后续同句柄1609导致级联失败；COM5在新枚举V17实例上完成并通过。已保留test-second/driver-second，不重开失败句柄掩盖异常；进行最后一次有界复测以核验完整连接窗口内的功能，而非无限重试。

首轮累计日志24条解析、同一行多tick计数0（仅有界观察，不是日志全部竞态证明）。测试完成后MI1在tick2484207报告USB error257，tick2484280 detach，双通知和双接收线程join0；异常原样保留，根因未确定，模块重启未解决。

首轮实机tick2473910两端口实例17，AT/ATI4/4、150项零失败；两份snapshot均knownE0 status00 raw00 accepted11 rejected0 error0，短输出/非零输入拒绝87，标准调制解调器查询拒绝50符合预期。首次日志/测试在第二次启动前保存为driver-first/test-first。真实正线路变化尚未出现，下一轮复测仍为只读状态与AT通路，不拨号。

用户继续开发。V17保留V16，新增实际10字节通知的严格解析器：A1/20、wValue0、接口匹配、wLength2、保留位校验；坏报文不覆盖原解析输出。位0/1/3映射RLSD/DSR/RING，CTS明确未知，不实现完整GetCommModemStatus、不新增非RXCHAR事件或PPP。私有IOCTL00223ff8输出8个DWORD/32字节（size/known/status/raw/accepted/rejected/tick/error），端口锁保护，未收到通知known0；有效后knownE0，拒绝计数不覆盖上次状态，线程退出清known。known仅表示最后通知能解释哪些位，不表示线路当前健康。

同一DLL进程内串行化日志open/seek/stamp/message/close，DllMain初始化/释放锁，避免V16多线程交错；不保证跨进程日志互斥。notify_unit直接复用生产解析器，Joe实际32位RAPI运行tick2422104，23/23通过、无USB/串口操作；包含合成正线路值和异常报文不覆盖输出，不是物理线路验证。build_com -Version17编译、deploy_com -Version17安装成功（dll_load1 installed1 error0），V6/8/10/14/15/16/17源分支与全部PowerShell语法通过。硬件测试增加每端口6项，共150项，正在等待新进程实测结果。源码/语义/来源见scripts/COM_V17.md；focused证据evidence/2026-10-11-com-v17与2026-10-11-notify-unit，raw ATI及完整快照留本地。

## 2026-10-11：V16 中断端点只读采集，首轮138项通过

第二轮独立新进程实机复测也完成：两句柄INSTANCE_VERSION16、AT/ATI4/4、138项零失败，保留首轮driver-first/test-first及第二轮driver/test。两个完整短窗口并不证明持续稳定，模块重启仍在发生。按用户授权发布本轮源码、自编诊断二进制和脱敏聚焦证据，不发布完整初始快照/raw ATI。

按用户“继续工作”通过Joe实际32位PowerShell/RAPI检查：初始没有ME3630活动串口，OHCI和V15匹配项存在。保持接线等待，未重启Host或WM6。实现V16独立版本，保留V15：描述符只选择MI1/MI2中断IN端点，每口独立64字节驱动缓冲和线程，30秒空闲期限、停止事件及detach join；失效线程保留引用/管道，避免释放待决USB缓冲。仅日志捕获，不解析为串口状态、不新增拨号/射频/APN/SIM/IMEI操作。来源核对Linux option.c的A1/20通知及CTS假设，不复制其代码或伪造CTS。

build_com.ps1 -Version16编译成功，deploy_com.ps1 -Version16安装dll_load1 installed1 error0，随后自动枚举。-Version16 -Test的新进程tick2076812两句柄核验实例16，AT/ATI4/4、138项零失败。首个枚举MI1/MI2各11个10字节通知，分别a1 20 00 00 01/02 00 02 00 00 00；头部与两字节载荷布局符合预期，当前载荷全零，未验证真实线路变化，不实现GetCommModemStatus/DSR/DCD事件。

tick2110619掉线，两个NOTIFY_EXIT/NOTIFY_JOIN wait0、两个RX_JOIN wait0；随后boot_id变化且uptime11.31，模块重启仍未解决。源码日志共享写入未串行化，已有少量行交错，不能以缺失日志证明没收到报文。生产队列模拟传输回归tick2101754为13/13，非USB中断测试；V6/V8/V10/V14/V15/V16三种C源分支语法与全部PowerShell语法通过。记录/证据：scripts/COM_V16.md、evidence/2026-10-11-com-v16（raw ATI和完整快照不发布）。后续严格报文解析与已知状态掩码/日志串行化/真实拨号分别验证，不能把捕获通知当成PPP完成。正在进行第二轮有界实机复测。

## 2026-10-11：V15 第三轮完整通过并发布

最后一次有界复测tick1537677两端口确认INSTANCE_VERSION15，4/4命令、138项、FAILURES0 COMPLETE；清空、非法请求不破坏队列、取消error995与随后AT恢复再次通过，Purge调用16/14ms。首次与第三次为两个完整通过窗口，中间第二次掉线失败原样保留，不称连续通过/长期稳定。双读取者隔离13项仍通过。旧V6/V8/V10/V14及V15源码分支、unit旧/新分支和全部PowerShell语法检查通过；发布focused日志脱敏筛查后按用户授权提交推送。尚未实现TX清空、线路状态/流控/PPP或解决模块重启。

## 2026-10-11：V15 队列状态、接收清空/取消，首轮138项通过

用户继续开发，新增V15独立版本、保留V14。根据此前实机ABI（ClearCommError001b0030输出16、PurgeComm001b0044输入DWORD），实现DWORD错误+COMSTAT12；cbInQue为锁保护的本地环形队列字节数，cbOutQue为当前同步发送尚未完成的字节数，不是模块/硬件队列。TX计数在方向锁内更新，结束前置零，避免与下一写入竞态。无流控hold，不伪造CTS/DSR/DCD；未解析线路错误，fatal RX仅CE_IOE，不通过清错误假装修复USB。

Purge仅支持RXCLEAR/RXABORT或组合，完整验证flags后才变更队列；TX/未知位error50不清空任何字节，零flagserror87。RXCLEAR清本地queue/pendingRXCHAR，不改变mask代数，不取消USB读取，也不保证丢弃模块/在途数据。RXABORT独立读代数，读者在取得rxIO前快照（配置锁先释放），待决读取error995或已有字节部分返回，不取消USB泵/事件等待。随后新读取可继续。不支持overlapped读写或TX清空。主来源与局限见scripts/COM_V15.md。

编译安装dll_load1 installed1 error0；真实DLL SHA256=b3cc7086a4411f3ebf2ed914f72a670a2706a568140ed21b699963f0da90c6b5。新增status_test.inc每端口18项，目标138。首轮tick1342462两端口INSTANCE_VERSION15：4/4命令、CONTROL_CHECKS138 FAILURES0 COMPLETE。队列6->5->0，非法请求保留5字节，清后200ms空读、待决取消和AT恢复均通过；Purge调用15ms，读寿命198/197ms含发送前延迟/日志，不称纯取消延迟，读返回995和0字节。首轮证据单独保存，原始ATI留本地。

第二轮tick1470145：4/4命令、138项、FAILURES20 COMPLETE。COM1通过；COM5在backlog期间RX_EXIT1477405/DETACH1477458，随后status/purge均disconnect。失败原样保存，不改断言、不自动重开句柄掩盖错误。再次启动一次有界复测，结果另记；模块重启仍未解决。未拨号、未改变SIM/RF/APN/供电/WMDC/LeadCore。

新增独立queue_unit_v15（不覆盖旧unit），单进程模拟输入复用生产ring/pump/read。实机tick1514311 CHECKS13 FAILURES0 COMPLETE：现有10项，加待决主读者和等待rxIO第二读者同被代数取消、下一代读者完整接收三项。纯队列测试不操作USB/串口/驱动注册，不能代替跨上下文或USB到达/purge竞态压力测试。证据evidence/2026-10-11-queue-unit-v15/test.txt。

## 2026-10-11：V14 第三轮完整通过，保存并发接收里程碑

第三轮新tick945045，两端口INSTANCE_VERSION14：4/4命令、102项、FAILURES0 COMPLETE。duplex读332/437ms，写18/114ms；同时事件/读取读182/164ms、事件389/372ms，均完整6字节。第一与第三轮完整通过，第二轮被模块重启打断失败；不称为连续通过/稳定拨号。真实失败证据与所有独立版本源码/二进制一起保存发布，原始ATI/完整注册清单仅本地。V6-V14相关分支语法检查、PowerShell语法及日志标识脱敏检查通过。下一步继续处理模块重启与未实现的线路状态/错误/purge/流控；仍未拨号。

## 2026-10-11：V14 第二轮被重启打断，不计为完整通过

复测tick875248：COMMANDS_PASSED2/4 CONTROL_CHECKS97 FAILURES26 COMPLETE。COM1 tick875256打开实例14，RX_EXIT875659/DETACH875700在控制阶段发生，之后同句柄操作失败；COM5在889007打开新枚举实例14，duplex331ms、同时读168ms均6字节，其事件/未读缓冲检查通过。第二轮test-second-attempt/driver-second-attempt保留，不声称连续通过，也不自动重新打开句柄让失败测试“变绿”。再次启动一次有界复测，结果后续记。

## 2026-10-11：V14 独立接收队列/事件并发首次全部通过

V14第一轮新tick790449，两端口INSTANCE_VERSION14 bytes4：COMMANDS_PASSED4/4 CONTROL_CHECKS102 FAILURES0 COMPLETE。COM1/COM5后台duplex读取336/338ms均完整6字节；同时事件/读取中的读寿命166/169ms、事件372/375ms；未读缓冲排空88/85ms。350ms空读期限仍通过，不随USB接收泵5000ms延长。安装与真实DLL SHA256=dc781f4093983e13e020b2859bd697348c40169e168fb116cfe3270b66fe21c6。证据evidence/2026-10-11-com-v14，首轮test-first-attempt/driver-first-attempt另存，均ATI脱敏。

V13仅改变发送缓冲位置未改善，V14再降低空闲USB取消频率后六项并发失败消失，支持频繁取消是重要诱因，但未确定具体OHCI/USB/固件机制或所有竞态安全。成功区间之外仍有模块重启/DETACH，不宣称长期稳定或可拨号。线路状态/中断解析、purge、流控、PPP仍未实现。本轮随后启动独立复测，新start_tick与实际实例版本再次核验，结果后续补记。没有改变SIM/RF/APN/IMEI/供电或无关驱动。

## 2026-10-11：V13 发送缓冲对照未改善，V14 对照接收取消频率

V13两端口实例13，4/4命令、102项、FAILURES6 COMPLETE；两个未读缓冲回归通过，六项并发问题仍复现。发送缓冲移至驱动持有的PORT内存没有解决观察到的失败。DLL SHA256=f11e0414a74f3e6a7c9eb958d4c88013a84c9957921f0e90a98fd42ba2689894，证据evidence/2026-10-11-com-v13。保留实验，不将它写成映射/缓存修复。

V14只将后台USB读取期限200ms改为5000ms，减少空闲取消/重发频率；应用ReadFile仍由队列独立执行350ms/2000ms等期限。USB循环仍检查stop，清理引用/join策略不变。独立文件/实例/注册路径，保留V13。安装dll_load1 installed1 error0；测试结果另记。没有发送模块复位、拨号或改变SIM/RF/APN/供电策略。

## 2026-10-11：V11/V12 排除部分假设，V13 对照发送缓冲位置

V11 tick313180：两端口实例11、4/4命令、102项、FAILURES6 COMPLETE。生产者/读取线程均当前进程1310334926、优先级251；已记录首次等待为正常超时0x102或成功0，无Wait/ResetEvent失败报告。这只排除已采样的无效事件假设，不证明每次等待均正常。证据evidence/2026-10-11-com-v11。未盲目提升线程优先级。

V12增加原子pumpPhase/pumpCycles/pumpTimeouts，读取前后快照及TX内容是否等于AT+CR，不记录任意payload。4/4命令、102项、FAILURES12 COMPLETE，仍有并发和额外backlog/掉线失败。COM5并发区间cycles6/timeouts4变为26/24，发送tick515394 expected_AT1；说明该期间生产者并非不运行，而是在不断返回接收超时。尚不能证明模块收到USB输出。DLL SHA256=bf30a67563f49fb2aafef067427561e7205de37d352564b597913d8529b95715。证据evidence/2026-10-11-com-v12，未放宽断言。

V13单变量实验：发送staging从调用线程栈移动至PORT中512字节txStage，由已有txIO串行化，接收/事件/USB协议保持。独立文件与注册路径，保留V12；安装dll_load1 installed1 error0。仅测试缓冲地址/上下文相关假设，未确认映射或缓存一致性根因，没有自行调用缺乏核验的权限/缓存API。实机结果另记。

## 2026-10-11：用户重启后 V10 恢复枚举，继续 V11 等待诊断

用户确认手动完成此前建议的操作，按已软重启WM6处理；未由脚本执行复位。Joe实际32位RAPI读取Active确认COM1/COM5均Drivers/ME3630ComV10，Host HCD1仍活动。deploy_com.ps1 -Version10 -Test启动新tick112587，两端口INSTANCE_VERSION10；4/4 AT/ATI、102项、FAILURES11 COMPLETE。六个并发接收失败复现；COM1在RX_EXIT122378/DETACH122424之后又增加五个backlog错误。COM5未读缓冲回归通过，不能将所有失败归因于掉线。

V10 Q日志里生产者/读取指针一致，但待决后台读期间没有Q_PUSH；COM1读113538-117551、COM5读136870-140884各两次返回0。随后wait-then-read在117849/141186收到6字节入队并完整读出，未见已正确入队的回复被ring_pop丢失。仍可能涉及USB输入/调度/取消/调用上下文，尚无根因证明。证据evidence/2026-10-11-com-v10/driver.txt、test.txt，ATI脱敏，原始留本地。

新增独立V11诊断版，保留V10：记录接收泵和调用线程的进程/线程优先级、首次Wait返回值，ResetEvent/Wait失败明确返回错误，避免无效等待静默循环。不调整优先级、不改变USB协议。安装dll_load1 installed1 error0；DLL SHA256=b34417e85a55eaf1044d2df362a2dedc2d32990940c7da63d92f638a24ab2b78。V11实机测试结果后续记录，不以安装成功代替通过。

## 2026-10-11：V9 并发失败、V10 等待枚举、队列隔离实机通过

用户要求继续开发、及时推送。新增 me3630_ring.inc、me3630_queue.inc：每端口后台接收泵与4096字节环形队列，ReadFile和WaitCommEvent分离等待；200ms有界USB接收，不覆盖未读本地数据。增加接收线程引用及拔出join，保留旧版本。构建/部署脚本支持V9/V10、验证实例版本和新start_tick。queue_test.inc复用生产ring，增加6项环形队列及每端口14项并发/未读缓冲回归，目标102项。

V9真实实机tick14794842：COM1/COM5均INSTANCE_VERSION9 bytes4；AT/ATI4/4，CONTROL_CHECKS102 FAILURES6 COMPLETE。两个后台读线程约4002-4036ms返回0字节，写2ms、配置13-14ms；同时事件/读取也失败，事件由测试最终取消。环形队列6项、再次等待期间读出剩余5字节的回归通过。没有放宽断言，也不能归因于后来掉线。后续DETACH14933735，两个RX_JOIN均wait0；不证明所有拆卸竞态安全。证据evidence/2026-10-11-com-v9，原始ATI仅本地，发布test脱敏。

V10仅增加队列入队/出队/读开始结束、线程和临时上下文诊断，并非已确认修复。安装dll_load1 installed1 error0，tick15063535；DLL SHA256=8df2116a79abec61217e447119759d951c41ee5843c629c36bdfa0b667b7f67d。用户拔插并等待30秒后再次运行deploy_com.ps1 -Version10 -Test，40秒内仍无READY，未启动USB测试。probe_active.ps1 -UsbState只读核验：LoadClients6610_5170指向V10、HCD1活动、ME3630活动端口0；随后PullOnly仍仅INSTALL，无ATTACH。USBDeviceInfo历史端点日志不能证明当前枚举。用户确认无驱动窗口，板上只有高频传输时亮的通信灯，不能以不亮判断电源状态。没有重启WM6或主控、没有更改WMDC/LeadCore/SIM/RF/APN/IMEI。

为了在无枚举情况下继续验证，新增queue_unit.c/run_queueunit.ps1：实机单进程复用生产ring/pump/read，只替换USB传输为确定性模拟输入，不注册驱动、不操作USB或串口。Joe实际32位WMDC运行tick15505883，10项全部通过，包括待决后台读收到6字节、事件独立保留、1+5字节完整读取、总超时部分返回、空读0、stop退出读和接收线程。证据evidence/2026-10-11-queue-unit/test.txt。这不能验证Device Manager调用上下文、跨进程映射、USB期限取消或实机并发收发；V9六个失败仍未解决。下一步需要恢复模块枚举后读取V10诊断；若需WM6软复位，应先获得用户同意，避免擅自中断其他程序和WMDC。

## 2026-10-11：V8 RXCHAR 事件子集首次通过

用户要求继续开发。复核先前实机ioctl-abi.txt：GetCommMask001b0024、SetCommMask001b0028、WaitCommEvent001b002c，DWORD缓冲。实现V8独立驱动/注册表/安装器/日志，保留V7回退。新增me3630_events.inc，只支持0/EV_RXCHAR掩码；不支持位明确error50且保留原配置。等待不持配置锁，使用200ms有界USB读片段将数据留在512字节缓冲，事件返回后ReadFile读取；mask generation变化或零掩码成功返回零，拔出/传输失败明确报错，不伪造事件。单等待者限制errorBUSY，等待期间增加端口引用。Close改变掩码代数释放等待。保留V7收发并发及只读ADB诊断。

私有IOCTL00223ffc返回实例版本DWORD8，非标准串口请求；应用对每个实际打开的句柄验证，不符则关闭等待重新枚举，40秒有界。host仍检查新start_tick及READY，解决仅凭旧READY无法证明活动驱动的问题。安装dll_load1 installed1 error0；实际驱动SHA256=a51b4bc2d42e8df87ef71833224a8bd1a3dfcf06fd208af79761cc267f2ef600。

第一轮tick14309370：4/4命令、68/68检查、FAILURES0 COMPLETE。COM1/COM5事件等待212/206ms（含发送前200ms延迟），maskEV_RXCHAR、AT响应均6字节OK；SetCommMask取消调用12/17ms，总等待寿命215/232ms，不将总寿命称为纯取消延迟。首轮test-first-attempt.txt、driver-first-attempt.txt保留。新增12项/端口在原44项上扩展至68。后续仅TestsOnly重编测试，加入重复Wait拒绝和零mask立即返回断言、显式INSTANCE_VERSION日志，复测结果另记；没有重编或覆盖活动DLL。

范围限制：当前等待者持RX锁，是wait-then-read子集；不得并行ReadFile与WaitCommEvent，不读空前次缓冲就再次等待可能阻塞RX至取消/拔出。不是独立后台接收泵或完整串口MDD。仍缺调制解调器状态、CTS/DSR/DCD事件、EV_TXEMPTY、错误状态、purge/队列和PPP；未解析中断端点前不伪造线路状态。持续DETACH/不同boot_id仍出现；未修改供电、SIM、射频、WMDC或LeadCore，也未拨号。源码、局限与微软CE API主来源见scripts/COM_V8.md。

增强第二轮tick14414419：两端口明确记录INSTANCE_VERSION8 bytes4；4/4命令、68/68检查、FAILURES0 COMPLETE。重复等待均被error170拒绝且输出事件0，零掩码等待立即成功返回0。COM1/COM5 RX206/207ms，完整6字节OK；取消调用11/15ms、总等待寿命213/219ms。驱动未重编/覆盖，只更新测试EXE。V6/V7宏编译分支通过/Zs语法检查（无二进制覆盖），部署脚本语法、Git空白及发布日志长标识筛查通过；原始ATI日志仍仅本地。

## 2026-10-11：V7 同句柄并发收发通过，继续等待重枚举后测试

用户重新拔插模块后授权继续开发，短暂不稳定时等待。通过Joe实际32位WMDC读取V6最新日志（driver.txt增加后续枚举记录，先前版本仍在Git历史中），继续出现uptime重置/不同boot_id，尚无证据说明重启问题消失。本轮不改变供电、SIM、射频、APN或网络配置。

V7分离rxIO、txIO、配置io锁；Read/Write在方向锁内短暂取配置快照，USB等待期间释放配置锁；接收缓存只由RX锁保护，发送使用本地缓冲。清理先stop，再按RX->TX->配置顺序取得锁关闭pipe；保留旧上下文引用计数。旧V4-V6编译路径仍保持单I/O锁。当前只实现同步RX/TX并发，不是完整串口MDD、事件、调制解调器状态或PPP。

编译独立ME3630ComV7.dll/安装器/测试程序，安装dll_load1 installed1 error0，保留V6回退。实机DLL SHA256=badae88bbd940314fbf7384c109483732d1d3b4e4582e3ecfb5461ad10947440。新增duplex_test.inc：先启动接收线程并延迟200ms，再查询配置、发送AT，检查写不被读取期限阻塞且接收线程得到OK；每端口6项，连同原32项共44项。

首轮测试tick13845940早于V7首次ATTACH13872517/READY13877909；旧V6实例仍活动。4/4命令通过，44项中6个并发断言失败，配置1815ms、写2008-2010ms。保留test-first-attempt.txt及driver-first-attempt.txt，不能认定为V7锁实现失败。等待新版本生效后第二轮tick13933581：4/4命令、44/44检查、FAILURES0 COMPLETE；COM1配置12ms/写4ms/读219ms，COM5配置12ms/写3ms/读219ms，两个读均6字节OK。对应second-attempt证据保留。

deploy_com.ps1增加V7 READY等待（40秒有界）与新旧start_tick对比，拒绝以旧COMPLETE作新测试成功。READY历史标记不保证当前版本，仍应结合该版本STREAM_OPEN时间核验。增强脚本后的独立复测结果另记。没有请求再次拔插、没有拨号，未声称模块长期稳定或所有拔出并发竞态已验证。源码和限制见scripts/COM_V7.md。

增强脚本后的第三轮tick14020260通过：COMMANDS_PASSED4/4 CONTROL_CHECKS44 FAILURES0 COMPLETE；等待串口可用后打开，COM1配置12ms/写2ms/读216ms，COM5配置12ms/写2ms/读218ms，两个并发读均6字节OK。系统采样为backlightoff flags10010000，不据此推断suspend或模块供电质量；随后tick14066298仍出现DETACH。最新test.txt/driver.txt保留，第二轮单独归档；两次新V7实例上的独立复测均通过。脚本语法及Git空白检查通过，发布日志长数字标识筛查后提交推送。

## 2026-10-11：V6 确认模块 Linux 重启，供电成为优先排查项

通过Joe实际32位PowerShell/WMDC运行90秒V5稳定性应用，保持COM句柄直到I/O失效后关闭重开，仅查询AT/CFUN/CPIN并采样WM6系统电源状态。elapsed=90094ms，generations=4、successful_queries=27、losses=4；旧句柄写失败error1609，随后可重开。系统采样均systemon，不能证明模块供电正常。起初CFUN1/CPIN READY，后来CFUN0/SIM_NOT_INSERTED；用户明确未拔插、取放SIM或改变供电。证据evidence/2026-10-11-stability/monitor.txt、driver-window.txt。完整历史driver-v5.txt留本地；基线CFUN查询kind误标2，派生CFUN状态正确，后续源代码/二进制修正标签但未重跑，不改写原证据。

新增V6固定只读ADB探针，对MI4 ff/42/01进行legacy CNXN并只执行cat /proc/uptime /proc/sys/kernel/random/boot_id，AUTH即停止，不记录banner、不写模块配置。每枚举至多4个样本，传输有界。独立ME3630ComV6.dll保留V5回退，实机安装dll_load=1 installed=1 error0；串口复测4/4命令、32/32控制、FAILURES0。证据evidence/2026-10-11-com-v6/install.txt、driver.txt、test.txt（IMEI脱敏，原始留本地）。部分V4/V5日志字符串为继承实现，并不表示加载旧DLL。

三个ATTACH tick13100417/13125536/13154305分别得到boot_id=46cec7d0-aedc-4688-aad0-17392989ecda、163cc7c6-3f5a-4838-a5d0-68015eb152be、91ac2c87-9d0f-4f56-9c67-9eacf8693dc6，首次uptime分别11.24/11.68/11.14秒。确认模块Linux内核重新启动，不只是WM6串口移除；仍无法区分电压跌落、看门狗、固件或USB交互触发崩溃。summarize_bootinfo.ps1可复核，uptime和boot_id有时分属不同WRTE帧。

用户明确转接板完全由WM6 USB Host供电。厂商mini-PCIE硬件指南V1.5第5.1-5.4节指定模块V_MAIN为3.0-4.0V，恶劣射频环境瞬时峰值可达2.3A，设计峰值能力至少2.5A；这是模块侧设计要求，不是本次实测电流，也不是转接板USB5V输入规格。未测量WM6限流、转接板稳压或电压波形，故只能提高供电假设优先级。下一步先核验转接板型号/供电输入及防反灌，再进行只改变供电的对照；不直接向模块送5V、不并联电源、不改LeadCore/WMDC、不拨号。指南与协议主来源链接、操作范围见scripts/ADB_V6.md。

## 2026-10-11：V5 COM 全部基础检查通过并保存里程碑

最新复测摘要：COMMANDS_PASSED=4/4 CONTROL_CHECKS=32 FAILURES=0 COMPLETE。COM1/COM5均成功打开；Get/SetCommState、DCB回读、Get/SetCommTimeouts、超时回读、DTR/RTS请求、非法配置拒绝、独占打开全部符合已定义子集。两次350ms空读实测均352ms，成功返回0字节。每条AT响应先读1字节、后读5字节；ATI先读1字节、后读111字节，两个通道均得到完整OK、ME3630及ME3630C1AV2.0B01，验证了小应用缓冲区不会截断USB缓存中的剩余响应。

证据：evidence/2026-10-11-com-v5/test.txt及driver.txt，失败/较早尝试另存first-attempt文件；IMEI已脱敏，原始日志被.gitignore排除。实际加载且未被后续测试构建覆盖的V5 DLL SHA256=84a017f5b9264c23d5563fdfecf9c57eea4a74907271697e3e7606d25cdf082c。V3保留，V4保留用于对照。

运行状态限制：测试结束后日志仍有DETACH/ATTACH。缩短5秒启动等待让串口功能能在枚举窗口内被验证，并未解决模块/USB链路重启问题。系统on样本不能排除模块供电、线缆、固件或主控问题；HCD1电源查询error2不给出主控状态。后续优先进行拔出带打开句柄、连续读写/事件队列、设备电源与模块重启原因诊断，再考虑Unimodem/PPP，不进行真实SIM数据拨号。

## 2026-10-11：V5 缩短启动等待，COM 命令与控制接口首次验证

V4第二轮40秒等待仍未赶上可用窗口。电源采样期间系统始终on，flags0x12010000；HCD1的GetDevicePower返回error2，不能据此判断USB主控D0/Dx。保留V4日志及失败证据，不再将SystemIdleTimerReset视为重枚举问题的解决方案。

新增COM_V5独立编译版本，仅把启动等待从20秒改为5秒，使用新的ME3630ComV5.dll和注册表子键，串口功能保持V4子集；安装成功error0，已自动在后续重枚举加载，不要求再次拔插。保留V3/V4文件及注册回退路径。部分继承日志仍显示V4 ACTIVATE，V5 WAIT/V5 READY及DLL名称确定实际版本。

V5第一轮实机：COM1/COM5打开成功；DCB回读、115200/8N1设置、非法波特率拒绝、DTR/RTS请求及软件状态、超时设置回读、非法无界超时拒绝均成功；空读分别354ms/352ms返回成功+0字节。AT/ATI两个接口4/4通过，型号与固件正确。32项控制检查中30项按原断言通过；另外两项第二次打开均被拒绝，但返回55而非预期32，原打开句柄随后AT/ATI正常，说明并非当时设备已移除。证据test-first-attempt.txt和driver-first-attempt.txt，不删除原失败记录。

测试修正为接受WM6的第二次打开error55/32，并继续以原句柄AT/ATI成功确认独占语义。新增V5响应首轮ReadFile只请求1字节、后续读取剩余数据，验证512字节缓存保留。此修正不重编或覆盖实机加载DLL，只更换测试EXE。最新复测结果另记。V5仍有持续重枚举；这不是长时稳定驱动或可用PPP实现。

## 2026-10-11：实测串口 IOCTL ABI，安装 V4 COM 原型

WM6 SDK缺少Pegdser.h，不使用桌面NT串口编码猜测。新增临时 TRC1 流驱动和测试程序，在实机用原生 Get/SetCommState、Get/SetCommTimeouts、EscapeCommFunction等18项API，记录其发给流驱动的IOCTL和缓冲区长度。追踪驱动不操作USB/现有COM，返回ERROR_NOT_SUPPORTED是预期行为，结束后DeregisterDevice。完整证据：evidence/2026-10-11-com-v4/ioctl-abi.txt。测得DCB=28字节，COMMTIMEOUTS=20字节；关键编码及支持范围见scripts/COM_V4.md。

基于V3新增COM_V4条件编译，以独立ME3630ComV4.dll保留旧二进制。最新活动注册表快照中COM1/COM5未占用，映射MI1->COM1、MI2->COM5；不更改LeadCore的COM0/2/3/4/6/8/9或WMDC驱动。实现Get/SetDCB、两种有界超时、DTR/RTS USB请求；读超时返回成功+实际字节（可为0）；512字节接收缓存保存小缓冲区读剩余字节。读写控制仍串行化，无完整事件/调制解调器状态/PPP。未支持设置明确返回ERROR_NOT_SUPPORTED。启动20秒内临时重置空闲计时器，不改持久电源策略。

V4编译成功，实机安装器LoadLibrary/注册成功error0；日志已见COM1和COM5激活成功。DLL SHA256=bc2f57370190877d183e79dc860c1b61bf1311c96089b07dd4fe256e15497e18。V3条件编译也已通过语法检查。第一轮应用测试固定等待25秒后打开：两个COM已被移除，error55，0/4、控制检查0项。记录test-first-attempt.txt和driver-first-attempt.txt，不能将其认定为串口控制实现失败。

V4日志仍出现反复DETACH/ATTACH，即便启动及测试调用SystemIdleTimerReset；因此之前V3的保持唤醒后成功只能作为相关现象，不能判定重枚举由自动休眠导致。已更新应用测试为40秒有界等待端口出现，并采样GetSystemPowerState/GetDevicePower(HCD1:)；不更改电源策略。测试还检查DCB/超时回读、非法配置拒绝、控制线缓存更新、350ms空读超时、独占打开，以及AT/ATI，目标32项控制检查及4项命令全部成功。测试结果另记，不以安装成功代替验证。

## 2026-10-11：V3 应用层双接口 AT/ATI 验证通过

继续开发自编流接口：me3630_probe.c 的 STREAM_V3 条件编译引用 me3630_stream.inc，新增 M36_Init/Open/Read/Write/Close/Deinit 等导出。MI1 映射 M361:，MI2 映射 M362:；暂不用 COM 前缀，避免碰撞原有 LeadCore 串口，也避免将缺少串口 IOCTL 的诊断接口误认为标准串口。通过 ActivateDevice/ClientInfo 传递上下文，独占打开；512 字节本地 USB staging buffer、3 秒有界传输；拔出先发 stop，等待启动线程退出，再注销子设备并关闭 pipe；引用计数保留仍有打开句柄的上下文。拔出时仍打开句柄的行为待实测。

构建使用 VS2008 ARM 编译器 + WM6 SDK，CE5.2/ARM Thumb。通过 Joe 的 32 位 PowerShell/WMDC 安装 ME3630StreamV3.dll，实机 LoadLibrary/注册成功，error=0；保留 V2 DLL，安装失败尝试恢复 V2 注册。用户确认重插并等待。

首轮独立应用测试失败，两个接口 OPEN 返回 error55，COMMANDS_PASSED=0/4。日志显示多次 ATTACH/DETACH，部分在启动等待20秒前断开。首次 M361 激活成功时，MI2 控制请求返回 USB_ERROR5 并断开。不能据此认定驱动ABI不可用或具体硬件故障。

第二轮在测试应用中临时每秒 SystemIdleTimerReset，等待25秒后测试，不修改持久电源策略。tick2710253 开始保持唤醒；tick2710399 ATTACH，tick2730717/2730778 激活 M361/M362；tick2735301/2735406 应用打开成功。两个接口 AT 写3字节、读6字节；ATI 写4字节、读112字节，各 USB error0。均返回 OK、Manufacturer ZTE WELINK、Model ME3630、Revision ME3630C1AV2.0B01；COMMANDS_PASSED=4/4 COMPLETE。实机独立应用的读写缓冲区路径得到验证。

证据：evidence/2026-10-11-stream-v3/install.txt、driver.txt、test-first-attempt.txt、test.txt。原始ATI日志 test-raw.txt留本地且被.gitignore排除，发布版本IMEI脱敏。上传时 DLL SHA256=0ebe068dbcaf18d79839c1129a0d4af14b537baf53bd78aca0464a5b390f3914；后续仅为更新测试EXE重新构建整个项目，DLL代码未变但PE时间戳导致工作目录二进制hash变化，未覆盖实机加载中的DLL。

结论：已经从USB内部原型推进到独立应用可用的双流设备。保持唤醒与成功存在时间相关，尚不足以断定自动休眠导致重枚举；需进一步电源状态与长时间/重复收发、拔出带句柄验证。尚未完成标准 COM IOCTL、DCB、事件、双向缓冲、suspend/resume、Unimodem或PPP，没有拨号。WM6 SDK缺Pegdser.h，不能套用桌面NT串口IOCTL编码。部署测试使用唯一EXE名，避免覆盖设备上锁定的旧映像（曾遇CeMoveFile error5）。

## 2026-10-11：建立提交与推送流程

用户要求及时推送。已确认 origin=https://github.com/lyk82468246/ME3630-S3C6410-WM6.git，分支main。准备提交自编源码、诊断/原型二进制、探索记录和针对性的实机测试证据。原始第三方驱动包、编译中间文件、完整设备目录和注册表快照保留本地，通过.gitignore排除。后续重要验证里程碑后提交并推送；不强制推送。

## 2026-10-11：V2 AT/MODEM 双通道验证成功

用户确认第二次重新插拔并等待。读取V2日志：

- tick2128638：USBDeviceAttach 识别19d2:1432；tick2128897开始20秒启动等待。
- tick2148910开始MI1测试：DTR/RTS控制传输成功，AT+CR写3字节成功，读6字节成功，响应OK；各传输USB错误0。
- tick2149037开始MI2测试：DTR/RTS控制传输成功，AT+CR写3字节成功，读6字节成功，响应OK；各传输USB错误0。
- tick2149160记录PROBE_COMPLETE。tick2169746还有一次DETACH，原因未确定；此单次成功不能证明长期稳定性。

V2同时增加启动等待和控制线初始化，因此不能独立认定V1失败由哪一项导致。已验证基础USB客户端能在WM6上直接控制ME3630两个串口接口；仍需COM流驱动封装、串口IOCTL实现、重复插拔与并发收发验证，之后才是Unimodem/PPP。当前没有进行拨号或联网。

源码：scripts/me3630_probe.c。二进制：build/ME3630ProbeV2.dll。实机日志：evidence/2026-10-11-usb-probe/usb-probe.txt。V2 SHA256 a26804eae689e4dc1658ad9788875cd9bb805522bc221e56d2f296c24575ddc4。

## 2026-10-11：AT 通道首次成功；重插稳定性仍待验证

用户确认重新插拔后，实机日志取得 USBDeviceAttach：VID19d2/PID1432，5个接口，所有 bulk 最大包为64字节。MI1 的 bulk IN=0x82、OUT=0x02；MI2 的 bulk IN=0x84、OUT=0x03。

V1 在 MI1 成功写出 AT+CR（3字节，USB错误0），成功读入6字节（USB错误0），内容为 CR LF OK CR LF。这是自编 CE5.2 USB 客户端在该 WM6 上实际完成 AT 通讯的证据。MI2 读返回 USB_DEVICE_NOT_RESPONDING_ERROR=5。日志随后出现两次 DETACH/ATTACH，后续 bulk 写超时并取消（USB_CANCELED_ERROR=257）。这些重枚举的具体原因未确定，不能据此判定电源故障或数据拨号可用。

已经构建并安装 V2（设备 Windows/ME3630ProbeV2.dll），保留V1文件。V2 在枚举后等待20秒再操作，尝试 CDC 风格 SET_CONTROL_LINE_STATE(DTR+RTS，0x21/0x22，wIndex=接口编号)，记录 tick 时间；标准控制请求如失败仍记录结果后测试 bulk。原型客户端ID和 VID/PID 匹配保持相同，安装器先注销V1，V2安装失败则尝试恢复V1。

V2 安装器实机 LoadLibrary 和 RegisterClientSettings 均成功。已请求再次只拔插模块并保持至少30秒，当前等待触发V2。尚未实现 COM 流接口；PPP和MODEM数据通路尚未验证。

## 2026-10-11：自行编译 CE5.2 USB 原型，安装成功

用户确认 ME3630 已接回 WM6 并要求开始实现。通过 WMDC 保存了新基线到 evidence/2026-10-11-wm6-module-attached。

实现 scripts/me3630_probe.c：只接受 19d2:1432，USBDeviceAttach 中记录活动配置与各接口端点；后台线程仅在接口1、2的 bulk endpoint 上发送 AT+CR，尝试读取响应。每次传输有超时/取消，处理 USB_CLOSE_DEVICE 通知。该版本是直接验证 USB 传输的客户端驱动，还没有 COM 流接口或 PPP 功能。

使用 VS2008 ARM 编译器、WM6 SDK 构建 CE5.2 DLL，导入仅来自 COREDLL；原始构建哈希与 PE 证据在 probe-pe.json。通过 RAPI 复制 DLL 到设备 Windows/ME3630Probe.dll、安装 EXE 到 Temp/ME3630-install.exe。安装程序在设备上 LoadLibrary 成功，通过 USBD 的 RegisterClientDriverID/RegisterClientSettings 注册精确 VID/PID。

实机返回 dll_load=1 installed=1 error=0 COMPLETE，日志 INSTALL ok=1。尚未收到 USBDeviceAttach 或 AT 响应。安装发生在设备已插入之后，已请求用户只重新拔插 ME3630 一次，保留 WMDC 连接，触发重新枚举。

新增 scripts/uninstall_usb_probe.ps1 和对应 EXE 作为明确回退操作：只注销 ME3630Probe 客户端匹配，保留 DLL/日志。此回退未执行。原厂系统驱动和 LeadCore 配置未修改。

## 2026-10-11：ADB 切回 x+n / 1432，已完成

用户将模块接到 Windows 并授权 ADB 切换。使用 C:/Program Files (x86)/搞机助手/adb.exe，设备序列号 3565a9。Windows 和模块 sysfs 初始均确认 PID=0602、functions=diag,serial,usb_mbim,ffs；持久配置 system=8、mode=n。

读取固件 /sbin/usb/compositions/9025、/sbin/usb/s_m_get、/sbin/usb/d_get。脚本依据 /data/configs/usb/system 和 mode 选择组合；x_n 分支是 PID1432、diag,serial,rmnet,ffs。没有修改这些原厂脚本。

通过 ADB 将 /data/configs/usb/system 写为 x，mode 写为 n，执行 sync。原状态为 system 单字节 8、mode 单字节 n；ADB pull 备份因偶发 offline 失败，未声称备份成功。nohup 热切换及 usb_composition_switch 9025 n n 均未改变运行中 PID，随后执行 adb -d reboot。

重启后 Windows 已实际枚举 VID_19D2/PID_1432，ADB 位于 MI_04。其他接口因 Windows 未安装匹配驱动显示 Error，这不影响确认 USB 组合已经切换。模块内部复核结果存放 evidence/2026-10-11-mode-switch/after.txt。

用户现在可拔下模块并改插 WM6 USB Host。接线后的下一步是读取描述符并开发/验证 CE5 串口驱动；当前模式切换不代表 WM6 已具备 ZTE 驱动。需要恢复 MBIM 时，原配置是 system=8、mode=n，并重启模块。

## 2026-10-11：接线完成与自研驱动可行性

用户确认 ME3630 已接到 WM6，怀疑目前为 MBIM 模式；模式仍需读取 USB 描述符或 AT 查询确认。此前“Host 接线尚未确认”已由用户更新，但尚未得到新枚举证据。

检查本机 WM6 SDK：存在 usbdi.h、usbclient.h、usbtypes.h、ceddk.h、devload.h，具备编译 USB Host 客户端/流驱动原型的基础；实机 OHCI、USBD、PPP 栈已经存在，诊断 EXE 已成功编译运行。

技术判断：优先开发特定 ME3630 USB 接口的 bulk 串口驱动，将 AT/MODEM 映射为 COM 端口，再对接 Unimodem/PPP。不能仅凭出现 bulk endpoint 就认定无需厂商初始化；需要实际描述符和传输验证。完整 MBIM 网卡驱动涉及控制协议和 NDIS 适配，工作量与风险明显增加。模式切换/驱动实现/安装尚未在此轮执行。

用户表示厂家从未支持 CE5。当前证据仅证明手头 CE6 DLL 不兼容此 WM6 实机，不能据此确认所有历史厂商包的支持范围。

## 先前的重要探索（根据会话补录）

- 模块 ATI：ZTE WELINK ME3630，固件 ME3630C1AV2.0B01。
- WSL/usbipd 中，19d2:1476 枚举三个 option 串口和 cdc_ether；ttyUSB1/ttyUSB2 可响应 AT/ATI，ZSWITCH=L，ZADSET=e。USB0 当时为 DOWN，未完成数据联网。
- 查询 ZSWITCH 支持 x/X、l/L、8。切换后曾观察 19d2:1432；模式 8 曾产生 wwan0，未完成 QMI 数据拨号。
- 未插 SIM 时 CPIN 返回 SIM not inserted、CSQ=99,99；不能据此判定射频硬件故障。
- 后续用户纠正目标为 S3C6410、Windows Mobile 6.1；不是 S3C2410 的纯 CE5 设备。
- VerySource RAR 是 SMDK6410 WM6.1 BSP 源码，包含 OHCI Host 和 USB Function 驱动；不是完整 WM6.1 构建环境。模拟器镜像是否包含某 DLL不能代替实机检查。
- 用户曾在设备上安装 WinCE6 CAB，但当时报告加载失败。本次实机快照没有相应 DLL/注册项，不能推断此前安装为何消失。

## 2026-10-11：WMDC 实机检查

参考 Positron/scripts/device_gate.ps1 中的 32 位 RAPI 连接和传输实现，通过 Joe 实际 Windows 会话访问 WMDC。未运行 Positron 的部署、清理或应用测试流程。

实机 OS：5.2 build 20782。Windows 目录存在 NDIS、NDISUIO、PPP、TAPI、AsyncMac、Unimodem、TCPSTK、Winsock。NDIS/TAPI/OHCI 有活动注册项。OHCI 为 HCD1，PhysicalPageSize=0x40000。当前不存在 ZTE USB LoadClients 匹配或 Windows 下 ZTE DLL。未找到 USB8023/RNDISMP 主机 RNDIS 通路。存在 LC6K_Modem 活动驱动。ceconfig.h/USBLog.txt 不存在。

证据：evidence/2026-10-11-wmdc 下的 version.json、windows-files.json、registry.txt、findings.md。注册表采集排除了 Comm/Security。

## 2026-10-11：核验用户提供的 CE6 DLL

原始包：用户 Downloads 的正点原子资料/GOSUNCN ME3630 WinCE6 Driver.zip。

ARM ZTEUSBManager.dll：SHA256 23e4f177b33df8957a9ef6754d113fc2fb6f5f66a86a3ebd869c675727000add。
ARM ZTEUSBCOM.dll：SHA256 8742b34c4eecb9b7de5fd5279005294d7a26bccc4679dc773bb10cfcb32aeeb0。

两个 DLL 都是 ARM Thumb PE32，Windows CE subsystem version=6.0，OS version字段=4.0。OS version 字段或 CAB 的版本范围不足以证明兼容 CE5。Manager 静态导入 COREDLL；COM 静态导入 COREDLL/CEDDK。详见 zte-pe.json。

本机 VS2008 ARM 工具和 WM6 SDK 可用。编译了针对 CE5.2 的小型诊断程序，通过 RAPI 放到设备 Temp 并运行。诊断只调用 LoadLibrary 加载已有系统 DLL、GetProcAddress 查询函数和写诊断文本；未加载厂商 DLL或更改驱动注册表。

运行结果：

- COREDLL #2571、#2572、#2696 返回 NULL，GetLastError=87；其余检查的所需 COREDLL 序号可解析。
- Manager 导入 #2696；COM 导入 #2571、#2572、#2696。当前包存在确切导入依赖缺口，不能直接用于实机。
- 所有所需 CEDDK 导出序号可解析。
- aygshell 的 RegistryNotifyCallback、RegistryCloseNotification 可解析。状态通知运行接口存在；尚未执行实际通知订阅测试。
- Windows 实际当前 USB 枚举没有 VID_19D2。此检查无法判断模块是否接在 WM6 Host 上。

完整证据：dependency-runtime.txt。诊断部署留下设备 Temp/ME3630-dependency-probe.exe 和同名 .txt；程序完成后退出。

当前停止 AT/MODEM 测试的条件：没有已核验兼容 CE5 的 ZTE 驱动、没有已枚举的 ZTE COM 端口，且模块实际 Host 接线尚未确认。不能将 LeadCore 串口当成 ME3630 发送 AT。应先取得 CE5 ARM 包/源码，核验后安装，再连接模块进行串口检查。AT测试不需要有效 SIM。
