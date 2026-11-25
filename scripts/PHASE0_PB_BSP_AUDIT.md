# Phase 0 离线审计：CE 5.0 Platform Builder 与 SMDK6410 BSP

日期：2026-11-25（纯 PC 离线只读分析与构建，未连接或修改 WM6 实机与 ME3630 模块）。

检查范围：
- CE 5.0 Platform Builder：`C:\WINCE500`
- SMDK6410 WM6.1 BSP：`C:\Users\Joe\Downloads\SMDK6410_WM61_PS2ONDPPC_REL_0.02_080731`
- 既有实机快照：`evidence/2026-10-11-wmdc/`

---

## 1. 修正 2026-11-16 关于 Hub 支持与 500mA 限额的结论

核对 `SMDK6410\Src\Drivers\Usb\Hcd\sources`、`s3c6410_ohci.c` 与 `C:\WINCE500\PUBLIC\COMMON\OAK\DRIVERS\USB\HCD\COMMON\cdevice.cpp` 后，得出以下确凿代码事实：

1. **CE 5.x 原生包含外部 Hub 支持，且内置于 `ohcdmdd2.lib` 而非独立 `USBHUB` 客户端**：
   - `s3c6410_ohci2.dll` 静态链接了 `ohcdmdd2.lib` 与 `hcdlib.lib`（`Src\Drivers\Usb\Hcd\sources:29-31`）。
   - `cdevice.cpp:1711-1784` 在 `CHub::AttachDevice` 中直接识别 `bDeviceClass == USB_DEVICE_CLASS_HUB` 并实例化 `new CExternalHub(...)`（同时实现 `EnterOperationalState`、`PowerAllHubPorts`、`ResetAndEnablePort`）。
   - 因此，实机 `HKLM\Drivers\USB\ClientDrivers` 下没有 `USBHUB` 属于 CE 5.x **正常架构设计**，不能据此判定镜像裁掉了外部 Hub 支持。
   - **对 `ME3630UsbWatch` 的重要推论**：`CExternalHub` 走 `DEVICE_CONFIG_STATUS_CREATE_NEW_HUB` 分支，**不会**调用 `USBD` 的客户端 `USBDeviceAttach`。因此 `ME3630UsbWatch` 只会看到下游 `CFunction` 设备，**不会**记录 `class=09(HUB)`。

2. **两只外挂供电扩展坞下游无新 `ATTACH` 的高概率代码根因**：
   - `cdevice.cpp:1624-1651` 在 `CHub::AttachDevice` 选择配置时检查供电能力：
     - 若挂在 Root Hub 下（`m_deviceInfo.lpConfigs == NULL`）：调用 `HcdPdd_CheckConfigPower(port, dwCfgPower, 0)`；
     - 若挂在外部 Hub 下，且该外部 Hub 自身激活配置的 `bmAttributes` **设置了 `SELF_POWERED`（0x40）**：放行高功耗设备（`isOK = TRUE`）；
     - 若外部 Hub 的配置描述符**未设置 `SELF_POWERED`**（许多带外接 5V 供电口的廉价 USB-C 扩展坞仍按总线供电上报）：**只允许 `dwCfgPower <= 100` mA 的下游设备**，否则输出 `!USB warning: cannot attach high-power device to a bus-powered hub` 并置 `DEVICE_CONFIG_STATUS_FAILED`，在 `SetConfiguration` 之前直接拒绝枚举，客户端驱动永远收不到 `ATTACH`。

3. **`HcdPdd_CheckConfigPower` 500mA 限额不是直连“激活后数秒失联”的根因**：
   - `s3c6410_ohci.c:467-473` 返回 `((dwCfgPower + dwTotalPower) > 500) ? FALSE : TRUE`。
   - `cdevice.cpp:1627-1636` 仅在 `AttachDevice` 的 `SetConfiguration` **之前**调用一次，且 `dwCfgPower = MaxPower * 2`（USB 2.0 标准下 500mA 对应 `MaxPower=250`，`500 > 500` 为 `FALSE`，返回 `TRUE`）。
   - 直连历史中 ME3630 已多次通过 `SetConfiguration`、进入 `USBDeviceAttach` 并完成 `V18 READY` 与 AT 收发，证明直连未被 500mA 限额拦截。激活后失联仍首要指向物理供电掉电重启（已有 `boot_id` 变化实证）及驱动传输/清理路径。

---

## 2. CE 5.0 主机 RNDIS（`rndismp.dll` + `usb8023.dll`）离线构建与深度审计

