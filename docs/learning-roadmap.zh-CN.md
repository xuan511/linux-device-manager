# 8 周学习路线

建议每周 8–12 小时，保留可运行主分支，重写放独立分支。

| 周 | 学习目标 | 代码范围 / Linux API | 实验与重写 | 调试任务 | 输出成果 / 面试问题 |
|---|---|---|---|---|---|
| 1 | 建立全局模型 | apps、architecture；fd/process | 跑两套 demo，画三条时序 | strace、/proc/fd | 架构图；为何 daemon 独占串口 |
| 2 | 字节与边界 | CRC/ring/frame；read/write | 重写 CRC+ring | gdb wrap、hex dump | known vectors；read≠frame |
| 3 | 流解析 | parser；nonblocking/EAGAIN | 删除重写 parser | 随机切片、坏 CRC | parser 状态图；如何 resync |
| 4 | tty/reactor | termios/PTY/epoll/timerfd/signalfd | 重写 serial，做最小 reactor | strace epoll、零 read | fd ownership 表；为何不用 sleep |
| 5 | IPC/session | AF_UNIX、sequence、deadline | 新增坏 IPC client/test | 并发 client、stale response | timeout 表；为何 CLOCK_MONOTONIC |
| 6 | 固件升级 | mmap、chunk、upgrade states | 重写 upgrade.c | 丢 ACK、坏 offset、verify fail | 升级时序；幂等如何保证 |
| 7 | 并发工程化 | pthread/mutex/condvar/eventfd | 实现 validation worker | gdb threads、TSAN（扩展） | 锁/ownership 图；worker 边界 |
| 8 | 交付与实机边界 | CI/systemd/udev/STM32 core | 选一原创功能 | ASan/Valgrind/journalctl | 作品集讲稿、40 题复盘、硬件验证计划 |

每周验收包含：全量测试仍绿；至少一个失败实验；一份“我为什么这样设计”的短文；
一个不看答案现场编码任务。不要用背 API 名称替代运行和调试。

