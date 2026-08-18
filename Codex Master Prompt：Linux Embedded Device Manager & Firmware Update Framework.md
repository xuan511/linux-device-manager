# Codex Master Prompt：Linux Embedded Device Manager & Firmware Update Framework

## 0. 你的角色

你现在不是在帮我写一个课程 Demo，也不是生成几百行“能跑就行”的示例程序。

请把自己视为：

**一名资深 Linux 系统软件工程师 + 嵌入式软件工程师 + 软件架构师 + Code Reviewer。**

你的任务是在当前仓库中，从零设计并实现一个：

# Linux Embedded Device Manager & Firmware Update Framework

这是一个面向真实嵌入式设备管理场景的 Linux C 工程。

项目最终需要同时具有：

- Linux 系统编程深度
- 嵌入式设备通信能力
- 二进制协议设计
- Firmware Update 能力
- 异步 I/O
- 状态机
- IPC
- daemon
- 并发
- 错误恢复
- 故障注入
- 自动化测试
- CI
- CMake
- Sanitizer
- 调试能力
- 完整工程文档
- 可学习性
- 可维护性
- 可扩展性
- 足够的秋招简历竞争力

这不是商业产品，但代码质量、架构思维、测试和可靠性标准应尽量向真实工程靠拢。

---

# 1. 项目背景

项目的最终使用者是一名计算机专业本科生，目标岗位主要包括：

- 嵌入式软件工程师
- Linux C/C++ 工程师
- BSP / Firmware Engineer
- IoT / Device Software Engineer
- Linux 系统软件方向

已有一定经历：

- C
- STM32
- UART
- Bootloader
- IAP
- 双 APP / AB 固件升级
- CRC
- Flash
- Makefile
- Shell
- GCC
- Git
- Gerrit
- Jenkins
- Linux 基础

但是 Linux 系统编程能力目前还不够系统。

因此这个项目的重要目标之一是：

> 将 STM32 / Firmware / IAP 经验向 Linux 系统软件方向延伸，最终形成“设备端 + 通信协议 + Linux Host + Firmware Upgrade + 工程化”的完整技术链。

所以请避免：

“再做一个简单 STM32 项目”。

本项目的技术重心应该在：

**Linux C 系统软件。**

STM32 Firmware 是真实硬件验证端和扩展部分，而不是整个项目的主体。

---

# 2. 项目总体目标

开发一个 Linux Embedded Device Management Framework。

Linux 主机能够通过 UART/PTY 与嵌入式设备通信，并实现：

1. 设备发现
2. 建立连接
3. 握手
4. 查询设备信息
5. Ping
6. 查询健康状态
7. 获取设备运行状态
8. 实时 Telemetry
9. 获取通信统计信息
10. Firmware Upgrade
11. Firmware Verify
12. Firmware Resume
13. Device Reboot
14. 错误恢复
15. 超时处理
16. ACK/NACK
17. 重传
18. CRC 校验
19. 断线重连
20. 多设备管理

系统必须能够在：

**没有真实 STM32 的情况下完整运行。**

因此必须实现：

# Device Simulator

Device Simulator 使用 Linux PTY 模拟真实串口设备。

真实硬件只是最终 Port。

---

# 3. 项目不是 Demo

请严格避免生成以下类型的项目：

- 一个 main.c 写所有逻辑
- 500 行串口 Demo
- read() 一次就假设得到完整一帧
- 大量 sleep()
- 到处 if/else 维持状态
- 全局变量管理全部状态
- malloc 后不考虑失败
- 不处理 partial read/write
- 不处理 EINTR
- 不处理 EAGAIN
- 不处理断线
- 不处理 CRC 错误
- 不处理超时
- 没有状态机
- 没有单元测试
- 没有集成测试
- 没有错误注入
- 没有日志
- 没有 CI
- 没有 Sanitizer
- 只有 README，没有设计文档
- 为了“看起来高级”加入无意义框架

特别禁止为了增加技术栈而加入：

- React
- Vue
- Web Dashboard
- Electron
- Qt GUI
- 数据库
- Redis
- Kafka
- Docker 微服务
- AI/LLM
- 云平台
- Kubernetes

除非未来明确证明项目确实需要。

第一阶段完全不需要 GUI。

这是：

**Linux systems programming project**

而不是互联网 Web 项目。

---

# 4. 项目名称

Repository：

`linux-device-manager`

项目正式名称：

**Linux Embedded Device Manager & Firmware Update Framework**

建议二进制程序：

```text
devmgrd
devctl
device-sim
```

含义：

```text
devmgrd
Linux device manager daemon

devctl
Device management CLI client

device-sim
Embedded device simulator
```

---

# 5. 总体架构

目标架构：

```text
                     ┌─────────────────┐
                     │     devctl      │
                     │      CLI        │
                     └────────┬────────┘
                              │
                     Unix Domain Socket
                              │
                              ▼
                    ┌───────────────────┐
                    │      devmgrd      │
                    │      Daemon       │
                    └─────────┬─────────┘
                              │
                         Device Manager
                              │
                ┌─────────────┼─────────────┐
                │             │             │
                ▼             ▼             ▼
          Device Session   Upgrade      Telemetry
                │           Manager       Manager
                │
                ▼
         Protocol Engine
                │
                ▼
       Transport Abstraction
                │
          ┌─────┴──────┐
          │            │
          ▼            ▼
       Serial          PTY
          │            │
          ▼            ▼
       STM32       Device Simulator
```

软件层次必须清晰。

---

# 6. 推荐目录结构

请根据实际设计微调，但不要把所有东西扔进 src。

建议：

