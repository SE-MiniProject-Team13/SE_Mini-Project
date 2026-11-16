# Deviations from the SRS and SAD

Traffic Management Simulation (TMS), Team 13. Final submission, 16 November 2026. Development is frozen at this baseline.

This document lists every place where the delivered product differs from the original design documents, and why. It is the reference for the deviation questions in the final demo.

## Document baselines

| Document | Original | Final |
|---|---|---|
| SRS | `archive/Traffic_Management_SRS_v1.0.docx` (Sep 2026, FR-01 to FR-47) | `Traffic_Management_SRS_v2.2.docx` (FR-TMS-01 to 09, NFR-TMS-01 to 04, SEC-TMS-01 to 06) |
| SAD | `archive/Traffic_Management_SAD_v1.0.docx` (written before any code existed) | `Traffic_Management_SAD_v2.1.docx.pdf` plus the v2.2 addendum below |
| Test Plan | `archive/Traffic_Management_Test_Plan_Final.docx` (old FR-01 IDs) | `Traffic_Management_Test_Plan_v2.2.docx` |
| Test cases | `archive/Traffic_Management_Test_Cases_v1.0.docx` (not executed) | `Traffic_Management_Test_Cases_v2.2.docx` (all executed) |

Superseded versions are kept in `Docx/archive/` for traceability.

## Deviations from the original SRS (v1.0)

| ID | Original requirement | Delivered behaviour | Reason | Verified by |
|---|---|---|---|---|
| D-01 | Requirement IDs FR-01 to FR-47, NFR-01 to NFR-14 | Grouped into FR-TMS-01 to 09 and NFR-TMS-01 to 04, one per GitHub backlog issue (#35 to #47) | Keeps the SRS, backlog, PRs and tests on one set of IDs | SRS v2.2 Appendix D |
| D-02 | Duration 1 to 86,400 ticks | 1 to 3,600 ticks | One simulated hour is enough for every scenario and keeps quiet runs under 5 s | TC-01 |
| D-03 | Load settings from `config.txt` (FR-04, SEC-05) | Interactive prompts with displayed defaults; no config file | Issue #35 asked only for console configuration | TC-04 (modified); TC-61 removed |
| D-04 | A separate arrival probability per approach | One arrival probability, with an independent trial on each approach every tick | Simpler configuration; issue #36 wording | TC-02, TC-07 |
| D-05 | Ask before overwriting `report.txt` / `events.csv` (SEC-04) | Exports overwrite the fixed files | Not part of the backlog issues | TC-60 removed |
| D-06 | Report the signal cycle count (FR-25) | Counted by the controller and unit-tested, not shown in the summary | Not part of the backlog issues | TC-25 removed |
| D-07 | Throughput per simulated minute | Throughput in vehicles per tick, unit shown | One tick is the simulation's time unit | TC-42 (modified) |
| D-08 | Average wait per approach | Overall average and maximum wait of served vehicles | Issue #41 wording | TC-40 (modified) |
| D-09 | Security requirements SEC-01 to SEC-08 | Objectives SO-1 to SO-4 and requirements SEC-TMS-01 to 06 (SRS v2.2 Section 6.5). The path-separator rule, overwrite prompt, config-line limits and 64-bit-counter rule were dropped | Matches what the product does; every remaining requirement is tested | Test Plan 5.1 |
| D-10 | All-Red exactly one tick (FR-21) | All-Red configurable from 1 to 5 ticks (issue #39), never less than one (issue #45) | Issue #39 requires a configurable duration | TC-21 |
| D-11 | Maximum pre-emption fixed at 15 ticks (FR-31) | Default 15 ticks, kept between Gmin and Gmax; not exposed in the console menu | Issue #38 asks for a finite configured maximum without a value | TC-31 |
| D-12 | No function longer than 50 lines (NFR-12) | One header/source pair per module; some menu functions in `main.cpp` are longer | Prior-draft rule, not in the backlog issues | TC-67 (modified) |
| D-13 | Ubuntu 20.04 with g++ 9 or later | GitHub Actions Ubuntu LTS runner with g++ 13, plus Windows MinGW-w64 | GitHub has retired its Ubuntu 20.04 runners; g++ 13 meets "g++ 9 or later" | TC-69 |

## Additions not in the original SRS

| ID | Addition | Notes |
|---|---|---|
| A-01 | Web analytics dashboard: Node.js API (`server/`) and React UI (`web/`) | Listens on 127.0.0.1 only. Uses `TrafficSimulation` through `src/analytics_main.cpp`. Covered by extension cases TC-77 to TC-80 and SEC-TMS-05 |
| A-02 | Live snapshot file `dashboard_state.json` and the Tkinter viewer `traffic_dashboard.py` | Read-only view of the running console simulation |
| A-03 | Live-display delay (`TMS_DASHBOARD_DELAY_MS`, default 1000 ms per tick) | Applies only to Run and Step with live display. Quiet runs and tests run at full speed |
| A-04 | Compare Fixed vs Adaptive menu option | Implements FR-TMS-08; both modes use one shared arrival schedule |

## SAD v2.2 addendum (changes after SAD v2.1)

SAD v2.1 records the code as of 12 November 2026. These changes came after it:

1. **Emergency pre-emption is integrated into the console engine.** SAD v2.1 (Sections 2.3, 4.5, 4.6, 6.4, 6.5 and Figures 8-1, 8-2, 8-6) describes `EmergencyHandler` as a standalone helper that is never called. Now `SimulationEngine` owns an `EmergencyHandler` built over its four queues. Each tick runs:
   1. arrivals;
   2. `EmergencyHandler::processTick`, then `SignalController::advanceState`;
   3. service;
   4. statistics and display.

   Pre-emption start and end appear in `events.csv` as `signal_phase_change` rows, for example "Emergency pre-emption started for EW phase (vehicle #12 from East)". `emergency_start`/`emergency_end` rows still mark an emergency vehicle entering its queue and leaving the intersection. The live display shows the pre-emption state, and the summary's "Pre-emptions" count is now filled in. `src/emergency_handler.cpp` is part of every console build: `CMakeLists.txt`, `build_and_test.ps1`, CI and the READMEs.
2. **All-Red duration is applied.** `SignalTimings` takes an All-Red value (1 to 5; out-of-range values fall back to 1). `SimulationEngine` and `TrafficSimulation` pass the configured value, so the console setting now takes effect.
3. **Quiet mode is not slowed by the live-display delay.** Before this fix, a quiet run waited 1 s per tick unless `TMS_DASHBOARD_DELAY_MS=0` was set, so a default 600-tick quiet run took about 10 minutes.
4. **Verification.** `tests/test_register_cases.cpp` automates every case that was Not Run or Blocked. CI now runs every C++ suite, a `-Werror` build, AddressSanitizer/UBSan runs of the full application, and the Node and React tests.

## Open decisions now closed

| Decision (SRS v2.1 Appendix C.1) | Outcome |
|---|---|
| #42: must both Compare runs see identical arrival sequences? | Met: both runs use one shared schedule (TC-44) |
| #46: 20 or 10 invalid-input test cases? | Met at the stricter number: 24 distinct invalid inputs (TC-55) |
| #39/#45: configurable All-Red versus "the required All-Red tick" | Configurable 1 to 5, never less than one tick (TC-21) |
| #38: maximum pre-emption value | 15-tick default, kept between Gmin and Gmax (TC-31) |
| TC-48 reference host | The GitHub-hosted Ubuntu runner, with the local laptop as a second host |
