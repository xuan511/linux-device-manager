# Linux C 配套学习指南

这不是 README 翻译。每章都给出“是什么/为什么/项目位置/调用链/错误与防御/
断点/实验/重写/面试”九个入口；读完后必须动手删掉模块重写，才能形成 ownership。

## 01 项目整体架构

- 是什么/为什么：CLI、daemon、设备三进程分离，保证串口单一所有者并隔离用户接口。
- 位置/调用链：`devctl main → IPC → devmgrd reactor → session → protocol → transport → simulator`。
- 错误/防御：跨层直接调用会混淆 ownership；本项目以 public header 和状态机分层。
- 断点/实验：在 `start_request`、`on_device_frame` 断点，画一次 ping 的时序和 fd 所有者。
- 重写/面试：先隐藏源码重画组件图；回答“为什么 CLI 不直接打开串口？”

## 02 Linux 文件描述符

- 是什么/为什么：fd 是内核对象的进程内句柄，串口、socket、timer、signal 都能统一等待。
- 位置/调用链：`daemon_context` 保存全部 fd，`setup_context → epoll_ctl → cleanup_context`。
- 错误/防御：泄漏、重复 close、跨线程乱用；初始化为 -1，逆序释放，reactor 独占。
- 断点/实验：查看 `/proc/$pid/fd`，SIGTERM 后确认全部消失；重写一个 fd owner 小结构。
- 面试：解释 close 后 fd 数字为何可能立即复用，以及日志为什么不能只记录数字。

## 03 open/read/write

- 是什么/为什么：它们操作字节流，不承诺一次读写完整协议对象。
- 位置/调用链：`serial.c`、`firmware.c`、daemon/client flush/drain 循环。
- 错误/防御：partial、EINTR、EAGAIN、0 字节 TTY 读取；保存 offset，按 fd 类型解释 0。
- 断点/实验：用 strace 观察一次帧被几次 read/write；把 write 限制为小片验证输出队列。
- 重写/面试：重写 transport read/write；回答“read 为什么不等于收到一个 frame？”

## 04 termios

- 是什么/为什么：termios 配置 tty 的波特率、数据位、奇偶、停止位和线路处理。
- 位置/调用链：`devmgr_transport_open → configure_serial → tcgetattr/cfmakeraw/tcsetattr`。
- 错误/防御：canonical/echo/CRLF 转换会破坏二进制；配置 raw、115200、8N1、无流控。
- 断点/实验：去掉 `cfmakeraw` 观察 CRC；比较 VMIN/VTIME 组合和零字节 read。
- 重写/面试：重写 serial configure；解释 VMIN=0/VTIME=0 与 epoll timeout 的职责边界。

## 05 非阻塞 I/O

- 是什么/为什么：O_NONBLOCK 让单 reactor 不被一个慢设备/客户端卡住。
- 位置/调用链：transport open、`accept4`、`flush_transport`、client input/output。
- 错误/防御：把 EAGAIN 当失败或丢掉 partial write；保留 buffer+offset，等下一次 readiness。
- 断点/实验：把 socket 缓冲调小并观察 EPOLLOUT；重写一个 nonblocking write queue。
- 面试：说明 nonblocking 不等于异步，以及 readiness 到达后为什么必须 drain。

## 06 epoll

- 是什么/为什么：Linux readiness 多路复用，把串口、IPC、timer、signal 放入统一顺序模型。
- 位置/调用链：`setup_context → epoll_update → epoll_wait → fd dispatch`。
- 错误/防御：错误事件遗漏、回调阻塞、TTY 误用 EPOLLRDHUP；按 fd 类型注册并检查 HUP/ERR。
- 断点/实验：strace epoll_wait；打印每个 event flags；删除 drain 循环观察饥饿/重复唤醒。
- 重写/面试：重写最小 reactor；回答为何不是“一设备一线程”。

## 07 timerfd