```text
linux-device-manager/
│
├── apps/
│   ├── devctl/
│   ├── devmgrd/
│   └── device-sim/
│
├── include/
│   └── devmgr/
│
├── src/
│   ├── common/
│   ├── core/
│   ├── transport/
│   ├── protocol/
│   ├── device/
│   ├── upgrade/
│   ├── ipc/
│   ├── logging/
│   └── worker/
│
├── simulator/
│   ├── protocol/
│   ├── device/
│   ├── flash/
│   └── fault/
│
├── firmware/
│   └── stm32f103/
│       ├── bootloader/
│       ├── application/
│       ├── common/
│       └── README.md
│
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── stress/
│   └── fuzz/
│
├── docs/
│   ├── architecture.md
│   ├── protocol.md
│   ├── firmware-update.md
│   ├── state-machines.md
│   ├── reliability.md
│   ├── threat-model.md
│   ├── testing.md
│   ├── debugging.md
│   ├── performance.md
│   ├── learning-guide.zh-CN.md
│   ├── learning-roadmap.zh-CN.md
│   ├── interview-guide.zh-CN.md
│   └── ownership-plan.zh-CN.md
│
├── scripts/
│   ├── run-demo.sh
│   ├── ci-local.sh
│   ├── run-sanitizers.sh
│   └── benchmark.sh
│
├── packaging/
│   ├── systemd/
│   └── udev/
│
├── .github/
│   └── workflows/
│
├── AGENTS.md
├── CMakeLists.txt
├── CMakePresets.json
├── Makefile
├── .clang-format
├── .editorconfig
├── .gitignore
├── LICENSE
├── README.md
└── README.zh-CN.md
```

不要为了完全匹配这个目录而破坏合理架构。

你可以修改，但需要在 architecture.md 中解释。

---

# 7. 编程语言

Linux Host：

**C17**

尽量不要使用 C++。

目的就是学习：

Linux C Systems Programming。

STM32：

C。

Shell：

Bash。

CMake：

现代 CMake。

---

# 8. 支持环境

最低目标：

- Ubuntu 22.04
- Ubuntu 24.04
- WSL2 Ubuntu

host + simulator + tests：

必须可以在普通 Linux 用户权限下运行。

核心 Demo：

**不能要求 root。**

systemd / udev 属于额外部署能力，可以需要管理员权限，但不能影响普通开发运行。

---

# 9. 外部依赖原则

尽量保持依赖简单。

核心优先使用：

- libc
- POSIX
- pthread
- Linux system calls

不要为了方便加入大型库。

CLI 参数：

使用：

`getopt_long()`

不要引入 CLI Framework。

协议：

自己实现。

CRC32：

自己实现并充分测试。

IPC：

自己实现。

event loop：

自己实现。

ring buffer：

自己实现。

这样可以保证项目具有学习价值。

---

# 10. Linux 系统编程要求

项目必须合理使用并体现以下 Linux API。

不是为了堆 API，而是必须有合理应用场景。

## 文件描述符

```text
open
close
read
write
pread
fcntl
```

必须正确处理：

```text
EINTR
EAGAIN
EWOULDBLOCK
partial read
partial write
```

---

# 11. Serial Transport

实现：

```text
serial_open
serial_configure
serial_read
serial_write
serial_close
```

至少支持：

```text
115200
8N1
no flow control
raw mode
```

使用：

```text
termios
tcgetattr
tcsetattr
cfsetispeed
cfsetospeed
```

禁止简单复制网上的 termios 代码。

必须理解并写文档解释：

- canonical / noncanonical
- VMIN
- VTIME
- baud rate
- parity
- stop bit
- flow control
- raw mode

---

# 12. Transport Abstraction

协议层不得直接依赖 UART。

建立统一 Transport API：

类似：

```c
struct transport;

int transport_open(...);
ssize_t transport_read(...);
ssize_t transport_write(...);
int transport_get_fd(...);
void transport_close(...);
```

支持至少：

```text
SERIAL
PTY
```

未来允许扩展：

```text
TCP
USB
CAN
```

但是 TCP/CAN 当前不是必须实现的核心功能。

---

# 13. Linux Event Loop

这是项目的重要技术核心。

不要使用：

```text
while(1)
{
    read();
    sleep();
}
```

应实现真正的事件驱动架构。

优先使用：

```text
epoll
```

并合理结合：

```text
timerfd
signalfd
eventfd
```

建议：

```text
epoll
 ├── serial fd
 ├── unix socket
 ├── timerfd
 ├── signalfd
 └── worker eventfd
```

必须真正理解为什么这样设计。

文档中解释：

```text
为什么选择 epoll
为什么不用 busy loop
为什么 timerfd 比大量 sleep 更合理
为什么 signalfd 有利于统一事件模型
eventfd 如何在线程和 event loop 之间通知
```

---

# 14. 并发模型

不要无意义创建大量线程。

推荐设计：

## 主线程

负责：

```text
epoll reactor
serial IO
IPC
device session state
timers
```

## Worker Thread

用于：

```text
firmware file validation
CRC calculation
较耗时文件任务
```

主线程和 Worker 之间可以通过：

```text
job queue
mutex
condition variable
eventfd
```

通信。

目的：

同时学习：

```text
pthread
mutex
condition variable
eventfd
线程安全
```

但不要把整个工程写成混乱多线程。

---

# 15. Device Simulator

Device Simulator 是核心模块，不是附属 Demo。

必须使用：

```text
posix_openpt()
grantpt()
unlockpt()
ptsname()
```

自己创建 PTY。

不能依赖：

`socat`

作为必须条件。

启动：

```bash
device-sim
```

应该创建类似：

```text
/dev/pts/5
```