### 2.1 资产与链接结果
`C:\WINCE500\PUBLIC\COMMON\OAK\LIB\ARMV4I\RETAIL\` 完整包含 `rndismp.lib/.def`、`usb8023.lib/.def`、`usbclient.lib` 以及 `ndis.def`、`cxport.def`、`usbd.def`、`ceddk.def`、`coredll.def`；`SDK\LIB\ARMV4I\RETAIL\` 包含 `ntcompat.lib` 与 `corelibc.lib`。按 `PUBLIC\COMMON\CESYSGEN\makefile`（第 1102、1199、2308–2324 行，`/ENTRY:DllEntry`）编写可复现脚本 `scripts/build_rndis_ce5.ps1`，使用 VS2008 ARM `lib.exe`/`link.exe` 零错误链接生成（受微软许可与 `.gitignore` 约束仅保留本地）：

- `build/rndismp.dll`（PE32 ARM Thumb `0x1c2`，subsystem `5.0`，SHA256 `7e5bb8d2e78c5fd5f1db1bf5c0191d719208ddbfc86befe588873a61e9f9e4d3`）
- `build/usb8023.dll`（PE32 ARM Thumb `0x1c2`，subsystem `5.0`，SHA256 `a26925249e117adb1bd50c5102f710f3ae2cbd2b5e1e4a7cefc9b955fd777e70`）

完整 PE 导入/导出清单归档于 `evidence/2026-11-25-phase0-pb-bsp/rndis-pe.json`。

### 2.2 静态导入兼容性评估（对比 WM6 实机基线）
- 目标依赖 DLL（`coredll.dll`、`ndis.dll`、`cxport.dll`、`usbd.dll`）在 `evidence/2026-10-11-wmdc/windows-files.json` 中**全部存在**。
- `NDIS.dll`（37 个符号）、`CXPORT.dll`（`CTEAllocMem`/`CTEFreeMem`）、`USBD.dll`（4 个客户端注册函数）全部**按名称导入**（无高版本序号风险）。
- `COREDLL.dll` 共引用 37 个唯一序号（最高 `#1487`，远低于 CE6 缺失的 `#2571/#2572/#2696`）：
  - 其中 **30 个**已在 `evidence/2026-10-11-wmdc/dependency-runtime.txt` 中实机验证 `present=1`；
  - 其余 **7 个**为 CE 5.x 核心内存与权限 API：`#524 VirtualAlloc`、`#525 VirtualFree`、`#560 VirtualCopy`、`#611 SetProcPermissions`、`#612 GetCurrentPermissions`、`#1486 AllocPhysMem`、`#1487 FreePhysMem`。