- 是什么/为什么：把单调时钟 deadline 变成 fd 事件，避免业务模块 sleep。
- 位置/调用链：`timerfd_create/settime → read expirations → devmgr_session_tick`。
- 错误/防御：不读取计数导致持续可读；使用 wall clock 会受校时影响；使用 CLOCK_MONOTONIC。
- 断点/实验：改变 tick 周期但不改 request deadline，验证 timeout 语义不依赖 wall clock。
- 重写/面试：实现 deadline heap 是扩展题；解释 timerfd 比大量 sleep 更适合 reactor。

## 08 signalfd

- 是什么/为什么：先 block signal，再把 SIGINT/SIGTERM 作为普通 fd 事件有序处理。
- 位置/调用链：`sigprocmask → signalfd → epoll → running=false → cleanup_context`。
- 错误/防御：传统 handler 调非 async-signal-safe 函数；本项目 handler 外完成全部清理。
- 断点/实验：连续发送 SIGTERM，检查 socket/mmap/fd；比较直接 signal handler 的限制。
- 重写/面试：解释为什么 signal mask 必须在创建其他线程之前设置。

## 09 eventfd

- 是什么/为什么：eventfd 是线程/内核到 reactor 的计数通知 fd，适合 worker completion。
- 项目状态/位置：`src/worker/worker.c` 完成固件 mmap/CRC，eventfd 接入 daemon epoll。
- 错误/防御：不读取、计数溢出、结果 ownership 不清；reactor 读取通知并接管 immutable result。
- 断点/实验：在 `handle_worker_completion` 断点，观察 result 从 worker 移交 reactor。
- 重写/面试：解释 eventfd 为什么只传通知、不传大型结果。

## 10 pthread

- 是什么/为什么：pthread 提供并发执行；只应把耗时 CPU/文件任务移出 reactor。
- 项目状态：host 状态核心保持单 reactor，只有一个有界 validation worker。
- 错误/防御：线程过多、fd 多 owner、退出 join 遗漏；限定一个 worker，reactor 仍拥有状态/fd。
- 断点/实验：用 `thread apply all bt`；实现启动、任务、停止、join 的最小生命周期。
- 重写/面试：回答“哪些代码绝不能让 worker 直接修改？”

## 11 mutex / condition variable

- 是什么/为什么：mutex 保护队列不变量，condvar 在谓词变化时休眠/唤醒 worker。
- 项目状态：worker 单槽 mailbox 已实现；不要给 reactor-owned session 随意加锁。
- 错误/防御：忘记 while 检查谓词、丢唤醒、锁内耗时、退出死锁；队列有界并有 stop 标志。
- 断点/实验：制造 spurious wakeup；用 TSAN 扩展 CI；画锁顺序图。
- 重写/面试：解释 condition variable 为什么必须和谓词、mutex 一起使用。

## 12 Ring Buffer

- 是什么/为什么：固定容量循环队列保存跨 read 的未解析字节，无需搬移整个流。
- 位置/调用链：`ring_buffer.c → parser.input → write/peek/discard`。
- 错误/防御：full/empty 混淆、wrap 越界、partial 操作；显式 size，分两段 memcpy，全面边界测试。
- 断点/实验：容量改为 5，手算 wrap；删除模块后按测试重写。
- 面试：解释为何保存 size 而不是只用 read/write index。

## 13 流式协议解析

- 是什么/为什么：parser 在任意切片中寻找完整帧，而不是假设 read==frame。
- 位置/调用链：`devmgr_parser_feed → parser_drain → frame_decode → callback`。
- 错误/防御：坏 CRC 后永久失步、非法 length 等待巨量数据；逐字节 resync、4096 上限。
- 断点/实验：单字节 feed、1.5 帧、垃圾+坏帧+好帧；运行 stress/fuzz smoke。
- 重写/面试：删除 parser 自己实现；说明保留 magic 前缀时如何恢复。

## 14 Binary Protocol