并输出 PTY 路径。

Simulator 必须模拟真实 Embedded Device。

---

# 16. Simulator 状态

Simulator 至少保存：

```text
device id
model
hardware version
firmware version
bootloader version
serial number
uptime
temperature
voltage
active state
boot state
upgrade state
flash state
rx statistics
tx statistics
error statistics
```

可以合理模拟变化。

---

# 17. 二进制协议

不要使用：

```text
PING\n
INFO\n
UPDATE\n
```

这种文本协议作为核心协议。

设计真正的 binary protocol。

推荐 Frame：

```text
+----------+
| MAGIC    |
+----------+
| VERSION  |
+----------+
| TYPE     |
+----------+
| FLAGS    |
+----------+
| RESERVED |
+----------+
| SEQ      |
+----------+
| LENGTH   |
+----------+
| PAYLOAD  |
+----------+
| CRC32    |
+----------+
```

建议：

```text
MAGIC       uint16_t
VERSION     uint8_t
TYPE        uint8_t
FLAGS       uint8_t
RESERVED    uint8_t
SEQ         uint32_t
LENGTH      uint16_t
PAYLOAD     N bytes
CRC32       uint32_t
```

Wire format 必须明确规定：

**Little Endian**

但是禁止直接：

```c
memcpy(struct)
```

作为 wire encoding。

必须显式 encode/decode：

```text
put_le16
put_le32
get_le16
get_le32
```

避免：

```text
padding
alignment
endianness
ABI
```

问题。

---

# 18. Protocol Parser

协议解析必须支持：

```text
一个 read 得到半帧
一个 read 得到一帧
一个 read 得到多个帧
一个 read 得到 1.5 帧
任意随机切片
CRC Error
Invalid Length
Garbage
Frame Resynchronization
```

必须采用：

**stream parser**

不得假设：

```text
read() == frame
```

需要：

```text
ring buffer
parser state machine
```

或者同等合理方案。

---

# 19. Ring Buffer

实现通用 Ring Buffer。

需要测试：

```text
empty
full
wrap around
read
write
peek
overflow
underflow
partial operations
```

不要隐藏边界问题。

---

# 20. Protocol Commands

至少设计：

```text
PING
GET_INFO
GET_HEALTH
GET_STATS
START_TELEMETRY
STOP_TELEMETRY

ENTER_BOOTLOADER

FW_BEGIN
FW_DATA
FW_STATUS
FW_END
FW_VERIFY
FW_ACTIVATE

REBOOT

ACK
NACK
ERROR
```

允许根据架构合理调整。

protocol.md 必须给出：

```text
Command ID
Request Payload
Response Payload
Error Code
State Requirement
Timeout
Retry Policy
```

---

# 21. Sequence Number

每个 request 支持：

```text
sequence number
```

用于关联：

```text
request
response
ACK
NACK
```

需要正确处理：

```text
duplicate frame
stale response
unexpected response
sequence mismatch
```

---

# 22. CRC

使用：

CRC32。

需要清晰定义：

```text
polynomial
initial value
xor out
reflection
byte range
```

Host / Simulator / STM32：

必须一致。

必须有：

known test vectors。

---

# 23. Device Session State Machine

必须采用明确状态机。

例如：

```text
DISCONNECTED
      │
      ▼
CONNECTING
      │
      ▼
HANDSHAKING
      │
      ▼
READY
      │
      ├─────────► STREAMING
      │
      └─────────► UPGRADING
                      │
                      ▼
                  REBOOTING
                      │
                      ▼
                HANDSHAKING
```

错误：

```text
TIMEOUT
CRC_ERROR
DISCONNECT
PROTOCOL_ERROR
DEVICE_ERROR
```

不能靠散落 if/else 管理。

---

# 24. Firmware Upgrade State Machine

必须独立实现。

建议：

```text
IDLE

PREPARE
   ↓
QUERY_DEVICE
   ↓
VALIDATE_IMAGE
   ↓
ENTER_BOOTLOADER
   ↓
HANDSHAKE
   ↓
FW_BEGIN
   ↓
TRANSFER
   ↓
FW_END
   ↓
VERIFY
   ↓
ACTIVATE
   ↓
REBOOT
   ↓
WAIT_RECONNECT
   ↓
CONFIRM_VERSION
   ↓
COMPLETE
```

任何阶段可能进入：

```text
ERROR
RETRY
RECOVER
```

---

# 25. Firmware Transfer

Firmware 不应该一次性：

```text
malloc(file_size)
```

然后全部载入。

应该设计：

```text
chunked transfer
```

例如：

```text
256
512
1024 byte
```

根据协议协商。

Firmware Manager 至少维护：

```text
firmware size
firmware CRC
version
current offset
acknowledged offset
chunk size
retry count
session id
```

---

# 26. Firmware File

合理使用：

```text
open
fstat
mmap
munmap
```

用于 firmware 文件处理。

需要：

```text
文件大小检查
空文件检查
最大 firmware size
CRC
权限错误
文件被截断
非法路径
```

mmap 必须正确释放。

---

# 27. Resume

Firmware Upgrade 必须支持：

**resume / reconnect recovery**

例如升级到：

```text
32768 / 65536
```

连接断开。

重新连接后：

Host：

```text
FW_STATUS
```

Device：

```text
next expected offset = 32768
```

Host：

继续：

```text
32768
```

而不是从零开始。

Simulator 必须可以验证。

---

# 28. Timeout / Retry

设计统一 Timeout Manager。

不要每个模块自己：

```text
sleep()
```

建议通过：

```text
timerfd
deadline
monotonic clock
```

实现。

使用：

```text
CLOCK_MONOTONIC
```

