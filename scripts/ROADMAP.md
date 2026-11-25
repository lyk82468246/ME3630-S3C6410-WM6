# ME3630 / S3C6410 / WM6.1 联网工程路线规划

日期：2026-11-25。目标：在 **S3C6410 / Windows Mobile 6.1（CE 5.2 build 20782）** 上让 **ME3630（ME3630C1AV2.0B01）** 提供稳定的系统级蜂窝上网能力。

## 核心原则

1. **硬件供电稳定优先于所有网络协议调优**：模块在 WM6 Host 直供下已实测发生 Linux 内核重启（`boot_id` 变化、`uptime` 重算）；任何软件路线（RNDIS 或 PPP）都以稳定供电与无线注册为硬前置。
2. **离线开发与硬件验证解耦并行**：PC 端利用 `C:\WINCE500`（CE 5.0 PB）与 `SMDK6410` BSP 完成源码核查、驱动编译和探测工具准备，不阻塞硬件供电改造。
3. **双线收敛、设决策门**：
   - **路线 2A（主攻）**：CE 5.0 主机 RNDIS 网卡（`usb8023.dll` + `rndismp.dll`）+ ME3630 `x+r` 模式，绕开 Unimodem/PPP 复杂栈。
   - **路线 2B（兜底）**：自研 D40/D41 串口驱动 + Unimodem/RAS PPP（`x+n` / `19d2:1432`），借助 CE 5.0 PB 原版 `UNIMODEM` / `COM_MDD2` 源码精准补齐 `hDevice_r0` 事件与载波监控缺口。
4. **安全与可回滚底线**：
   - ME3630 `V_MAIN` 严格限制 **3.0–4.0V**（推荐 3.8V / 限流 2.5–3A）；**严禁 5V 直入**、**严禁双电源并联**；
   - 不修改 ROM 既有 LeadCore（`1ab7:6000`）与 WMDC Function 侧 `rndisfn.dll` 配置；
   - 新驱动必须先过 PE 导入表检查与实机只读依赖/加载验证，再写入可逆注册表项；不盲目覆盖开机关键驱动 `s3c6410_ohci2.dll`。

---

## 分阶段计划与决策门

### Phase 0：CE 5.0 PB & BSP 离线盘点与源码核查（✅ 已完成，2026-11-25）

- 盘点 `C:\WINCE500` ARMv4I 库、源码与 `SMDK6410` BSP，详查见 `scripts/PHASE0_PB_BSP_AUDIT.md`。
- 关键成果：
  1. 修正 2026-11-16 关于 Hub 与 500mA 的推断（CE 5.x 外部 Hub 由 `ohcdmdd2.lib` 内置 `CExternalHub` 处理，且解释了非 `SELF_POWERED` 扩展坞静默拒接 >100mA 下游设备的根因）。
  2. 使用 `scripts/build_rndis_ce5.ps1` 成功离线链接出 CE 5.0 ARMv4I `build/rndismp.dll` 与 `build/usb8023.dll`（受微软许可与 `.gitignore` 约束保留本地），完成 PE 静态导入审计（`evidence/2026-11-25-phase0-pb-bsp/rndis-pe.json`）。
  3. 识别出原版 CE 5.0 `usb8023.dll` 的匹配硬约束（`dwDeviceClass == 2` + `0x02/0x02/0xFF` + `0x0A/0x00/0x00`），并备齐只读依赖/加载探针（`scripts/run_rndis_dep_probe.ps1`）及可逆部署脚本（`scripts/deploy_rndis.ps1`）。
  4. 通过核对 `C:\WINCE500\PRIVATE\WINCEOS\COMM\TAPI\UNIMODEM\modem.c` 与 `COM_MDD2\mdd.c`，定位了 D34–D40 在 `hDevice_r0`（`access=0`）上拒绝 `SetCommMask`/`WaitCommEvent`/`GetCommModemStatus` 导致 `WatchForDisconnect` 立刻挂断（`619`）的精确代码根因。

---

### Phase 1：供电 A/B 验收与基础链路稳定（待实机接回 WMDC）

**前置条件**：恢复 32 位 WMDC/RAPI 会话；将 ME3630 从当前 MBIM（`8+n`）切回基线模式 `x+n`（`19d2:1432`）。

