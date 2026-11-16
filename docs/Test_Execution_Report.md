# Test Execution Report

Traffic Management Simulation, Team 13. Final execution: 16 November 2026, following Test Plan v2.2.

## Result

| Measure | Value |
|---|---|
| Active test cases | 73 (TC-01 to TC-80, minus 7 removed in v2.1) |
| Passed | **73** |
| Failed / Blocked / Not Run | 0 / 0 / 0 |
| C++ test executables | 15 / 15 passed |
| Node API tests | 4 / 4 passed |
| React component tests | 1 / 1 passed |
| Line coverage | 84.56% (see `Code_Coverage_Report.md`) |

Before this run the register stood at 47 Passed, 4 Blocked and 22 Not Run (SAD v2.1 Appendix A). Every Not Run and Blocked case has now been executed.

## Environments

| Environment | Toolchain | What ran |
|---|---|---|
| Local Windows 11 laptop | MinGW GCC 6.3, Node 22 | `build_and_test.ps1` (15 C++ suites, menu smoke run) and `npm test`. Logs in `test-evidence/` |
| GitHub Actions, Ubuntu (ubuntu-latest) | g++ 13 | All C++ suites, register cases, step-menu check, `-Werror` build, AddressSanitizer/UBSan runs of the unit suites, the register cases and the full application |
| GitHub Actions, Windows | MinGW-w64 (UCRT64) | Application build, smoke run, engine, export, performance and register suites |
| GitHub Actions, Ubuntu | Node 22 | `npm ci` and `npm test` (API and React) |

## Results by requirement

| Requirement | Cases | Result | Main evidence |
|---|---|---|---|
| FR-TMS-01 Configuration | TC-01 to TC-06, TC-57 | Passed | `test_config`, `test_register_cases` |
| FR-TMS-02 Vehicle generation | TC-07 to TC-11 | Passed | `test_register_cases` (TC-09: 100,000 vehicles within 1 point of 60/25/8/7) |
| FR-TMS-03 Queues | TC-12 to TC-18, TC-71, TC-74 | Passed | `test_vehicle_queue`, `test_register_cases`, ASan in CI |
| FR-TMS-04 Signal control | TC-19 to TC-24, TC-26, TC-70, TC-72, TC-73 | Passed | `test_signal_controller`, `test_register_cases` (configured All-Red 1 to 5) |
| FR-TMS-05 Emergency priority | TC-27 to TC-33, TC-71, TC-73 | Passed | `test_emergency_handler`, `test_emergency_preemption`, integrated console run (128 pre-emptions in 3,600 ticks, no unsafe state) |
| FR-TMS-06 Execution and display | TC-34 to TC-38 | Passed | `test_register_cases`, `test_simulation_engine`, CI menu check |
| FR-TMS-07 Statistics | TC-39 to TC-43, TC-75 | Passed | `test_simulation_engine`, `test_simulation_analytics`, `test_register_cases` |
| FR-TMS-08 Compare modes | TC-44 | Passed | `test_signal_control_comparison` |
| FR-TMS-09 Events and export | TC-45 to TC-47, TC-63 | Passed | `test_event_exports` |
| NFR-TMS-01 Performance | TC-48 to TC-51 | Passed | `test_performance`; 20/20 quiet 3,600-tick runs under 5 s (slowest 2.9 s on the laptop) |
| NFR-TMS-02 Safety | TC-20, TC-52 to TC-54, TC-70 to TC-74 | Passed | `test_randomized_safety` (100 x 500 ticks), `test_register_cases` (100 seeds x 3,600 ticks with emergencies) |
| NFR-TMS-03 Robustness | TC-05, TC-15, TC-17, TC-55 to TC-58 | Passed | 24 distinct invalid inputs (TC-55); ASan/UBSan and `-Werror` in CI (TC-56, TC-58) |
| NFR-TMS-04 Usability and portability | TC-65 to TC-69 | Passed | Smoke run, public-function inventory (TC-68), Ubuntu g++ 13 and MinGW-w64 builds (TC-69) |
| Extension: dashboard and API | TC-77 to TC-80 | Passed | `server/test/api.test.js`, `web/src/App.test.tsx` |

`Traffic_Management_Test_Cases_v2.2.docx` gives the status and evidence for every individual case.

## Defects found and fixed during final testing

| Defect | Effect | Fix |
|---|---|---|
| Emergency pre-emption was not called by the console engine | Emergency vehicles got queue priority but never changed the signals in the console app | `SimulationEngine` now runs `EmergencyHandler` before every signal update |
| Console All-Red setting was ignored | Any value other than 1 had no effect | `SignalController` applies the configured 1 to 5 ticks |
| Quiet mode waited for the live-display delay | A default quiet run took about 10 minutes unless `TMS_DASHBOARD_DELAY_MS=0` was set | The delay now applies only to the live display |

All three fixes have regression tests in `tests/test_register_cases.cpp`.

## How to reproduce

```powershell
powershell -ExecutionPolicy Bypass -File .\build_and_test.ps1   # C++ suites
npm ci; npm test                                                 # API and React tests
```

The CI workflow `.github/workflows/cpp-tests.yml` runs the full set on every push to `main` and on pull requests.
