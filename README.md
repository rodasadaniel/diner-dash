# Diner Dash — Discrete-Event Restaurant Order Simulation

A discrete-event simulation of a restaurant kitchen, written in modern C++
(C++17). Orders arrive over time, wait for a free cook, are prepared, and
leave the system. Two dispatching strategies are simulated on **identical
seeded workloads** and compared head-to-head:

- **FIFO** — a strict first-in, first-out baseline.
- **Priority** — shortest-processing-time first, tempered by wait-time aging
  and order value.

The headline finding, measured on a 100,000-order benchmark: the priority
strategy cuts **average order wait time by 34.5%** and **median wait by
64.7%** relative to FIFO, at no cost to total throughput.

## Design

### Event engine

The simulation is a classic discrete-event engine (`src/simulation.cpp`).
State changes happen only at two kinds of events — **order arrivals** and
**order completions** — which are held in a time-ordered priority queue. The
engine repeatedly pops the earliest event, advances the simulated clock to
it, and updates state:

- *Arrival*: if a cook is free, the order starts immediately; otherwise it
  enters the scheduler's waiting room.
- *Completion*: the cook is freed and immediately dispatched to the next
  order chosen by the scheduler, if any.

The engine is **work-conserving** (a cook never idles while orders wait) and
fully deterministic: the same workload always produces the same results.
Ties at identical timestamps are broken stably (completions before arrivals,
then by insertion sequence), so runs are bit-for-bit reproducible.

### Workload model

`WorkloadGenerator` produces reproducible synthetic order streams with a
fixed seed (`std::mt19937_64`):

- **Arrivals** follow a Poisson process (exponential inter-arrival gaps,
  mean 99 s), giving a kitchen utilisation of ≈ 0.85 with 6 cooks.
- **Preparation times** are a two-component mixture: 70% standard orders
  (120–420 s) and 30% large orders (600–1500 s). This variance is what makes
  scheduling strategy matter.
- **Order value** ($8–$80) correlates loosely with preparation time.

Because both schedulers replay the *same* generated vector of orders, every
difference in results is attributable to the strategy alone.

### Schedulers

Both strategies implement a small `Scheduler` interface (`Push` /
`PopNext`) consumed by the engine.

**FIFO** keeps waiting orders in a deque and dispatches the oldest. It is
perfectly fair in arrival order but blind to order size: a 2-minute order
can sit behind a 25-minute one.

**Priority** dispatches the order minimising

```
score(order, now) = prep_time − 0.5 · (now − arrival_time) − 1.0 · value
```

i.e. short jobs first (the classic SPT rule, which minimises mean wait),
with two corrections: an **aging** term that steadily promotes orders the
longer they wait, and a modest **value** term favouring higher-value
orders. Since `now` is identical for all candidates at a dispatch instant,
minimising the score is equivalent to minimising the static key
`prep_time + 0.5 · arrival_time − value`, computed once at enqueue time.
The waiting room is therefore a binary heap with O(log n) push/pop that
still implements exact linear aging.

The aging weight was tuned empirically (0.1 → 1.0): weaker aging improves
mean wait slightly but lets the unluckiest large orders wait over twice as
long as under FIFO; 0.5 keeps nearly all of the mean improvement while
bounding the worst case.

### Statistics

Wait times are collected in a sample-storing `Distribution` (mean, standard
deviation, interpolated percentiles); turnaround times use an online
Welford accumulator (`RunningStats`). Both are unit-tested against
hand-computed values.

## Building

Requires a C++17 compiler and CMake ≥ 3.16. GoogleTest is fetched
automatically at configure time (FetchContent).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Running the benchmark

```bash
./build/diner_dash                        # 100,000 orders, 6 cooks, seed 42
./build/diner_dash --orders 500000 --seed 7 --cooks 8
./build/diner_dash --scheduler fifo       # single-strategy run
```

Each run prints per-scheduler statistics — average / p50 / p95 / max wait,
average turnaround, throughput, events processed, execution time and event
rate — followed by percentage deltas of Priority vs. FIFO.

## Running the tests

```bash
./build/diner_tests        # or: ctest --test-dir build
```

The suite contains **68 GoogleTest cases**, all passing, covering:

- statistics primitives against hand-computed values (incl. numerical
  stability at large offsets),
- FIFO ordering, sizing, and interleaved push/pop behaviour,
- priority ordering: SPT, aging (including aging outweighing a prep-time
  difference), value preference, tie-breaking, and bulk heap-order
  properties over 200 generated orders,
- workload reproducibility (same seed ⇒ identical stream), arrival
  monotonicity, mixture bounds, and mixture proportions,
- end-to-end simulation: hand-computed wait times for contended scenarios,
  conservation (every generated order is served), event accounting,
  determinism, and the priority strategy's average-wait advantage on a
  20,000-order generated workload.

## Results

Benchmark: 100,000 orders (200,000 events), 6 cooks, seed 42. Simulation
metrics are deterministic and exactly reproducible; wall-clock figures vary
with machine load (the full run took ≈ 0.07–0.29 s across repeated runs on
the 2-core build VM, i.e. roughly 0.7M–3.1M events/second).

| Metric                    | FIFO      | Priority  | Δ vs FIFO |
|---------------------------|-----------|-----------|-----------|
| Average wait              | 282.87 s  | 185.23 s  | **−34.5%** |
| Median (p50) wait         | 103.46 s  | 36.57 s   | **−64.7%** |
| p95 wait                  | 1140.04 s | 903.07 s  | **−20.8%** |
| Max wait                  | 3535.76 s | 4280.25 s | +21.1% |
| Average turnaround        | 786.49 s  | 688.85 s  | **−12.4%** |
| Throughput (orders/sim-h) | 36.4      | 36.4      | 0.0% |
| Makespan                  | 9,883,093 s | 9,883,093 s | 0.0% |

A scale check at 500,000 orders (1,000,000 events, seed 7) shows the same
pattern: average wait −34.2%, average turnaround −12.7%.

**Reading the results.** Both schedulers are work-conserving, so they
complete the same total work in the same makespan — overall throughput is
identical by construction. What the priority strategy changes is *who waits
and for how long*: short orders stop being trapped behind large ones, which
collapses the median wait and pulls the whole wait distribution down. The
honest trade-off is the worst case: even with aging, the single unluckiest
(large) order waits ~21% longer than FIFO's worst case. The aging weight is
the knob that trades mean performance against that tail, and 0.5 was chosen
to keep the tail bounded while retaining most of the mean gain.

## Project structure

```
include/diner/   order.h, scheduler.h, stats.h, workload.h, simulation.h
src/             scheduler.cpp, stats.cpp, workload.cpp, simulation.cpp,
                 main.cpp (benchmark CLI)
tests/           test_stats.cpp, test_scheduler.cpp, test_workload.cpp,
                 test_simulation.cpp
```

## Limitations and future work

- Cooks are identical and interchangeable; real kitchens have specialised
  stations (grill, fryer, cold prep) with order-dependent routing.
- Preparation times are known exactly at dispatch. An estimated-prep
  variant (dispatch on predictions with error) would test robustness of the
  priority rule.
- The workload is stationary; modelling lunch/dinner rush profiles
  (time-varying arrival rates) is a natural extension.