而不是 wall clock 做超时。

Retry 应支持：

```text
max retries
retry interval
backoff
```

但是不要无限重试。

---

# 29. Fault Injection

Device Simulator 必须提供强大的 Fault Injection。

例如：

```bash
device-sim --drop-rate 0.05
```

支持至少：

```text
packet drop
response drop
CRC corruption
random response delay
fixed response delay
duplicate frame
garbage bytes
disconnect
reconnect
device reset
flash write failure
verify failure
NACK
timeout
partial write
```

参数接口可以自行设计。

必须可 deterministic：

支持：

```text
--seed
```

使 CI 能稳定复现。

---

# 30. 升级中断测试

必须测试：

```text
正常升级
10% 进度断线
50% 进度断线
90% 进度断线
ACK 丢失
DATA 重复
CRC 错误
VERIFY 失败
设备重启
daemon 重启
```

重要：

这些不能只存在 README。

必须至少有一部分成为：

**automated integration tests。**

---

# 31. Daemon

`devmgrd`

负责：

```text
device connection
device lifecycle
protocol
upgrade
telemetry
statistics
logging
IPC
```

默认前台运行，便于开发：

```bash
devmgrd --foreground
```

安装后支持 systemd。

Daemon 不应该随便 double-fork。

如果使用 systemd：

优先前台服务模式。

---

# 32. Unix Domain Socket IPC

`devctl` 不直接控制 daemon 内部数据。

使用：

```text
Unix Domain Socket
```

例如：

```text
/run/devmgrd.sock
```

开发模式允许：

```text
/tmp/devmgrd-$UID.sock
```

避免必须 root。

IPC 必须定义自己的 message format。

不引入：

```text
gRPC
protobuf
DBus
```

除非未来扩展。

需要处理：

```text
client connect
disconnect
partial message
malformed request
daemon unavailable
multiple clients
```

---

# 33. CLI

实现：

```text
devctl
```

至少支持：

```bash
devctl list

devctl ping DEVICE

devctl info DEVICE

devctl health DEVICE

devctl stats DEVICE

devctl monitor DEVICE

devctl upgrade DEVICE firmware.bin

devctl reboot DEVICE
```

建议：

```text
--help
--version
--verbose
--timeout
```

CLI 必须提供良好错误信息。

例如：

```text
ERROR: device 'board0' is not connected
```

而不是：

```text
failed
```

---

# 34. 多设备

Daemon 至少架构上支持多个 Device Session。

理想情况下真正支持：

```text
board0
board1
sim0
sim1
```

每个设备维护独立：

```text
fd
parser
sequence
state
timers
statistics
upgrade context
```

Integration Test 最好测试同时两个 simulator。

---

# 35. Telemetry

Simulator 周期发送：

```text
temperature
voltage
uptime
error count
```

CLI：

```bash
devctl monitor sim0
```

显示：

```text
12:01:02 TEMP=25.3C VOLTAGE=3.30V UPTIME=102s
12:01:03 TEMP=25.4C VOLTAGE=3.30V UPTIME=103s
```

注意：

Telemetry 是 protocol event。

不能破坏 Request/Response。

---

# 36. Statistics

每个 Device Session 统计：

```text
bytes_rx
bytes_tx
frames_rx
frames_tx
crc_errors
parser_errors
timeouts
retries
disconnects
reconnects
upgrade_bytes
upgrade_retries
```

`devctl stats`

可以查看。

---

# 37. Logging

设计统一 Logger。

支持：

```text
ERROR
WARN
INFO
DEBUG
TRACE
```

日志必须包含尽量有价值的信息：

```text
timestamp
level
module
device
message
```

不要每行无脑打印。

允许：

```text
--log-level
```

systemd 下让 stdout/stderr 被 journal 捕获即可。

不要求依赖 libsystemd。

---

# 38. Error Handling

定义统一错误模型。

例如：

```c
enum devmgr_error
```

但是不要完全丢掉 errno 信息。

需要区分：

```text
system error
transport error
protocol error
device error
upgrade error
timeout
invalid input
```

不能大量：

```c
return -1;
```

然后调用者完全不知道原因。

---

# 39. Resource Lifetime

高度重视 C Resource Management。

检查：

```text
fd
socket
mmap
malloc
mutex
thread
timerfd
eventfd
signalfd
PTY
firmware file
```

任何 error path 都不能泄漏。

允许合理使用：

```c
goto cleanup;
```

不要因为迷信 goto 而写出重复错误处理。

---

# 40. Memory Safety

必须防范：

```text
buffer overflow
integer overflow
signed/unsigned conversion
invalid length
out-of-bounds
use-after-free
double free
NULL dereference
```

Wire length 永远不能直接相信。

所有长度必须进行：

```text
range validation
```

---

# 41. C Coding Style

要求：

```text
C17
```

编译警告至少：

```text
-Wall
-Wextra
-Wpedantic
-Wshadow
-Wformat=2
-Wstrict-prototypes
```

在合理情况下增加：

```text
-Wconversion
-Wsign-conversion
```

尽可能做到：

**zero warnings**

不要为了 zero warnings 乱 cast。

---

# 42. 代码设计规则

避免：

```text
mutable global state
巨大函数
巨大源文件
复杂宏
宏模拟 OOP
过度抽象
过度设计
copy-paste
```

推荐：

```text
context struct
opaque types
static internal functions
small modules
clear ownership
explicit state
```

函数名字清晰。

注释主要解释：

**WHY**

而不是：

```c
i++; // increment i
```

---

# 43. Assertions

`assert()`：

只能用于：

程序员错误 / invariant。

不能用来处理：

用户输入、设备数据、文件数据。