- 实机验证仍为必需：已编译只读探测程序 `build/rndis_dep_probe.exe`（源码 `scripts/rndis_dep_probe.c`，脚本 `scripts/run_rndis_dep_probe.ps1`），支持在实机检查全部 80 项依赖及可选 `-LoadTest`（在 `\Temp\` 测试 `LoadLibrary` 与签名策略放行，不写注册表）。

### 2.3 修正 `common.reg` 绑定细节与 `usb8023.dll` 匹配硬约束
1. **注册表修正**：核对 `C:\WINCE500\PUBLIC\COMMON\OAK\FILES\common.reg:2247-2277`，`Comm\USB8023` 与 `Comm\USB80231` 的 `"ImagePath"` 必须为 **`"USB8023.DLL"`**（而非 BSP 文档旧稿第 7 节所写的 `"rndismp.dll"`），由 `usb8023.dll` 导出 `DriverEntry` 并调用 `rndismp.dll` 的 `RndisMInitializeWrapper`。`scripts/install_rndis.c` 与 `scripts/deploy_rndis.ps1` 已按此官方结构实现且完全可逆。
2. **反汇编 `usb8023.lib` 中 `USBDeviceAttach` 与 `ParseUsbDescriptors` 发现的两项硬约束**：
   - `USBDeviceAttach+0x164..0x174` 检查 `lpDriverSettings->dwDeviceClass == 2`（`USB_DEVICE_CLASS_COMMUNICATIONS`），否则直接返回 `FALSE`；
   - `USBDeviceAttach+0x19C..0x1F8` 依次调用 `ParseUsbDescriptors` 在活动配置中严格查找：
     - 通信接口：`bInterfaceClass=0x02`, `bInterfaceSubClass=0x02`, `bInterfaceProtocol=0xFF`
     - 数据接口：`bInterfaceClass=0x0A`, `bInterfaceSubClass=0x00`, `bInterfaceProtocol=0x00`
   - **工程含义**：ME3630 在 `x+r`（RNDIS，PID `0601`）下为多接口复合设备，若其设备类为 `0x00`/`0xEF` 或 RNDIS 控制接口为 Wireless Controller（`0xE0/0x01/0x03`），原版 `usb8023.dll` 会在 `USBDeviceAttach` 入口直接拒绝。届时须先实测抓包确认 ME3630 `x+r` 描述符，再按需编写轻量级 `USB_FUNCS` 适配封装 DLL 桥接至 `usb8023.dll`（不提前盲写）。

---

## 3. 定位 Unimodem `hDevice_r0` 触发 `RAS 619` 的精确代码根因

核对 `C:\WINCE500\PRIVATE\WINCEOS\COMM\TAPI\UNIMODEM\modem.c:700-781` 与 `C:\WINCE500\PUBLIC\COMMON\OAK\DRIVERS\SERIAL\COM_MDD2\mdd.c:2134-2152` 确认：

1. Unimodem 在呼叫进入 `LINECALLSTATE_CONNECTED` 后调用 `WatchForDisconnect(pLineDev)`，通过**零访问权限句柄 `pLineDev->hDevice_r0`（`dwAccess == 0`）**监控载波掉线：
   - `SetCommMask(pLineDev->hDevice_r0, EV_RLSD)`（`IOCTL 0x001b0028`）
   - `WaitCommEvent(pLineDev->hDevice_r0, &Mask, NULL)`（`IOCTL 0x001b002c`）
   - `GetCommModemStatus(pLineDev->hDevice_r0, &ModemStatus)`（`IOCTL 0x001b0034`）
   - 若 `WaitCommEvent` 返回 `FALSE`，函数立即跳出循环并执行 `MdmCommandMode` + `DoDevlineDrop`，直接挂断刚建立的 PPP 连接。
2. 微软官方 `COM_MDD2\mdd.c:2134-2142` 明确允许零访问句柄调用 `IOCTL_SERIAL_GET_WAIT_MASK`、`SET_WAIT_MASK`、`WAIT_ON_MASK`、`GET_MODEMSTATUS`、`GET_PROPERTIES`、`GET_TIMEOUTS` 与电源 IOCTL，并为每个 Open 实例维护独立事件队列（使 AsyncMac 持有的读写句柄与 Unimodem 持有的 `hDevice_r0` 互不冲突）。
3. 核对本项目 `scripts/me3630_open.inc:16-22`：D34–D40 对 `ownsData == FALSE`（`access == 0`）拒绝了 `0x001b0028`、`0x001b002c` 与 `0x001b0034`（返回 `ERROR_ACCESS_DENIED=5`）。这与 D38 首轮 `619` 日志中“数据路径刚交接即出现元数据 Open 拒绝 5、随后双 Close”完全吻合。
4. 若后续进入路线 2B（D41），只需按 `COM_MDD2` 语义为每个 `M36_OPEN` 分离事件掩码/等待状态并放开上述 3 个 IOCTL，即可消除这一挂断缺口。

---

## 4. 本轮离线产出与验证

- `scripts/build_rndis_ce5.ps1`：AST 检查通过，离线链接成功并生成 `evidence/2026-11-25-phase0-pb-bsp/rndis-pe.json`。
- `scripts/rndis_dep_probe.c` → `build/rndis_dep_probe.exe`（SHA256 `e41f7f98ac377777befd963985140856392f0ba8c9c2758a3330084d60db8dfb`）及 `scripts/run_rndis_dep_probe.ps1`（AST 检查通过）。
- `scripts/install_rndis.c` → `build/install_rndis.exe`（SHA256 `1c542e28c3d81c29a6ee48989f4428834fbda9d630ff4dbc713d9ef799991684`）、`build/uninstall_rndis.exe`（SHA256 `b196f73f93a0a339ac262d515cbb53d7c885e90ba25299e8b747f7efac9e5bab`）及 `scripts/deploy_rndis.ps1`（AST 检查通过）。
- `.gitignore` 已排除本地微软链接产物 `/build/rndismp.dll`、`/build/usb8023.dll` 及中间目录 `/build/ce5_rndis_tmp/`。
- **尚未完成的实机前置**：以上二进制均为离线构建与静态验证，尚未在 WM6 实机运行；实际兼容性与联网效果须待 WMDC 恢复并完成 Phase 1 供电验收后实测确认。
