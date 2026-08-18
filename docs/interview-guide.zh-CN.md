# 项目技术面试指南（40 题）

答案是讲解骨架，不是背诵稿。面试时应结合一次真实失败、代码位置和验证方法。

1. **为什么 daemon 用 epoll 而不是一设备一线程？** 单 reactor 统一串口、IPC、timer、signal 的顺序和 ownership，避免共享 session 加锁；耗时文件任务才适合 worker。
2. **read 为什么不等于收到一帧？** tty/socket 是字节流，任意分片/粘包；parser 跨 read 保存 ring buffer，按 header length 等完整帧。
3. **坏 CRC 后如何恢复？** 不相信坏帧 length/payload，丢一个字节重新扫描 magic；length 有 4096 上限，后续好帧可恢复。
4. **为何不能 memcpy C struct 到 wire？** padding、alignment、ABI、endianness 不稳定；项目逐字段 put/get little endian。
5. **CRC32 的参数是什么？** ISO-HDLC reflected，poly 04C11DB7/反射 EDB88320，init/xorout FFFFFFFF，check CBF43926。
6. **CRC 能保证固件安全吗？** 不能，只检测意外损坏；恶意安全需签名、公钥、anti-rollback、secure boot。
7. **为何 timeout 用 CLOCK_MONOTONIC？** wall clock 会被 NTP/人工校时跳变；elapsed deadline 只应随单调时间前进。
8. **timerfd 比 sleep 好在哪里？** deadline 进入 epoll 统一事件序，不阻塞 reactor，也能读取累积 expirations。
9. **signalfd 的价值？** block signal 后在普通上下文有序清理，避免 handler 调用非 async-signal-safe 函数。
10. **TTY read==0 一定是 EOF 吗？** 不一定；VMIN=0/VTIME=0 排空后可返回 0。项目以 HUP/ERR/真实 I/O 错误判断 transport loss。
11. **为何 TTY 不注册 EPOLLRDHUP？** RDHUP 是 stream socket 半关闭语义；PTY/serial 应看 EPOLLHUP/ERR，避免类型混淆。
12. **partial write 怎么处理？** reactor buffer 保存 size+offset，写到 EAGAIN 后注册 EPOLLOUT，下次从 offset 继续。
13. **sequence 有什么作用？** 关联 request/response/ACK，错序、过期、重复响应不能完成当前请求并计数。
14. **为什么每设备只允许一个 in-flight request？** 串行固件状态与响应类型更易推理；吞吐让位于正确性，未来可按协议能力扩窗。
15. **session timeout 后发生什么？** 原帧带相同 sequence 和 retry flag 有界重发；耗尽清 pending 并返回 timeout。
16. **为何 upgrade state 独立于 device state？** transport 可断线重连，但固件 offset/session 必须存活；两者变化速度和恢复语义不同。
17. **FW_DATA ACK 丢了会重复写 flash 吗？** device 按 session+offset；已提交范围只有字节完全相同才 ACK，不再次写，否则 NACK。
18. **断点续传如何实现？** 普通 retry 耗尽进入 RECOVER，发 FW_STATUS(session)，校验 offset≤image size，从该 offset 继续，最多三轮。
19. **为何 FW_STATUS 不直接相信设备 offset？** 错误/恶意 offset 会越界 mmap；必须验证 session、payload、上限和合法状态。
20. **为什么 firmware 不 malloc(file_size)？** mmap 只读文件，分块访问和发送，映射/大小/fd 生命周期清晰；仍需防 concurrent truncate。
21. **升级时 CLI 断开怎么办？** operation 属于 daemon；start 返回 ID，CLI 短连接轮询，退出不取消设备状态机。
22. **IPC timeout 和 device timeout 如何分层？** 单次本地 exchange 5 秒；设备命令 2 秒+retry/recovery；没有一个 socket 跨越整个升级。
23. **为什么 IPC 选 AF_UNIX？** 本机、低配置、文件权限、无网络暴露；TCP/D-Bus/共享内存各增加不需要的攻击面或依赖。
24. **IPC 如何防恶意长度？** 先读固定 header，32-bit length 与 4096 上限比较，再计算 expected size；codec 要求 exact length。
25. **旧 socket path 怎么处理？** lstat 后仅当目标是 socket 才 unlink；普通文件绝不删除。
26. **PTY simulator 为什么是核心而非 demo？** 它提供真实 tty/byte-stream/process 行为，让 CI 无 root/硬件验证 parser、daemon 和升级恢复。
27. **fault blackout 与真实拔线差异？** 黑洞保留 fd/path，只丢响应，稳定测试 FW_STATUS；USB 拔插还包含 HUP、udev、路径变化，需实机。
28. **故障注入如何可复现？** xorshift 固定 seed，概率参数有界，percentage blackout 只触发一次。
29. **如何证明 parser 不怕随机切片？** unit 覆盖单字节/粘包/坏帧，stress 2000 帧随机 1–23 字节片，另有 10000 随机输入和 fuzz entry。
30. **ASan、UBSan、Valgrind 各验证什么？** ASan 内存边界/UAF/leak，UBSan 未定义行为，Valgrind 独立动态检查；都不证明业务逻辑或实机正确。
31. **如何检查 fd 泄漏？** `/proc/$pid/fd`、strace close、Valgrind/`--track-fds`; SIGTERM 后进程/socket 都消失。
32. **daemon 为什么 foreground？** systemd 管 fork/restart/log/runtime dir；自行 double-fork 会让监督和退出状态更复杂。
33. **systemd 服务为何不默认 root？** 串口通过 dialout/udev 授权，socket/runtime dir 由专用账号拥有，减少攻击面。
34. **当前 concurrency 最大缺口？** mmap/CRC validation 仍在 reactor；下一步单 worker+有界队列+eventfd，不把 session 变成共享对象。
35. **eventfd 应传什么？** 只传 completion 计数；实际 immutable result 放受 mutex 保护的队列，reactor 读取后接管。
36. **condition variable 为什么用 while？** 唤醒可能虚假或条件被其他消费者改变；必须在 mutex 下重查谓词。
37. **STM32 端验证到了哪里？** portable protocol/update core 用 arm-none-eabi-gcc Cortex-M3 warnings-as-errors 编译；没有链接/烧录/实机行为验证。
38. **真实 STM32 port 还缺什么？** startup/linker/HAL、UART DMA、flash geometry、boot metadata、掉电原子性、watchdog、跳转和板级测试。
39. **如何做性能陈述？** 保存环境/commit/raw samples，区分 PTY 与 115200 UART，报告分布；没有测量就不写数字。
40. **你会先重写哪个模块证明 ownership？** parser：范围小但覆盖 ring/stream/CRC/resync；保留 tests/header，删除实现，从失败测试逐步恢复。