Release build 也必须正确处理非法数据。

---

# 44. Security Mindset

虽然项目不是安全 OTA 产品，但仍需要体现基本安全工程意识。

处理：

```text
malformed packet
oversized length
invalid firmware
IPC malformed input
unexpected state transition
path errors
permission problems
```

不要：

```text
system(user_input)
```

不要 shell 拼接用户输入。

---

# 45. Threat Model

生成：

```text
docs/threat-model.md
```

说明当前项目：

解决什么问题。

不解决什么问题。

尤其说明：

CRC32：

用于：

```text
data integrity / transmission error detection
```

不是 cryptographic authentication。

不要错误宣传成：

“secure firmware update”。

未来可以规划：

```text
Ed25519 signature
secure boot
anti-rollback
```

但当前不要为了炫技强行加入密码学。

---

# 46. Device Simulator Flash

Simulator 模拟 Flash。

例如：

```text
flash.bin
```

或者内存映射文件。

模拟：

```text
erase
write
read
verify
activation
```

必须检查：

```text
address bounds
write size
image size
```

提供 Flash Failure 注入。

---

# 47. STM32F103 Reference Port

Host + Simulator 完成并稳定后，再提供真实 STM32 参考端。

目标硬件：

```text
STM32F103C8T6
UART
ST-Link / J-Link
```

Firmware 不应该成为整个项目最复杂的部分。

建议结构：

```text
bootloader
application
common protocol
```

UART：

```text
115200 8N1
```

需要支持至少：

```text
PING
GET_INFO
GET_HEALTH
FW_BEGIN
FW_DATA
FW_END
FW_VERIFY
REBOOT
```

---

# 48. STM32 Bootloader 原则

不要实现 AB 双区。

这是 Linux 项目的 Reference Firmware。

可以采用：

```text
Bootloader
+
Single Application
```

Bootloader 必须永久保留。

Firmware Upgrade：

```text
Host
 ↓
Bootloader
 ↓
erase app
 ↓
write chunks
 ↓
verify CRC
 ↓
mark image valid
 ↓
jump app
```

如果中途断电：

Bootloader 仍然存在。

下次启动检测 APP 无效：

停留 Bootloader。

Host 可以重新升级。

这样足够体现可靠性，同时避免重复实现复杂 AB 系统。

---

# 49. STM32 Flash Layout

不要假设所有 F103C8 都有 128KB。

默认按：

**64KB Flash**

设计。

Flash Layout 应单独文档说明。

地址必须统一定义。

不要在多个文件散落 Magic Number。

如果最终发现真实 Board Flash 规格不同：

通过 linker / configuration 修改。

---

# 50. STM32 APP Validation

Bootloader 跳 APP 前至少检查：

```text
image valid metadata
size
CRC
stack pointer range
reset vector range
```

不要盲跳。

---

# 51. Portable Protocol Core

Host 和 STM32 不需要共用完全相同代码，但 Wire Protocol 必须完全一致。

最好把：

```text
CRC
command ID
wire constants
error definitions
```

设计成容易同步维护。

protocol.md 是：

**Source of Truth。**

---

# 52. Unit Tests

必须有真正的 Unit Tests。

至少覆盖：

## Common

```text
CRC32
byte order
ring buffer
error helpers
```

## Protocol

```text
encode
decode
fragmented frame
multiple frames
garbage
invalid length
CRC error
resync
sequence
```

## Device

```text
state transition
invalid transition
timeout
disconnect
```

## Upgrade

```text
normal state flow
retry
timeout
resume
NACK
verify failure
```

## IPC

```text
encode
decode
partial message
malformed request
```

---

# 53. Parser Stress Test

写随机分片测试。

例如生成：

```text
10000 frames
```

然后随机拆成：

```text
1 byte
2 bytes
7 bytes
50 bytes
...
```

喂给 parser。

最终必须恢复全部 Frame。

同时注入：

```text
garbage
CRC error
```

验证 resynchronization。

随机种子固定，保证 CI 可复现。

---

# 54. Fuzz Target

如果使用 clang：

增加一个可选：

```text
libFuzzer
```

target：

```text
protocol parser
IPC parser
```

但 fuzzing 不是核心构建依赖。

例如：

```text
ENABLE_FUZZING
```

默认关闭。

---

# 55. Integration Tests

必须启动：

```text
device-sim
devmgrd
devctl
```

真正走：

```text
PTY
protocol
daemon
IPC
CLI
```

完成：

```text
PING
INFO
HEALTH
TELEMETRY
UPGRADE
VERIFY
REBOOT
```

不能把 integration test 写成假的 mock。

---

# 56. End-to-End Upgrade Test

自动：

1. 创建 test firmware
2. 启动 simulator
3. 启动 daemon
4. devctl 查询旧版本
5. firmware upgrade
6. Simulator 写 flash
7. verify
8. reboot
9. reconnect
10. devctl 查询版本
11. 确认新版本

必须作为自动测试。

---

# 57. Fault Recovery Integration Test

至少实现自动场景：

```text
lost ACK
delayed response
CRC corruption
disconnect during upgrade
resume
```

正常情况下：

最终 upgrade 成功。

---

# 58. Sanitizer

提供：

```text
ENABLE_ASAN
ENABLE_UBSAN
```

至少支持：

```text
AddressSanitizer
UndefinedBehaviorSanitizer
```

执行全部 host tests。

必须修复 sanitizer 报告。

---

# 59. Valgrind

提供：

```text
scripts/run-valgrind.sh
```

至少运行：

```text
unit tests
basic simulator/daemon path
```

目标：

```text
definitely lost: 0
invalid read: 0
invalid write: 0
```

