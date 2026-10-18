# HW3 Scheduler Simulator

Compile:

```bash
g++ -std=c++17 -Wall -Wextra -o simulator simulator.cpp
```

Run one algorithm:

```bash
./simulator --workload sample.csv --algo fcfs
```

Compare all five algorithms:

```bash
./simulator --workload sample.csv --compare
```

