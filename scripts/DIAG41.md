# D41：按 CE 5.0 `UNIMODEM` / `COM_MDD2` 源码修复 `hDevice_r0` 事件监控与热插拔契约核查

日期：2026-11-26（离线实现、编译与主机多线程回归验证完成；**尚未部署到 WM6 实机**）。

---

## 1. 修复内容（仅在 `COM_DIAG41` 启用，不破坏 V18/D33–D40 历史构建）

依据 `C:\WINCE500\PRIVATE\WINCEOS\COMM\TAPI\UNIMODEM\modem.c:700-781` 与 `C:\WINCE500\PUBLIC\COMMON\OAK\DRIVERS\SERIAL\COM_MDD2\mdd.c:2134-2152`：

1. **放行零访问句柄（`hDevice_r0`）的载波/事件监控 IOCTL**：
   - `scripts/me3630_open.inc` 在 `COM_DIAG41` 下允许零访问句柄执行 `0x001b0024`（`GetCommMask`）、`0x001b0028`（`SetCommMask`）、`0x001b002c`（`WaitCommEvent`）、`0x001b0034`（`GetCommModemStatus`）、`0x001b0040`（`GetCommTimeouts`）、`0x001b0050`（`GetCommState`）与只读诊断查询；仍对 `SetCommState`/`SetCommTimeouts`/`PurgeComm`/`EscapeCommFunction`/`Read`/`Write` 返回 `ERROR_ACCESS_DENIED`。
2. **每 Open 独立事件状态与引用计数防 UAF**：
   - 每个 `M36_OPEN` 拥有独立的 `eventMask`、`eventPending`、`eventEpoch`、`eventWaiter`、`eventWake` 与 `refs`，并挂入 `PORT.openList`（由 `port->io` 保护）。
   - `AsyncMac`（读写句柄 `hDevice`，订阅 `EV_RXCHAR`）与 `Unimodem WatchForDisconnect`（零访问句柄 `hDevice_r0`，订阅 `EV_RLSD`）可并发调用 `SetCommMask` 与 `WaitCommEvent`，互不覆盖掩码、不返回 `ERROR_BUSY`。
   - `serial_rx_pump` 与 `serial_notify_probe` 通过 `m36_broadcast_events` 按各自订阅掩码扇出事件；`PURGE_RXCLEAR` 仅清除各 Open 的 `EV_RXCHAR` 待决位而不打断 `EV_RLSD` 等待者；关闭单个句柄或物理 `DETACH` 均能安全唤醒对应等待者，且 `refs` 计数防止关闭与唤醒并发时的 UAF。
3. **独立身份与修复 D40 的 `ActivateDevice` DLL 名称遗漏**：
   - `me3630_stream.inc` `worker()` 修复了此前 D40 沿用 `ME3630ComV18D39.dll` 名称激活端口的遗漏；D41 明确使用 `ME3630ComV18D41.dll`、私有诊断版本 `41`、独立命名映射 `ME3630D41Lifecycle` 及日志 `\Temp\ME3630-life-capture-d41.txt`。
   - 仍不伪造未知 CTS，不声称 TXEMPTY/RXFLAG，不改动既有 LeadCore / WMDC / 保存拨号条目。

---

## 2. CE 5.0 `USBD` 热插拔清理契约核查（补充 `HOTPLUG_AUDIT.md`）

核对 `C:\WINCE500\PUBLIC\COMMON\OAK\DRIVERS\USB\USBD\usbddrv.cpp:376-449`、`usbd.cpp:570-594` 及 `CLASS\PRINTER\usbprn.c:578-634` 后确认：

1. **解释了 D37/D38 中 18 次 `lpClosePipe` 返回 `FALSE / error=6`**：`HcdDeviceDetached`（`usbddrv.cpp:396-400`）在调用 `CloseUSBDevice`（`USB_CLOSE_DEVICE` 通知）**之前**就先关闭了设备上的所有管道，因此回调内再次 `lpClosePipe` 返回无效句柄属于 CE 5.0 USBD 的正常时序，并非本项目双重关闭或内存泄漏。
2. **确认 `USB_CLOSE_DEVICE` 返回前的同步清理硬约束**：`HcdDeviceDetached`（`usbddrv.cpp:406-445`）在 `CloseUSBDevice` 返回后立即对客户端 DLL 执行 `FreeLibrary` 并 `delete pDev`。因此必须确保全部 USB 工作线程在 `USB_CLOSE_DEVICE` 返回前退出（不可简单把 USB 清理推迟到回调返回之后）；`RETAIN_STUCK` 若在线程未退出时提前返回回调，仍存在被 `USBD` 卸载/释放 `SDevice` 的理论风险，留待 Phase 3 若实机触发时再做无界/安全卸载改造。

---

## 3. 离线构建与验证证据

- **主机多线程回归（真实生产 C 源码）**：`scripts/run_d41_host_test.ps1`（`test_d41_host.c`，VS2008 `/W3` 零警告）执行 **17/17 通过、0 失败**（覆盖双句柄并发等待、`EV_RXCHAR`/`EV_RLSD` 隔离扇出、`PURGE_RXCLEAR`、`SetCommMask(r0,0)`、并发 `Close` 防 UAF 与 `DETACH` 唤醒），日志见 `evidence/2026-11-26-diag41-offline/host-unit.txt`。
- **主机 RAPI 快照回归**：`scripts/test_rapi_snapshot.ps1` 5/5 通过。
- **ARMv4I 交叉编译**：D33、D40（回归）与 D41 均零错误通过，核心产物 SHA256：
  - `build/ME3630ComV18D41.dll`：`1b813a4c29e46135708d3915f49bdefba8065bd85ab8715864f621fcf80b359d`
  - `build/install_diag41.exe`：`fb029ceed59bc362636b5a8509ecd89af8a55538fa2ae7ede4c08346da187911`
  - `build/open_unit.exe`（D41，19 项检查）：`1c7042888e937615835de8c756d3c8a5300e040c2446955994a48b4e4d48d4a7`
  - `build/compat_unit.exe`（D41，43 项检查）：`2a781106c1ecc0393408f56805b058aded4dcbda96722faaf61b9e40ce8667ac`
  - `build/life_unit_d41.exe`：`c6c5bdf98d6e1f85f75e57acb0543afeae355b0e48b258fb3bd9925eb944771c`
  - `build/life_capture_d41.exe`：`1938c3a73063703d18b27c222df36c3ce62f82071b8a56551ee15661759d82f0`
  - `build/ras_saved.exe`：`54cf1f128b4898d374c531a6cf30bb0fe46eb54ba462ce7167c37e80f497648f`

---

## 4. 实机部署与回滚命令（待 Phase 1 供电稳定且走路线 2B 时使用）

```powershell
# 1) 在 32 位 PowerShell 中先跑实机 19+43+29=91 项隔离单测，全部通过后才切换精确匹配到 D41：
powershell -File scripts\deploy_diag33.ps1 -Version41

# 2) 用 D41 身份强校验执行保存条目 RAS 拨号（需先达到 CREG/CEREG=1/5、CSQ<99、CGATT=1）：
powershell -File scripts\run_rassaved.ps1 -Dial -ExpectedDiagnostic 41 -OutDir evidence\2026-11-26-d41-ras

# 回滚到 D40：
powershell -File scripts\deploy_diag33.ps1 -Version40
```
