# 探索记录

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
