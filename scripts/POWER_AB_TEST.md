# 供电 A/B 对照测试（免构建树，仅供实测执行）

日期：2026-11-16。目的：验证"USB 激活后模块反复失联/重启"是否由供电不足引起。
所有结论必须以新 ATTACH/日志证据为准，不得以"看起来稳定"替代。

## 0. 安全边界（必须遵守）

- ME3630 V_MAIN 允许范围 **3.0–4.0V**（典型 3.3/3.8V）。**严禁 5V 直接接入 V_MAIN**。
- 峰值电流可达 **2.3A**（弱信号 burst），系统侧平均 ≥2.0A；外部电源过流能力 ≥3A。
- **严禁两路电源并联**同时给 V_MAIN 供电（例如扩展坞 5V 经板载 LDO 与外部 3.8V 同时接入）。
- A/B 期间保持 SIM、天线、USB 数据线、模块 USB 组合（x+n/19d2:1432）不变，仅改变供电路径。

## 1. 方案 A（首选）：载板直供 3.8V

适用：转接板 V_MAIN 可断开与 WM6 5V 的连接，或载板提供独立 VIN 端子。

器材：可调直流电源（设 3.8V，限流 2.5–3A）或 3.7–3.8V 锂电（带保护板）。

步骤：
1. 断电状态确认转接板 V_MAIN 与 WM6 VBUS 之间无直通（万用表测通断）。
2. 外部 3.8V 接 V_MAIN（注意极性），GND 与 WM6 GND 共地。
3. 上电 → 接 USB 数据线到 WM6 Host 口 → 等 15 秒。
4. 采集证据（见第 3 节命令）。

## 2. 方案 B：带外置供电的 USB Hub（需先确认 Hub 可被枚举）

注意：本机两次实测两只带外供扩展坞均**无任何新 ATTACH**，且 BSP/注册表均无
Hub client（HKLM\Drivers\USB\ClientDrivers 下只有 ME3630Probe 与 LC6K_*）。
方案 B 只在"全设备观察探针证明本机可枚举该 Hub"时才有效。

已备好观察探针（未部署）：`scripts/watch_usb.ps1`（构建 ME3630UsbWatch.dll，
全 USB_NO_INFO 匹配、accept=FALSE，仅记录 `\Temp\ME3630-usb-watch.txt`）。
部署：32 位 PowerShell 下
`powershell -File scripts\watch_usb.ps1`，然后插入待测 Hub，再
`powershell -File scripts\watch_usb.ps1 -PullOnly` 读取日志。
日志出现 `ATTACH ... class=09(HUB)` 才说明 Hub 被枚举；只有
`USBDeviceInfo.txt` 式 bulk EP 流水而无 hub 记录，则本机不支持该 Hub，
直接放弃方案 B，回到方案 A。

## 3. 每次测试的采集命令（32 位 PowerShell）

```powershell
# 1) 活跃驱动/USB 注册表快照（只读）
powershell -File scripts\probe_active.ps1 -UsbState -OutDir evidence\2026-11-16-power-ab\<场景>

# 2) 无线状态只读探针（需 COM5 在线；未注册也会输出 CSQ/CREG 等）
powershell -File scripts\run_networkprobe.ps1 -OutDir evidence\2026-11-16-power-ab\<场景>

# 3) 保存条目 RAS 拨号（仅在 CREG/CEREG=1/5 且 CSQ 有数值后执行）
powershell -File scripts\run_rassaved.ps1 -OutDir evidence\2026-11-16-power-ab\<场景>
```

记录项（人工）：供电电压（模块端实测）、电源限流、AT+CSQ、CREG/CEREG、CGATT、
ZPAS、D40 日志 ATTACH/DETACH/usb_error 时间线、是否出现 boot_id 变化。

## 4. 判定

- 通过：连续 ≥30 分钟无 DETACH/RX_EXIT，network_probe 中 CSQ<99、
  CREG 或 CEREG=1/5、CGATT=1；随后 ras_saved 进入 PPP 握手（不再秒回 619/633）。
- 失败：供电改善后仍反复 DETACH → 重点转驱动清理路径（HOTPLUG_AUDIT 所列
  同步 join/注销/关管道问题），用 D40/LIFE 日志定位。