- 是什么/为什么：版本化 frame 在带宽/固件场景可控，字段语义与 ABI 解耦。
- 位置/调用链：`protocol.h/frame.c` 与 STM32 `common/protocol.c` 共享 wire specification。
- 错误/防御：padding/alignment/endian；显式 put/get，reserved 必须为零。
- 断点/实验：手工编码 PING 并对照 protocol.md；在大端模拟器中验证字段。
- 重写/面试：回答为什么不能 `write(fd,&struct,sizeof struct)`。

## 15 CRC32

- 是什么/为什么：检测传输/存储意外损坏；不是签名或认证。
- 位置/调用链：`crc32.c → frame encode/decode`，host/simulator/STM32 参数一致。
- 错误/防御：多项式/反射/init/xor/覆盖范围不一致；known vector `123456789`。
- 断点/实验：逐 bit 跟踪前两个字节；翻转 payload 一位观察 parser crc_errors。
- 重写/面试：重写 table/bitwise 版本并互测；解释 CRC 为什么不能防恶意篡改。

## 16 状态机

- 是什么/为什么：状态+事件限制合法边，替代散落 if/else 隐式状态。
- 位置/调用链：`session.c` 和 `upgrade.c`；图见 state-machines.md。
- 错误/防御：非法跃迁、两个状态机耦合、错误后残留 pending；transition 函数和独立 upgrade。
- 断点/实验：覆盖每条合法/非法边；注入 NACK/timeout 看终态和资源。
- 重写/面试：解释为何 Device State 与 Upgrade State 必须分开。

## 17 Unix Domain Socket

- 是什么/为什么：本机 stream IPC，不暴露网络端口，可用文件权限控制访问。
- 位置/调用链：`create_listener/accept_clients` 与 devctl `connect_socket/exchange`。
- 错误/防御：partial、恶意 length、旧 socket、路径误删；长度上限、只 unlink socket、0600。
- 断点/实验：并发发两个请求、发送坏 magic；观察 busy/error 和客户端 close。
- 重写/面试：比较 AF_UNIX、TCP、D-Bus、共享内存的权衡。

## 18 Linux daemon

- 是什么/为什么：这里是 systemd 管理的 foreground service，不自行 fork/setsid。
- 位置/调用链：`devmgrd main → devmgr_daemon_run`；packaging/systemd 管 restart/runtime dir。
- 错误/防御：双重 daemonize、root 强依赖、SIGTERM 泄漏；普通用户 demo、signalfd 清理。
- 断点/实验：前台/系统服务各跑一次；journalctl 和 `/proc/$pid/fd` 检查。
- 重写/面试：解释现代服务为什么常不再 double-fork。

## 19 mmap

- 是什么/为什么：把只读 firmware 文件映射为地址区间，避免一次 malloc+read 整文件。
- 位置/调用链：`devmgr_firmware_open → fstat → mmap → CRC/chunk → munmap`。
- 错误/防御：空/超大/非普通文件、symlink、未 munmap、截断 SIGBUS；限制 16 MiB、O_NOFOLLOW。
- 断点/实验：空文件、目录、symlink、17 MiB；用 Valgrind 看释放。
- 重写/面试：解释 mmap 不等于“文件自动永远安全”，并讨论并发 truncate 风险。

## 20 CMake

- 是什么/为什么：target-based build 描述依赖、选项、测试、安装，而非第二套编译系统。
- 位置/调用链：根 CMakeLists、tests/CMakeLists、presets；Makefile 只委托。
- 错误/防御：GLOB 漏变更、全局 include/link；显式 sources、target scopes。
- 断点/实验：查看 verbose build/link；新增一个 library+test target。
- 重写/面试：解释 PUBLIC/PRIVATE/INTERFACE 传播差异。

## 21 Unit Test

- 是什么/为什么：在最小范围固定不变量和边界，失败时定位快。
- 位置/调用链：`tests/test_*`，自包含 `TEST_CHECK/TEST_RUN`。
- 错误/防御：只测 happy path、假 assert、依赖顺序；known vector、边界、错误、恢复。
- 断点/实验：故意破坏 ring wrap/offset 检查，确认测试能红。
- 重写/面试：为一个新错误路径先写失败测试再实现。

