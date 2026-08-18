# Performance and Measurement

Correctness and recovery precede throughput. No fixed performance claim is
published because PTY, VM/WSL scheduling, CPU, sanitizer, UART baud, chunk size,
and fault settings materially change results.

```sh
# Start simulator and daemon first
./scripts/benchmark.sh 1000
/usr/bin/time -v ./scripts/run-demo.sh
perf stat -e task-clock,context-switches,cpu-migrations,page-faults \
  build/debug/devctl ping
```

Record commit, kernel, CPU, native/VM/WSL, build type, iteration count, baud,
fault flags, firmware size, and sanitizer state. Report median/p95 latency only
from saved raw samples; the simple script reports observed aggregate/average.

Expected constraints: 115200 baud bounds real wire throughput near 11.5 kB/s
before framing/retries; PTY has no UART baud pacing and must not be presented as
hardware throughput. One in-flight request per device favors ordering over
pipelined throughput. Chunk size trades frame overhead against retransmit cost.
Use parser stress tests for CPU/memory safety, not latency claims.

