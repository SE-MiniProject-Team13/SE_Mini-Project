# Optional Live Traffic Dashboard

This dashboard is a separate visualization layer. The C++ simulation remains the source of truth; the engine writes `dashboard_state.json` after initialization, every simulation tick, and reset.

## Requirements

- Windows 10/11
- Python 3 with Tkinter (the standard Windows Python installer normally includes it)
- The same MinGW `g++` setup used by `build_and_test.ps1`

No Python packages need to be installed.

## Run it

1. Open PowerShell in the repository root.
2. Build and run the existing checks:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build_and_test.ps1
   ```

3. In the first PowerShell window, start the dashboard:

   ```powershell
   python .\traffic_dashboard.py
   ```

   It will initially say that it is waiting for the C++ simulation.

4. Open a second PowerShell window, navigate to the same repository root, and start the existing application:

   ```powershell
   cd C:\Users\Vedanta\Desktop\SE_Mini_Project
   $env:TMS_DASHBOARD_DELAY_MS = "100"
   .\.codex-build\traffic_sim.exe
   ```

5. Use the normal C++ console menu. As you choose Run, Step, Quiet, or Reset, the dashboard refreshes from the engine's state file. The dashboard does not control the simulation; continue using the existing console menu.

## State file

`dashboard_state.json` is generated output, not source code. Add this line to `.gitignore` so it is not committed:

```gitignore
dashboard_state.json
```

The state file contains the current tick, duration, generated/served/blocked/waiting counts, average waiting time, throughput, signal phase and remaining time, pre-emption flag, queue capacity, per-direction light colors, and per-direction queue lengths.

## Notes

- The displayed queue trend is built from actual snapshots emitted by the engine.
- The intersection is a schematic, not a to-scale road map; small blue blocks are illustrative queue markers while the numeric queue counts are exact.
- Because the existing C++ application is console-driven, keep it running in the second terminal while viewing the dashboard.
- The dashboard adds read-only telemetry and does not change the tick order or signal/queue algorithms.
- `TMS_DASHBOARD_DELAY_MS` changes only wall-clock pacing, never simulated tick calculations. It is disabled by default and capped at 5000 ms per tick.
- `TMS_DASHBOARD_DELAY_MS` changes only wall-clock pacing, never the simulated tick calculations. It is disabled by default and capped at 5000 ms per tick.