---

# 60. Coverage

提供可选：

```text
ENABLE_COVERAGE
```

使用：

```text
gcov
```

如果环境存在 lcov：

允许生成 HTML。

不要为了追求 100% coverage 写无意义测试。

---

# 61. CMake

必须采用现代 CMake。

不要把所有内容：

```text
file(GLOB ...)
```

粗暴处理。

合理使用：

```text
add_library
add_executable
target_sources
target_include_directories
target_compile_options
target_link_libraries
add_subdirectory
enable_testing
add_test
```

要求：

```text
cmake -S . -B build
cmake --build build -j
ctest --test-dir build
```

可以正常工作。

---

# 62. CMake Presets

提供：

```text
CMakePresets.json
```

至少：

```text
debug
release
asan
coverage
```

例如：

```bash
cmake --preset debug
cmake --build --preset debug
```

---

# 63. Makefile

顶层可以有一个非常薄的 Makefile。

只是提供：

```text
make build
make test
make demo
make clean
```

内部调用 CMake。

不要再维护第二套 Build System。

---

# 64. GitHub Actions

创建：

```text
.github/workflows/ci.yml
```

至少执行：

```text
Configure
Build
Unit Tests
Integration Tests
ASan / UBSan
```

Release build：

可以额外执行。

如果某项 CI 因 runner 环境无法稳定运行：

解释原因并设计可靠替代方案。

---

# 65. systemd

提供：

```text
packaging/systemd/devmgrd.service
```

正确采用：

foreground daemon。

至少考虑：

```text
ExecStart
Restart
RestartSec
RuntimeDirectory
```

不要把 root 作为默认强制条件。

---

# 66. udev

提供：

```text
packaging/udev/
```

示例规则。

目的：

给真实 USB-UART 创建稳定设备名。

但是规则必须是：

**example**

因为 VID/PID 依赖真实硬件。

不要随便伪造用户真实 VID/PID。

---

# 67. Development Demo

必须提供：

```bash
./scripts/run-demo.sh
```

最好能够：

自动：

```text
build
start simulator
start daemon
run devctl
info
ping
health
firmware upgrade
verify
stop processes
```

Demo：

不得需要真实 STM32。

不得需要 root。

---

# 68. Benchmark

提供：

```bash
devctl benchmark
```

或者：

```text
scripts/benchmark.sh
```

能够测试至少：

```text
request latency
frames/sec
bytes/sec
retry count
```

如果结果受环境影响：

正常。

不要在 README 编造性能数字。

---

# 69. README

README.md 应该达到 GitHub Portfolio 水平。

必须包含：

```text
Project Overview
Why this project exists
Architecture
Key Features
Demo
Build
Testing
Fault Injection
Firmware Upgrade
Linux APIs Used
Project Structure
Design Decisions
Known Limitations
Roadmap
```

README 顶部应快速告诉招聘者：

这个项目到底解决什么问题。

不要写几十屏流水账才进入重点。

---

# 70. README.zh-CN

另外生成：

```text
README.zh-CN.md
```

用于学习和中文介绍。

---

# 71. Architecture Document

`docs/architecture.md`

必须包括：

```text
component diagram
data flow
event loop
thread model
resource ownership
failure propagation
```

可以使用：

```text
Mermaid
```

---

# 72. State Machine Document

`docs/state-machines.md`

绘制：

```text
Device State Machine
Upgrade State Machine
Connection Recovery
```

不要只有文字。

使用 Mermaid。

---

# 73. Protocol Document

`docs/protocol.md`

必须足够详细，使另一个工程师：

**只看 protocol.md 就能写一个兼容设备。**

包括：

```text
frame layout
endianness
CRC
sequence
commands
error codes
timeouts
retry
state requirements
firmware upgrade flow
examples
```

---

# 74. Reliability Document

`docs/reliability.md`

解释：

```text
partial IO
packet corruption
disconnect
retry
duplicate packet
idempotency
firmware resume
timeouts
device reboot
daemon shutdown
```

这是重要的面试材料。

---

# 75. Testing Document

`docs/testing.md`

解释：

```text
unit
integration
stress
fault injection
fuzzing
sanitizer
valgrind
coverage
CI
```

同时告诉用户：

每类测试在验证什么。

---

# 76. Debugging Document

`docs/debugging.md`

要求至少覆盖：

```text
gdb
strace
valgrind
ASan
UBSan
hexdump
journalctl
```

针对本项目提供实际示例。

例如：

```text
在哪里给 protocol parser 下断点
如何观察 epoll
如何检查 fd 泄漏
如何定位 CRC error
```

---

# 77. 中文 Learning Guide

这是整个项目非常重要的交付。

生成：

# docs/learning-guide.zh-CN.md

它不是 README 翻译。

它是一份：

**这个项目配套的 Linux C 教材。**

章节至少包括：

```text
01 项目整体架构
02 Linux 文件描述符
03 open/read/write
04 termios
05 非阻塞 IO
06 epoll
07 timerfd
08 signalfd
09 eventfd
10 pthread
11 mutex / condition variable
12 Ring Buffer
13 流式协议解析
14 Binary Protocol
15 CRC32
16 状态机
17 Unix Domain Socket
18 Linux daemon
19 mmap
20 CMake
21 Unit Test
22 Integration Test
23 Sanitizer
24 Valgrind
25 Fault Injection
26 Firmware Upgrade
27 STM32 Port
28 CI
```

---

# 78. 每个 Learning Guide 章节

每一章必须回答：

