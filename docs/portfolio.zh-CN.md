# 作品集与简历表达

## 项目简介

用 C17 实现 Linux 嵌入式设备管理框架：epoll daemon 通过 termios/PTY 与设备通信，
本地 CLI 使用 AF_UNIX IPC，二进制协议支持遥测、状态查询和可恢复固件升级；无需 root
或硬件即可在 CI 中运行完整流程。

## 核心技术（可在掌握后写入）

- epoll/timerfd/signalfd 非阻塞 reactor 与明确 fd ownership
- ring buffer + stream parser，处理任意分片、粘包、CRC 错误与重新同步
- sequence/deadline/retry、双状态机、FW_DATA 幂等与 FW_STATUS resume
- daemon-owned 异步 upgrade operation，IPC 与设备 deadline 分层
- PTY 故障注入、三进程 integration、ASan/UBSan、Valgrind、Release/install CI
- 同 wire specification 的 STM32 Cortex-M3 portable core 交叉编译

## 简历描述建议

> 设计并实现 C17 Linux 设备管理 daemon，使用 epoll、timerfd、signalfd 和非阻塞
> termios 统一管理串口、IPC、超时和退出；实现显式 little-endian/CRC32 流协议解析、
> 分块固件升级、丢 ACK 幂等与 FW_STATUS 断点续传，并通过 PTY simulator、故障注入、
> ASan/UBSan、Valgrind 和 GitHub Actions 验证 host 全链路。

只有当你能现场画状态机、定位一次真实失败、删除重写 parser/transport/upgrade 中至少
一个模块时，才建议把这段当成自己的经历。不要写“提升 80%”“10 万 QPS”等未测数据。

## 面试讲解路径（15 分钟）

1. 1 分钟：问题、边界、为何不是 Web 项目。
2. 3 分钟：三进程架构、reactor 与 fd ownership。
3. 3 分钟：wire frame、stream parser、CRC/sequence。
4. 4 分钟：upgrade operation、chunk、lost ACK、42% blackout resume。
5. 2 分钟：测试金字塔与一次 TTY `read()==0` 误判 disconnected 的真实修复。
6. 2 分钟：已验证边界、缺口、worker/eventfd 或实机 port 下一步。

准备现场打开 `session.c`、`upgrade.c`、`daemon.c`、两条 integration script 和 CI 运行
结果，而不是只展示 README。

