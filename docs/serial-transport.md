# Serial and PTY Transport

The protocol consumes an arbitrary byte stream and knows nothing about UART.
`devmgr_transport` currently opens serial devices and PTY slaves with the same
nonblocking read/write API; TCP, USB, or CAN can be added without changing frame
code.

The serial setup uses noncanonical raw mode: input bytes are delivered without
line editing, echo, signal characters, CR/LF translation, or output processing.
`VMIN=0` and `VTIME=0` are intentional because readiness and deadlines belong to
the event loop. Blocking inside termios would split timeout ownership. The
supported baseline is 115200 baud, 8 data bits, no parity, one stop bit, and no
software or hardware flow control.

Every descriptor is opened `O_NOCTTY | O_NONBLOCK | O_CLOEXEC`. Callers must
drain reads until EAGAIN, retain unwritten output after partial writes, retry
EINTR, and treat other errors or EOF according to session state.

The simulator calls `posix_openpt`, `grantpt`, `unlockpt`, and `ptsname` directly.
It prints the slave path for the daemon; `socat` and root access are unnecessary.

