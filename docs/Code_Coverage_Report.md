# Code Coverage Report

Traffic Management Simulation, Team 13. Measured 16 November 2026 on the final submission code.

## Summary

| Scope | Line coverage | Branch coverage (taken at least once) |
|---|---|---|
| All 11 production source files | **84.56%** (1,391 / 1,645) | 48.42% (1,257 / 2,596) |
| Excluding `src/main.cpp` | **93.25%** (1,327 / 1,423) | 59.34% (1,191 / 2,007) |

The previous measurement (SRS v2.1, 9 Nov 2026) was 78.16% line and 45.42% branch coverage.

## Per file

| Source file | Lines | Line coverage | Branches | Branch coverage |
|---|---|---|---|---|
| `src/vehicle_queue.cpp` | 75 | 100.00% | 28 | 96.43% |
| `src/simulation_engine.cpp` | 264 | 98.48% | 529 | 56.71% |
| `src/signal_comparison.cpp` | 165 | 98.18% | 360 | 54.44% |
| `src/config.cpp` | 79 | 97.47% | 252 | 53.57% |
| `src/signal_controller.cpp` | 234 | 95.30% | 146 | 86.99% |
| `src/vehicle_generator.cpp` | 34 | 94.12% | 33 | 72.73% |
| `src/event_logger.cpp` | 50 | 90.00% | 91 | 56.04% |
| `src/emergency_handler.cpp` | 163 | 88.34% | 110 | 76.36% |
| `src/simulation.cpp` | 292 | 87.33% | 324 | 56.48% |
| `src/console_menu.cpp` | 67 | 80.60% | 134 | 47.76% |
| `src/main.cpp` | 222 | 28.83% | 589 | 11.21% |

## Method

- Tool: gcov from MinGW GCC 6.3.0, built with `-std=c++11 -O0 --coverage`.
- Each production source is compiled once with coverage. All 15 C++ test executables link against the same object files, so counts add up across suites.
- Executed: the 15 C++ suites in `tests/` and one application run through the menu (run, reset, step with invalid and valid counts, quiet run, compare, export, exit).
- `src/analytics_main.cpp` is excluded: the Node API builds and runs it, and the API tests use an injected runner.
- Branch figures count compiler-generated branches, including those inside standard-library calls. They are not MC/DC coverage.

## Notes

- `src/main.cpp` is low because the legacy command-line modes (`--demo`, `--menu`, `[ticks] [mode]`) are not part of the test register. The configured menu path that the SRS specifies is exercised.
- All four modules named in NFR-TMS-04 (queue, signal, emergency, statistics) are above 88% line coverage, and every public function in them has a unit test (TC-68).
