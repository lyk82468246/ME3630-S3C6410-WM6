# ME3630 固件网络模式与 WM6 路线

## 结论

ME3630 固件不是只能提供一个“通用 Modem”串口。参考手册定义了 NDIS、RNDIS、ECM 和 MBIM 四种 USB 组合，也定义了模块内部的 PDP/TCP/IP AT 命令。但“切换 USB 组合”不等于 WM6 已经有对应主机协议驱动；主机仍须把 USB 网络功能绑定到 NDIS miniport，或者由应用直接使用模块内部 socket。

对当前固件 `ME3630C1AV2.0B01`，最值得优先验证的不是 MBIM，而是：

1. 保持当前 `x+n`/`19d2:1432` WinCE NDIS 组合，补齐或确认 WM6 主机侧 USB NDIS 驱动；这是手册明确标为 WinCE 的组合。
2. 如果目标只是让自己的应用访问服务器，评估 `AT+ZIPCFG`、`AT+ZIPCALL`、`AT+ZIPOPEN` 等模块内部 TCP/IP 命令。它可绕过 PPP/RAS，但不会给 WM6 的浏览器或系统 TCP/IP 栈创建透明网卡。
3. 不把 `AT+ZSWITCH=8`/MBIM 当作捷径。WM6 当前快照没有 MBIM 主机栈证据，切过去只会增加一个协议栈和驱动实现任务。

## 固件组合

手册第 12.8 节列出下表。PID 是文档中的组合 PID；实际接口描述符仍应以每次枚举为准。

| 设置 | 组合 | 文档描述 | WM6 代价 |
| --- | --- | --- | --- |
| `ZSWITCH=x`, `ZADSET=n` | `19d2:1432/1403` | DIAG、AT、MODEM、NDIS、ADB；Windows XP/7/Vista/WinCE | 需要主机 USB NDIS/miniport；仅有 `NDIS.DLL` 不足 |
| `ZSWITCH=x`, `ZADSET=r` | `0601/1404` | RNDIS 组合 | 需要主机 RNDIS USB miniport；WMDC 使用的 `rndisfn.dll` 是设备功能侧，不能证明主机侧存在 |
| `ZSWITCH=l`, `ZADSET=e` | `1476/1406` | Linux/Android ECM | Linux 已观测到 `cdc_ether`；WM6 未发现 CDC-ECM 主机驱动 |
| `ZSWITCH=8` | `0602/1405` | Win8 MBIM | WM6 未发现 MBIM 主机栈；不作为当前路线 |

## 2026-10-12 恢复 WMDC 后的实机只读结果

证据：`evidence/2026-10-12-restored-wmdc/` 和 `evidence/2026-10-12-network-probe-restored/test.txt`。

- 活动端口为本项目自己的 `COM1`/`COM5`，注册表键为 `Drivers\\ME3630ComV18\\Port1/Port2`；没有把 LeadCore 的 COM 端口当成 ME3630。
- `AT+ZSWITCH?` 返回 `x`，`AT+ZADSET?` 返回 `n`，因此当前确实是 `x+n`/NDIS 组合。
- `AT+ZNVREAD=auto_dial_mode` 和 `AT+ZNVREAD=ecmcall_urc` 均返回 `ERROR`。参考手册将这些 NV 功能限定为较新的 `ME3630J2AV1.0B12`、`ME3630J2ASV1.0B05` 及以上固件；不能把它们套用于当前 `C1AV2.0B01`。
- `AT+ZIPCFG?` 返回空 APN；`AT+ZIPCALL?` 返回 `0`。这说明内部 socket/PS call 接口可查询，但当前没有内部数据会话，也没有为其写入 APN。
- 无线侧仍返回 `+CPIN: READY`、`+CSQ: 99,99`、`+CREG: 0,2`、`+CEREG: 0,2`、`+CGATT: 0`、`+ZPAS: "NO SERVICE"`。因此这次不是“USB 网络模式已失败”，而是模块目前尚未注册到无线网络。
- RAS 枚举仍只有既有 `Cellular Line`；本次探针没有拨号，也没有修改任何拨号条目或 APN。

## 资料依据

- GOSUNCN ME3630 AT Command Reference Guide：
  https://welinkjapan.co.jp/pdf/GOSUNCN%20AT%20Command%20Reference%20Guide%20of%20Module%20Product%20ME3630.pdf
