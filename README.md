# ME3630 / S3C6410 / Windows Mobile 6.1

本目录从 2026-10-11 起作为探索记录、诊断脚本和证据的固定工作目录。

目标：让 ME3630 在 S3C6410 的 WM6.1 设备上提供串口控制和蜂窝数据连接。

## 当前结论

- 当前注册为 V10 诊断版，但尚未重新枚举激活，不能声称已通过 USB 测试。V9 的独立接收队列实机测试为 AT/ATI 4/4、102 项中 6 项并发失败；V8 仍是最近全部硬件测试通过的受限版本。另新增不接触 USB 的 WM6 队列隔离测试，10/10 通过，仅验证单进程内线程与队列逻辑。详见 scripts/COM_V9.md、scripts/COM_V10.md。

- 当前实机已更新为 V8：新增EV_RXCHAR事件掩码/等待/取消，私有实例版本查询避免测到旧驱动。两轮实机均4/4命令、68/68检查通过，事件发生后AT响应仍完整可读；复测还验证了重复等待拒绝和零掩码立即返回。当前仅支持“等待事件后读取”的受限流程，尚不支持同时ReadFile与WaitCommEvent、DCD/DSR/CTS状态事件或PPP。详见scripts/COM_V8.md；模块重启仍未解决。

- 当前实机已更新为 V7：将接收、发送、配置锁分离；同一句柄并发读写实测通过，4/4命令、44/44控制/并发检查通过，等待接收期间AT发送约3-4ms。仍无事件、调制解调器状态监控或PPP，不代表供电/重启问题已解决。详见 scripts/COM_V7.md、evidence/2026-10-11-com-v7/test.txt。

- 当前实机已更新为 V6，增加只读 ADB 启动诊断：连续三个枚举窗口出现不同 Linux boot_id，uptime 重新从约11秒起算，确认是模块自身重启，不只是 COM 掉线。串口复测仍为4/4命令、32/32控制检查通过。用户确认模块完全由WM6 USB Host供电，供电需优先排查，但尚未确认根因。下一步先核验转接板的独立供电接法，做仅改变供电的对照测试。详见 scripts/ADB_V6.md 和 evidence/2026-10-11-com-v6/driver.txt；尚未拨号。

- V5已验证串口子集，V6继承：COM1:（MI1/AT）、COM5:（MI2/MODEM），支持基础115200/8N1配置、DCB/超时回读、DTR/RTS请求与有界读写。AT/ATI 4/4、控制检查32/32通过；1字节读取后剩余响应完整保留。反复重枚举尚未解决，事件/调制解调器状态/双向并发/PPP尚未实现，不能当作稳定拨号驱动。源码范围和操作见 scripts/COM_V4.md，证据 evidence/2026-10-11-com-v5/test.txt。

- V3 已在实机提供 M361:（AT）、M362:（MODEM）诊断流接口。独立应用通过 CreateFile/WriteFile/ReadFile 对两个接口执行 AT/ATI，4/4 成功，型号/固件正确，各传输 USB 错误0。不是标准 COM 驱动：尚缺串口控制、事件和电源管理。测试过程中发现反复重枚举；短时重置空闲计时器后成功，原因仍需确认。详见 scripts/STREAM_V3.md 和 evidence/2026-10-11-stream-v3/test.txt。

- 自编 CE5.2 USB V2 原型已在 WM6 成功完成 MI1（AT）、MI2（MODEM）的 AT 收发：两个通道都写入3字节、读入6字节并响应 OK，USB错误0。还未实现 COM 接口和 PPP，长期稳定性未验证。源码和日志位于 scripts/me3630_probe.c、evidence/2026-10-11-usb-probe/usb-probe.txt。

- 实机版本：CE 5.2 build 20782。USB Host 的 s3c6410_ohci2.dll 已加载为 HCD1，OHCI PhysicalPageSize 为 256 KiB。
- PPP、TAPI、Unimodem、AsyncMac、NDIS、TCP/IP 等主要组件存在；主机 RNDIS 的 USB8023/RNDISMP 文件和注册项未找到。
- 设备侧 RNDISFN 已加载用于 WMDC；不能据此判断外部 RNDIS 模块可用。
- 当前镜像没有生效的 ZTE 驱动：Windows 下没有 ZTEUSBCOM/ZTEUSBManager，USB LoadClients 中没有 VID/PID 6610_5170。
- 已有 LeadCore LC6K_Modem 驱动和活动串口，匹配 1ab7:6000。尚无证据说明它可以驱动 ME3630。
- 现有 GOSUNCN WinCE6 ARM DLL 无法直接用于当前 WM6：依赖的 COREDLL 导出序号 #2696（两个 DLL），#2571/#2572（ZTEUSBCOM）在实机中不可解析。
- 状态通知的 RegistryNotifyCallback/RegistryCloseNotification 在 aygshell 中可解析；所有所需 CEDDK 序号也可解析。尚未测试通知 API 的实际注册调用。

## 目录

- `EXPLORATION_LOG.md`：历史与后续探索记录。
- `evidence/2026-10-11-wmdc/`：文件列表、注册表、版本、PE 导入信息及实机 API 检查结果。
- `vendor/wince6/`：从用户提供的 ZIP 解出的原始驱动包，未修改 DLL。
- `scripts/`：只读 RAPI 探测、PE 检查、设备端 API 检查源码及运行脚本。
- `build/`：诊断 EXE/OBJ，不是可安装的 ZTE 驱动。

## 下一步条件

1. 自编原型已经证明基础 AT 通路可行；下一步验证启动、断开和重插稳定性，随后封装 COM/Unimodem。获取厂商 CE5 包或源码仍是备选路径，不再是开展原型开发的前置阻碍。修改 CE6 DLL 的版本字段不能补齐缺失 API。
2. 检查兼容包后再安装到 WM6；确认 DLL 路径和 ME3630 USB 匹配注册项。
3. WM6 的 USB Function/同步口继续连接 Windows/WMDC；ME3630 的 USB 数据口接 WM6 的独立 USB Host 口，并保证模块供电。若设备没有可同时使用的两个口，需要另行确认 USB 接口拓扑。
4. 优先使用厂商串口驱动支持的 x+n / 19d2:1432 模式，重新查询确认实际模式。
5. AT/ATI 串口检查不需要 SIM；真正的数据拨号才需要有效 SIM、网络注册和正确 APN。

如有新探测、配置更改或失败，在 EXPLORATION_LOG.md 写入日期、操作、证据、结论和未解决事项。厂商指南针对 CE5 的说明不代表用户手头 CE6 二进制也兼容 CE5。
