# 探索记录

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