```text
这个知识是什么？
为什么存在？
本项目为什么需要？
对应哪些文件？
核心函数在哪里？
调用链是什么？
常见错误是什么？
本项目是怎么避免的？
我应该在哪里打断点？
我应该怎么实验？
我应该删除哪段代码重新实现？
面试官可能怎么问？
```

---

# 79. Ownership Plan

生成：

```text
docs/ownership-plan.zh-CN.md
```

目标：

帮助一个初学者把 AI 生成项目真正转化为自己的能力。

不要建议：

“把 README 看一遍”。

制定：

**拆解 + 重写计划。**

例如：

### Stage 1

运行整个项目。

### Stage 2

画架构。

### Stage 3

阅读：

```text
crc32
ring buffer
frame
parser
```

### Stage 4

删除 parser 自己重写。

### Stage 5

自己重写 serial transport。

### Stage 6

自己重写 upgrade state machine。

### Stage 7

自己实现一个新功能。

---

# 80. Learning Roadmap

生成：

```text
docs/learning-roadmap.zh-CN.md
```

设计大约：

**6～8 周学习路线。**

不要要求每天学习十小时。

每周：

```text
学习目标
代码范围
Linux API
实验
重写任务
调试任务
输出成果
面试问题
```

---

# 81. Interview Guide

生成：

```text
docs/interview-guide.zh-CN.md
```

至少提供：

**40 个围绕本项目的技术面试问题。**

不能只是：

“什么是 epoll？”

应该结合项目：

例如：

```text
为什么 daemon 使用 epoll 而不是一个设备一个线程？
read 为什么不能等价于收到一个协议帧？
你的 parser 如何从 CRC Error 中恢复？
为什么 timeout 使用 CLOCK_MONOTONIC？
为什么 Upgrade State Machine 独立于 Device State Machine？
如果 ACK 丢了，重复发送 FW_DATA 会不会重复写 Flash？
怎么保证 FW_DATA 幂等？
为什么用了 eventfd？
为什么 IPC 选择 Unix Domain Socket？
升级一半 daemon 崩溃怎么办？
```

同时提供高质量参考答案。

---

# 82. Resume Guide

在：

```text
docs/portfolio.zh-CN.md
```

提供：

```text
项目简介
核心技术
简历描述建议
面试讲解路径
```

但是：

禁止编造：

```text
性能提升 80%
稳定性提升 60%
10 万 QPS
```

任何数据：

必须来自真实 benchmark/test。

没有测量：

就不要写数字。

---

# 83. AGENTS.md

项目根目录必须创建：

```text
AGENTS.md
```

作为未来 Codex 继续开发此项目的永久工程规范。

至少记录：

```text
architecture rules
coding style
build commands
test commands
no-toy policy
dependency policy
error handling policy
documentation requirements
code review checklist
```

以后任何 Codex Session 都应该容易继续维护本项目。

---

# 84. ADR

建议建立：

```text
docs/adr/
```

至少记录几个关键 Architecture Decision：

```text
Why C17
Why epoll
Why Unix Domain Socket
Why PTY simulator
Why binary protocol
Why single reactor + worker
```

使用简单 ADR 格式：

```text
Context
Decision
Alternatives
Consequences
```

---

# 85. 不要隐藏复杂度

非常重要：

不要因为你能快速生成代码，就把：

```text
protocol
event loop
IPC
state machine
```

隐藏进第三方库。

这个项目的主要价值就是：

理解这些东西。

---

# 86. 不要为了代码量写代码

不要刻意达到：

```text
10000 lines
20000 lines
```

代码量不是目标。

但是项目不能只有：

```text
1000 行
```

就声称完成了一个完整 Linux Device Framework。

我希望最终规模大致达到：

**数千到约一万行具有实际意义的 C / Test / Build Code**

但不要：

```text
重复代码
大量模板
生成代码
无意义封装
```

来增加 LOC。

---

# 87. 代码必须可解释

优先：

```text
Readable
Explicit
Testable
Debuggable
```

而不是：

```text
Clever
Magic
Macro-heavy
```

这个工程最终必须适合学生：

逐模块阅读并重新实现。

---

# 88. 不允许留下核心 TODO

完成项目时：

核心链路不能存在：

```text
TODO: implement later
stub
return success
fake implementation
```

对于确实属于 Future Work 的功能：

写入：

```text
ROADMAP
```

而不是假装已经实现。

---

# 89. Real Hardware 与 Host Core 分离

真实 STM32 无法在你的环境中物理验证时：

不要假装已经验证。

需要明确标记：

```text
Host + Simulator: verified
STM32 reference firmware: build verified / unverified
Hardware behavior: requires physical validation
```

绝不能伪造硬件测试结果。

---

# 90. 工作流程

现在开始执行时：

首先检查仓库状态。

如果为空：

从零开始。

先创建：

```text
PLAN.md
```

里面记录：

```text
Architecture
Milestones
Dependencies
Risks
Acceptance Criteria
```

但是：

# 不要只输出 PLAN 然后停止。

Plan 完成后直接进入实施。

除非存在真正无法自行解决的阻塞问题，否则不要反复询问我：

“要不要继续？”

你应该：

```text
plan
implement
build
test
inspect failures
fix
retest
review
```

持续执行。

---

# 91. 推荐实施顺序

严格优先：

## Phase 1

Repository / Build Foundation

```text
CMake
common
logging
error
CRC
ring buffer
tests
```

## Phase 2

Protocol

```text
frame
codec
parser
tests
stress
```

## Phase 3

Transport + PTY

```text
serial
PTY
device-sim
```

## Phase 4

Device Session

```text
state machine
request/response
timeout
retry
```

## Phase 5

Daemon + IPC

```text
epoll
UDS
devmgrd
devctl
```

## Phase 6

Telemetry + Statistics

