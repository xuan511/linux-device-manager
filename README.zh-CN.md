# Linux 嵌入式设备管理与固件升级框架

这是一个面向 Linux C 系统编程学习与作品集展示的真实工程：`devmgrd`
通过非阻塞串口/PTY 管理设备，`devctl` 通过 Unix Domain Socket 发命令，
`device-sim` 在没有 STM32 实机时提供完整二进制协议、遥测、升级与故障注入。

## 它解决什么问题

它把 MCU 侧常见的 UART、CRC、IAP、Bootloader 经验延伸到 Linux Host：字节流
解析、fd 生命周期、epoll reactor、IPC、单调时钟超时、有界重试、进程退出和
断点续传。核心没有 Web、GUI、数据库或大型第三方框架。

## 架构与能力

```text
devctl ── AF_UNIX IPC ── devmgrd/epoll
                              │
                    session + upgrade 状态机
                              │
                    frame/CRC32/stream parser
                              │
                    termios serial / PTY
                              │
                         device-sim
```

- 显式 little-endian 二进制帧，禁止 `memcpy(struct)` 上线
- 支持半帧、粘包、垃圾、CRC 错误、非法长度和重新同步
- 115200/8N1/raw/nonblocking termios；直接创建 PTY，不依赖 socat
- epoll + timerfd + signalfd；partial read/write、EINTR、EAGAIN
- 请求 sequence、重复/过期响应统计、有界 timeout/retry
- info、ping、health、stats、实时 telemetry
- mmap 文件校验、1024 字节分块升级、verify/activate/reboot
- FW_DATA 幂等、FW_STATUS 断点续传、丢包/损坏/延迟/黑洞故障注入
- Unit、stress、随机输入、三进程 integration、ASan/UBSan、Valgrind
- STM32 Cortex-M3 portable core 交叉编译验证

## 快速运行

```sh
./scripts/run-demo.sh
./scripts/demo-fault-recovery.sh
```

普通 demo 自动完成构建、PTY、daemon、信息查询、遥测、固件升级、版本确认、
SIGTERM 和 socket 清理。故障 demo 在 42% 制造 6 秒响应黑洞，普通重试耗尽后
通过 FW_STATUS 获取设备 offset，再继续传输；测试没有放宽 timeout 来“变绿”。

## 构建与测试

```sh
cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug --output-on-failure

./scripts/run-sanitizers.sh
./scripts/run-valgrind.sh
./scripts/run-coverage.sh
```

CI 实际执行 Ubuntu 24.04 Debug/Release/安装/集成测试、ASan+UBSan，Ubuntu
22.04 Valgrind，以及 `arm-none-eabi-gcc` Cortex-M3 交叉编译。

## 升级 operation 模型

升级属于 daemon，而不是某个客户端 socket。开始请求立即返回 operation ID，CLI
通过多个短连接轮询 state/result/offset/total；退出 CLI 不会取消或遗失设备升级。
每次本地 IPC 有独立 5 秒存活边界，设备命令仍使用 2 秒 deadline、有限重试和
有限 RECOVER，两层生命周期互不冒充。

## 故障注入

```sh
build/debug/device-sim --drop-rate 0.05 --corrupt-rate 0.02 --delay-ms 20 --seed 7
build/debug/device-sim --disconnect-at-percent 42
build/debug/device-sim --fail-verify
```

## 真实限制

- 当前 daemon 只管理一个配置好的 transport，尚未实现多设备/hotplug。
- 固件 mmap/CRC 目前在 operation 启动时由 reactor 完成；16 MiB 上限控制停顿，
  后续应迁移到 worker + eventfd。
- Simulator flash 是内存模型，不能证明真实擦写时序或掉电原子性。
- 尚无固件签名和 anti-rollback；CRC 不是安全认证。
- Host + Simulator：CI 已验证；STM32 portable core：交叉编译已验证；STM32
  硬件集成：尚未验证。

学习顺序建议：先看 [架构](docs/architecture.md)，再看
[协议](docs/protocol.md)、[可靠性](docs/reliability.md)、
[固件升级](docs/firmware-update.md)，最后按
[中文学习指南](docs/learning-guide.zh-CN.md) 删除并重写模块。

