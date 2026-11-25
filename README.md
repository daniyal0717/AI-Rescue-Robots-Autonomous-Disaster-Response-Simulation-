# AI Rescue Robots - Disaster-Response Simulator 

Data Structures Lab, Fall 2026 (DS, AI), NUCES Islamabad.
Group: Muhammad Umair Anjum (25i-2502), Daniyal Khalid (25i-2638), Zulqarnain Rajper (25i-2512).

A deterministic, discrete-time simulator in C++17. A fleet of robots serves rescue requests on a map
whose roads are blocked and reopened during the run. **No STL containers are used**: `Vector`, hash
table (`IntMap`), binary heap, indexed heap, string buffer and merge sort are in `src/containers.h`.

## Build

```
make                                   # or:
g++ -std=c++17 -O2 -Wall -Wextra -o rescue src/*.cpp
```
On Windows with MinGW the program is `rescue.exe`. Any C++17 compiler works; no libraries needed.

## Run

| Command | What it does |
|---|---|
| `./rescue data/scenario_provided.txt` | Run the whole simulation. Writes `output/<name>_record.csv` and `output/<name>_stats.txt`, prints the statistics |
| `./rescue FILE --out DIR` | Same, with another output folder (created if missing) |
| `./rescue FILE --validate` | Load and validate only; prints every problem found, exit code 1 if invalid |
| `./rescue FILE --step N` | Run N steps, then print environment, robots and requests (rows are echoed) |
| `./rescue FILE --interactive` | Shell: `step [n]`, `run`, `env`, `robots`, `requests [all\|pending\|active\|done\|failed]`, `stats`, `quit` |
| `--echo` / `--quiet` | Echo every record to the console / hide the final statistics |

Tests: `sh tests/run_tests.sh` (24 checks: invalid inputs, every scenario, determinism, reroute,
wait/release, priority order, events) and `tests/test_containers.cpp` (container unit tests).
Performance: `python tools/benchmark.py ./rescue output/performance.csv <scenario files...>`.
Generator for larger maps: `python tools/gen_scenario.py <locations> <out.txt> [seed]`.

## Folder layout

```
src/  containers.h types.h scenario.* environment.* robot.h rescue_request.h
      simulation.h/.cpp (loop, events)  movement.cpp (robot state machine)
      coordinator.cpp (assignment, timeouts, charging)  report.cpp  logger.*  main.cpp
data/ scenario_provided.txt  scenario_1_easy  scenario_2_medium  scenario_3_hard  scenario_4_stress
      generated/gen_{130,260,520,1040,2080}.txt  (size series for the performance analysis)
tests/ run_tests.sh  test_*.txt  bad/*.txt  test_containers.cpp
output/ record + statistics of the five scenarios, performance.csv
report.pdf
```

## Scenarios (increasing difficulty)

| File | Locations | Roads (blocked) | Robots | Requests | Events | Steps |
|---|---|---|---|---|---|---|
| scenario_1_easy | 14 | 22 (2) | 5 | 12 | 6 | 80 |
| scenario_2_medium | 30 | 60 (4) | 8 | 30 | 15 | 150 |
| scenario_provided (minimum working) | 56 | 117 (7) | 12 | 60 | 20 | 250 |
| scenario_3_hard | 60 | 125 (10) | 14 | 80 | 39 | 300 |
| scenario_4_stress (required stress scenario) | 130 | 280 (22) | 26 | 140 | 79 | 520 |

## Input format

Exactly the format of `scenario_provided.txt` (comments `#`, blank lines, `KEYWORD count` headers
followed by `count` records, `END`). Roads are **undirected**. Any file runs with no source change.
Optional extra `PARAM` keys (defaults make the provided file work unchanged):

| PARAM | Default | Meaning |
|---|---|---|
| `request_timeout` | 150 | A request unassigned this many steps after its report time is FAILED |
| `reroute_wait` | 5 | Steps a robot waits at a node for a blocked route before releasing its request |
| `low_battery_pct` | 40 | Idle robots below this % of max battery go to charge |
| `max_releases` | 3 | Releases (route/battery failures) after which a request is FAILED |

Validation (errors are listed with line numbers, nothing crashes): record count different from the
header (too few or too many), missing/duplicate section, missing `END`, unknown keyword/category/
capability/type, bad numbers, duplicate ids, unknown location in a road/robot/request, a road to
itself or with cost <= 0, urgency outside 1-5, duration <= 0, initial battery above maximum,
events naming an unknown road/request/robot, a `STATION_*` event on a non-CHARGING location.
Requests/events out of time order are stable-sorted with a warning.

## Output format

`<name>_record.csv`: `step,kind,subject,detail` - one row per change, so the run can be reconstructed.
Subjects: `Q<id>` request, `R<id>` robot, `E<id>` road, `L<id>` location.

| kind | meaning |
|---|---|
| `INIT` `FINAL` | sizes at start; totals at the end |
| `REPORT` | request enters the system |
| `REQUEST_STATE` `ROBOT_STATE` | every lifecycle / state change (`from->to`, reason) |
| `CONFLICT` `ASSIGN` | several candidate robots and the winner; request given to a robot (distance, battery) |
| `MOVE` `ARRIVE` | robot enters a road (cost, ETA, battery left) / reaches a node |
| `START_WORK` `COMPLETE` | work begins (response time) / request completed (total time, battery) |
| `REROUTE` `WAIT` `RELEASE` | new route after a blocked road / no route, waiting / request given back to PENDING |
| `CHARGE` `RECHARGE` | robot sent to a charger / charging session ended (battery gained) |
| `BLOCK` `UNBLOCK` `STATION` `URGENCY` `ROBOT_EVENT` | environmental and operational events |
| `FAIL` | request failed (reason) |
| `REJECT` | refused action: invalid transition, no-op event, cannot depart for lack of battery, invariant violation |

`<name>_stats.txt`: requests completed/failed/remaining, average and maximum response time, average
completion time, robot utilisation (overall and per robot), travel cost, battery used/recharged,
assignments, conflicts, reassignments, reroutes, rejected actions, invariant violations, Dijkstra
runs/cache hits, execution time.

## Rules (summary - details in the report)

* **Time:** one step = one unit of simulated time; with `speed 1` a road of cost *c* takes *ceil(c/speed)* steps.
* **Order inside a step (always the same):** 1 events (file order) -> 2 requests reported (file order)
  -> 3 robots by ascending id -> 4 request timeouts -> 5 assignment -> 6 low-battery robots sent to charge
  -> 7 statistics and invariant check.
* **Route criterion:** minimum total cost over open roads (Dijkstra, ties to the lower location id).
* **Priority:** urgency (high first), then report time, then request id. **Robot choice:** nearest
  capable AVAILABLE robot with enough battery (trip + work + trip to a working charger); ties go to a specialised
  robot before a GENERAL one, then the lower id. More than one candidate counts as a *conflict*.
* **Battery:** `battery_per_cost_unit x cost` paid when entering a road; `battery_per_work_step` per working step;
  `recharge_per_step` per step at a working charger.
* **Blocked road while travelling:** a robot on a road finishes it; at the next node it reroutes (REROUTE); if no route
  exists it waits `reroute_wait` steps, then releases the request (back to PENDING).
* **Termination:** `max_steps`, or earlier when every event has been applied, every request is COMPLETED or FAILED and all robots are AVAILABLE/UNAVAILABLE.
