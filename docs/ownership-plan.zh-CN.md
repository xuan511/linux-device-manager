# Ownership：拆解与重写计划

目标不是“看懂 AI 代码”，而是让你能在没有原实现时重新做出关键模块、解释权衡并
承担故障。每阶段都保存自己的图、实验记录、失败日志和重写 commit。

## Stage 1：只运行，不读实现

跑普通 demo、42% fault demo、ASan、Valgrind；记录三个进程、所有 fd、每条命令输出。
验收：能从零启动，能解释哪部分真实验证、哪部分未验证。

## Stage 2：画架构与 ownership

只读 public headers 和文档，画 ping/telemetry/upgrade 时序，标出每个 fd、buffer、mmap
的创建/拥有/释放者。验收：不看 architecture.md 复述 data flow 和 failure propagation。

## Stage 3：读四个基础模块

顺序为 `crc32 → ring_buffer → frame → parser`；每个函数写输入、不变量、返回值和边界。
用 gdb/单字节 feed 验证，不接受“代码看起来对”。

## Stage 4：删除并重写 parser

新分支删除 `parser.c`，只保留 header/tests/protocol.md；先让测试编译失败，再逐项恢复
fragment/coalesce/garbage/CRC/length/resync/stress。比较你的接口与参考实现。

## Stage 5：重写 serial transport

从 termios man page 出发实现 115200/8N1/raw/O_NONBLOCK；用 PTY test 验证双向字节，
专门复现 VMIN=0 零字节 read。不要复制参考函数。

## Stage 6：重写 upgrade state machine

先画状态/事件表，再实现 begin/data/end/verify/activate/reboot 和错误边；用 fake response
走 2500 字节三 chunk、坏 offset、FW_STATUS resume。

## Stage 7：实现 worker + eventfd（原创扩展）

把 mmap/CRC validation 移出 reactor：单 worker、有界队列、mutex+condvar、immutable result、
eventfd completion、SIGTERM join。新增大文件期间 ping 不被阻塞的 integration test。

## Stage 8：真实新功能

二选一：多设备 session+hotplug，或签名 manifest+anti-rollback。先写 ADR/threat model/
acceptance tests，再实现；benchmark 只记录真实测量。完成后用 15 分钟向别人讲解并接受追问。