## Phase 7

Firmware Upgrade

## Phase 8

Fault Injection + Resume

## Phase 9

Integration Tests

## Phase 10

ASan / UBSan / Valgrind

## Phase 11

CI / systemd / packaging

## Phase 12

STM32 reference port

## Phase 13

Documentation

## Phase 14

Full Audit

---

# 92. 每个 Phase 的要求

每个阶段都必须：

```text
build
test
fix warnings
review interfaces
```

不要：

先生成整个 Repository，

最后才第一次：

```text
cmake
```

---

# 93. 自我 Review

完成主要代码后，至少进行一次独立 Code Review Pass。

检查：

```text
memory leaks
fd leaks
incorrect state transition
error handling
integer overflow
buffer bounds
partial IO
EINTR
EAGAIN
use after free
deadlock
race conditions
protocol desync
upgrade idempotency
shutdown behavior
```

发现问题：

直接修复。

不要只列问题。

---

# 94. Architecture Review

再进行一次架构 Review。

寻找：

```text
unnecessary coupling
giant module
duplicate abstraction
wrong ownership
global state
poor interface
overengineering
underengineering
```

如果设计明显可以改进：

重构。

---

# 95. Testing Review

检查测试是否真的覆盖：

```text
happy path
failure path
boundary
recovery
```

测试不能只有：

```text
assert(1 == 1)
```

或者大量 mock，完全没走真实流程。

---

# 96. 完成条件 Definition of Done

只有满足以下条件，项目核心部分才能称为完成。

## Build

```text
Debug build passes
Release build passes
zero important compiler warnings
```

## Unit Tests

```text
all pass
```

## Integration Tests

```text
all pass
```

## Sanitizer

```text
ASan passes
UBSan passes
```

## Demo

```text
run-demo.sh returns success
```

## Normal Upgrade

必须成功。

## Disconnect Upgrade

必须可以恢复。

## Lost ACK

必须可以恢复。

## CRC Error

必须可以检测并正确处理。

## Daemon Shutdown

SIGTERM 后：

资源正常释放。

## Documentation

核心文档必须存在且与实际代码一致。

---

# 97. Demo 最终必须展示

类似：

```text
$ ./scripts/run-demo.sh

[1/8] Starting device simulator...
PTY: /dev/pts/5

[2/8] Starting devmgrd...

[3/8] Discovering devices...
sim0 CONNECTED

[4/8] Device information...
Model: STM32-SIM
Firmware: 1.0.0
Bootloader: 1.0.0

[5/8] Ping...
RTT: 1.8 ms

[6/8] Firmware upgrade...
[████████████████████] 100%

Verify: OK
Reboot: OK

[7/8] Reconnect...
Firmware: 1.1.0

[8/8] Tests complete.

PASS
```

实际输出设计由你决定。

但体验应该达到类似完整度。

---

# 98. Fault Demo

最好另外提供：

```bash
./scripts/demo-fault-recovery.sh
```

例如：

```text
upgrade 42%
device disconnected

reconnecting...

device online
resume offset 43008

upgrade resumed

100%
verify OK
```

这将成为项目的重要展示点。

---

# 99. 最终报告

完成后不要只回答：

“Done”。

给我一份工程报告：

## Implemented

实际完成了什么。

## Architecture

核心模块如何协作。

## Build

使用什么命令。

## Test

实际执行了哪些测试。

## Test Results

真实输出摘要。

## Sanitizers

是否实际运行。

## Limitations

当前真实限制。

## Hardware

哪些部分还需要 STM32 实机验证。

## Learning Order

我应该先阅读哪些文件。

## First Rewrite Task

我应该最先删除并重写哪个模块。

---

# 100. 最重要的项目哲学

这个项目的最终价值不是：

> AI 写了一堆代码。

而应该是：

> AI 构建了一个完整、合理、可运行、可测试的参考工程；我可以通过运行、调试、阅读、修改、删除、重写和扩展，把它逐步真正转化成自己的 Linux C 工程能力。

因此你的实现必须：

```text
真实
完整
清晰
可靠
可测试
可调试
可学习
可扩展
```

优先级：

```text
Correctness
    ↓
Reliability
    ↓
Architecture
    ↓
Testability
    ↓
Readability
    ↓
Performance
    ↓
Feature Count
```

不要为了 Feature Count 牺牲前面的东西。

---

# 101. 立即开始

现在：

1. 检查当前 Repository。
2. 创建 `PLAN.md`。
3. 设计架构。
4. 写核心 ADR。
5. 建立 CMake 工程。
6. 从最基础的 common/protocol/test 开始实现。
7. 每完成一个阶段就实际 Build/Test。
8. 遇到失败自行分析和修复。
9. 继续实现后续阶段。
10. 最后完成全量测试、自我 Review 和文档。
11. 不要停留在伪代码。
12. 不要只给建议。
13. 不要只生成计划。
14. 直接修改当前 Repository 并完成尽可能完整的实现。

如果当前环境缺少某个非核心工具：

不要因此停止整个项目。

应：

- 完成不依赖它的部分；
- 给出明确安装/验证方式；
- 不伪造测试结果；
- 继续推进其他模块。

如果某项实现存在多个合理方案：

自行进行工程权衡。

将关键选择记录到 ADR。

只有遇到会根本改变项目方向、且无法根据以上要求合理决策的问题时，才需要询问我。

除此之外：

**优先行动，持续实现，持续验证。**

最终目标不是“代码生成完成”。

最终目标是：

# 一个我能够在秋招简历上认真写出来、面试时有大量技术内容可以深入讨论、并且值得我花数周系统学习和重写的 Linux C 嵌入式设备管理工程。