1. **只读软件预检（无配置写入）**：
   - 运行 `scripts/run_rndis_dep_probe.ps1 -LoadTest`（在 `\Temp\` 测试 `rndismp.dll` / `usb8023.dll` 的全部 80 项依赖与加载器/签名放行情况）。
   - 部署 `scripts/watch_usb.ps1` 短时抓取 ME3630 直连完整描述符（记录 `bmAttributes`/`MaxPower` 及接口表），随后 `-Remove` 卸载。
2. **执行供电方案 A**（按 `scripts/POWER_AB_TEST.md`）：
   - 断开载板 5V→LDO 路径（万用表验通断），外部 3.8V / 限流 2.5–3A 直供 `V_MAIN` 并与 WM6 共地，USB 数据直连 WM6 Host。
3. **采集稳定基线**：
   - `scripts/probe_active.ps1 -UsbState` + `scripts/run_networkprobe.ps1` + D40 日志时间线。

**决策门 G1**：
- **通过**：持续在线无 `DETACH`/`RX_EXIT`/`boot_id` 变化，且 `CPIN READY`、`CSQ<99`、`CREG/CEREG∈{1,5}`、`CGATT=1` → 进入 **Phase 2**。
- **供电达标仍反复 `DETACH`**：进入 **Phase 1B**，结合 `HOTPLUG_AUDIT.md` 与 `ME3630UsbWatch` 描述符排查 USB 传输与清理路径。
- **在线但长期 `NO SERVICE`**：优先排查天线/频段/SIM（以 Win11 同卡同天线对照）。

---

### Phase 2：整机上网双线冲刺（在 G1 通过后执行）

#### 路线 2A（主攻）：CE 5.0 主机 RNDIS 网卡
1. 将 ME3630 切至 `x+r`（RNDIS），先采集其真实 USB 配置/接口描述符（`bDeviceClass`、RNDIS 接口是 `0x02/0x02/0xFF` 还是 `0xE0/0x01/0x03`、是否仍带 AT 串口）。
2. 若 Phase 1 的 `rndis_dep_probe -LoadTest` 零缺失且加载成功：
   - 若 ME3630 `x+r` 符合 `usb8023.dll` 原生要求 → 运行 `scripts/deploy_rndis.ps1` 直接部署验证；
   - 若 ME3630 `x+r` 为复合设备（`bDeviceClass=0x00/0xEF`）或使用 Wireless RNDIS（`0xE0/0x01/0x03`）→ 编写轻量级 `USB_FUNCS` 适配封装 DLL（仅将 `dwDeviceClass` 与该接口描述符映射为 `usb8023.dll` 预期形态并透传全部 USB 传输），复用已验证的 `usb8023.dll` + `rndismp.dll`。
3. 核实数据建链触发方式（自动建链或经伴随 AT 口发送 `AT+ZECMCALL=1` 等指令），检查 WM6 网卡 DHCP 获取 IP/默认路由/DNS 及系统级外网连通性。
4. **退出门 G2A**：若 CE 5.2 加载器拒绝签名、NDIS 绑定存在不可解 ABI 差异、或固件 `C1AV2.0B01` 在 `x+r` 下无法拉起 IP 数据通道 → 卸载 RNDIS 并立即切入 **路线 2B**。

#### 路线 2B（兜底）：D40/D41 串口 + Unimodem/RAS PPP
1. 在 G1 稳定链路与无线注册前提下，先直接复测已预装的 D40 + `scripts/run_rassaved.ps1 -Dial`。
2. 针对 Phase 0 已定位的 `UNIMODEM\modem.c:726-778` `hDevice_r0` 缺陷，实现 **D41**（按 `COM_MDD2\mdd.c:2134-2142` 为每个 `M36_OPEN` 维护独立事件掩码/等待队列，并允许 `access=0` 句柄执行 `SetCommMask`/`WaitCommEvent`/`GetCommModemStatus`），在隔离单测全通过后部署实测，直到拿到 PPP IP 与默认路由。

---

### Phase 3：稳定性与热插拔工程化（通网后执行）
- 对照 CE 5.0 PB `USBD` 源码落实 `scripts/HOTPLUG_AUDIT.md` 清理修复（有界退出、安全取消、电源休眠恢复、无须软重启的重枚举）。
- 验证连续拔插、弱信号断网重连与连接管理器联动。
- 固化一键安装/卸载/回滚包，归档实测证据。

---

### Phase 4（可选扩展，置后）：外部 Hub / OHCI 优化
- 仅在整机联网完成后、且确有刚需时评估；若需使用外部 Hub，优先选用在配置描述符中真实声明 `SELF_POWERED`（`bmAttributes & 0x40`）的标准 USB 2.0 有源 Hub，而非盲目替换 `s3c6410_ohci2.dll`。