## 22 Integration Test

- 是什么/为什么：真实启动 simulator/daemon/CLI，验证进程、PTY、IPC 与状态协作。
- 位置/调用链：`run-demo.sh`、`demo-fault-recovery.sh` 由 CTest/CI 调用。
- 错误/防御：后台进程提前死、只等路径不等 READY、清理吞错；kill -0、PING gate、trap。
- 断点/实验：让 simulator 退出，确认 harness 快速打印日志而非假 timeout。
- 重写/面试：解释为何大量 mock 不能替代这一层。

## 23 Sanitizer

- 是什么/为什么：ASan 找越界/UAF/leak，UBSan 找未定义行为。
- 位置/调用链：asan preset、run-sanitizers.sh、独立 CI job 跑全套测试。
- 错误/防御：只编译不运行、忽略 child process；同一 integration suite 在 sanitizer 下执行。
- 断点/实验：临时制造越界观察 stack trace，修复后删除实验代码。
- 重写/面试：解释 sanitizer 为什么不能证明无 race/无逻辑错误。

## 24 Valgrind

- 是什么/为什么：动态模拟检查 invalid access 和 definite leak，速度慢但独立于 ASan。
- 位置/调用链：`run-valgrind.sh` 跑全部 host tests 和三进程 demo。
- 错误/防御：只看 leak summary、忽略 error exit；definite leak/invalid access 都令 CI 失败。
- 断点/实验：使用 `--track-fds=yes` 扩展；比较 ASan/Valgrind 报告。
- 重写/面试：说明二者覆盖与性能差异。

## 25 Fault Injection

- 是什么/为什么：可重复制造正常环境难出现的丢包、损坏、延迟、黑洞、verify 失败。
- 位置/调用链：device-sim 参数 → `write_frame`/FW_DATA/VERIFY；xorshift seed。
- 错误/防御：随机不可复现、fault 自身破坏状态；seed、单次百分比触发、有界参数。
- 断点/实验：42% 黑洞观察 TRANSFER→RECOVER→FW_STATUS→TRANSFER。
- 重写/面试：解释为什么黑洞模型不等于真实 USB 拔插。

## 26 Firmware Upgrade

- 是什么/为什么：跨多个有状态命令安全传输、校验、激活并恢复，而非一个 UPDATE 文本命令。
- 位置/调用链：devctl operation → daemon upgrade → session → simulator flash model。
- 错误/防御：整文件 malloc、重复写、offset gap、ACK 丢失、无限恢复；mmap/chunk/idempotency/cap。
- 断点/实验：在每个 upgrade state 断点；改坏 verify CRC；CLI 退出后用 operation ID 查询。
- 重写/面试：删除 upgrade.c 重写；回答 ACK 丢失后重复 FW_DATA 是否会重复写 flash。

## 27 STM32 Port

- 是什么/为什么：同一 wire spec 的 freestanding Cortex-M3 portable core，flash/UART 由 BSP 注入。
- 位置/调用链：`firmware/stm32f103/common`、`bootloader/update.c`、flash ops。
- 错误/防御：把交叉编译冒充实机验证；hardware-status.md 明确三层边界。
- 断点/实验：arm-none-eabi objdump 查看对象；在 fake flash host harness 测 idempotency。
- 重写/面试：实现某块板 flash ops；解释掉电、双 bank、boot flag 和 watchdog 风险。

## 28 CI

- 是什么/为什么：在干净 Linux 环境持续重放构建/测试，阻止“只在我机器能跑”。
- 位置/调用链：`.github/workflows/ci.yml` 的 normal、sanitizer、Valgrind、STM32 jobs。
- 错误/防御：隐藏日志、删除 flaky integration、只跑 Debug；失败注释、原测试、Release/install。
- 断点/实验：本地 `ci-local.sh`，再故意破坏 known vector 观察各 job。
- 重写/面试：解释为什么 job 分开，以及 CI success 仍不能证明实机行为。
