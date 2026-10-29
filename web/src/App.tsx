import { useEffect, useMemo, useState } from "react";
import {
  Bar,
  BarChart,
  CartesianGrid,
  ComposedChart,
  Legend,
  Line,
  LineChart,
  ReferenceLine,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from "recharts";

type Summary = {
  generated: number;
  served: number;
  blocked: number;
  waiting: number;
  averageWaitingTime: number | null;
  maximumWaitingTime: number | null;
  maximumQueueLength: number;
  throughput: number | null;
  emergencyServed: number;
  averageEmergencyWaitingTime: number | null;
  maximumEmergencyWaitingTime: number | null;
  preemptionCount: number;
  vehicleCountInvariantPassed: boolean;
  signalSafetyPassed: boolean;
  elapsedTicks: number;
};

type Snapshot = {
  tick: number;
  state: string;
  preemptionActive: boolean;
  queues: Record<"North" | "South" | "East" | "West", number>;
  signals: Record<"North" | "South" | "East" | "West", string>;
};

type SimulationEvent = {
  tick: number;
  vehicleId: number;
  type: "ARRIVAL" | "REJECTION" | "DEPARTURE" | "PREEMPTION";
  direction: string;
  emergency: boolean;
  waitingTime: number | null;
};

type SimulationResult = {
  runId: string;
  status: "completed";
  mode: "fixed" | "adaptive";
  seed: number;
  summary: Summary;
  timeSeries: Snapshot[];
  events: SimulationEvent[];
};

type RunRecord = {
  runId: string;
  status: "running" | "completed" | "failed";
  createdAt: string;
  mode: "fixed" | "adaptive";
  ticks: number;
  seed: number;
  error: string | null;
  result: SimulationResult | null;
};

const approachColors = {
  North: "#3d82f6",
  South: "#21a67a",
  East: "#d68b2d",
  West: "#9b6ade",
};

const stateNames = [
  "NS green",
  "NS yellow",
  "All-red 1",
  "EW green",
  "EW yellow",
  "All-red 2",
];

const stateCodes: Record<string, number> = {
  NS_GREEN: 0,
  NS_YELLOW: 1,
  ALL_RED_1: 2,
  EW_GREEN: 3,
  EW_YELLOW: 4,
  ALL_RED_2: 5,
};

async function requestJson<T>(url: string, init?: RequestInit): Promise<T> {
  const response = await fetch(url, init);
  const payload = (await response.json()) as T & { error?: string };
  if (!response.ok) {
    throw new Error(payload.error || `Request failed (${response.status}).`);
  }
  return payload;
}

function formatMetric(value: number | null, digits = 0): string {
  return value === null || !Number.isFinite(value)
    ? "N/A"
    : value.toFixed(digits);
}

function waitLabel(value: number | null): string {
  return value === null ? "N/A" : `${value} ticks`;
}

function App() {
  const [runs, setRuns] = useState<RunRecord[]>([]);
  const [runId, setRunId] = useState("");
  const [run, setRun] = useState<RunRecord | null>(null);
  const [ticks, setTicks] = useState(300);
  const [mode, setMode] = useState<"fixed" | "adaptive">("adaptive");
  const [seed, setSeed] = useState(42);
  const [isRunning, setIsRunning] = useState(false);
  const [isRefreshing, setIsRefreshing] = useState(false);
  const [graphsGenerated, setGraphsGenerated] = useState(false);
  const [error, setError] = useState("");

  const loadRuns = async () => {
    const records = await requestJson<RunRecord[]>("/api/simulations");
    setRuns(records);
  };

  useEffect(() => {
    loadRuns().catch((loadError: Error) => setError(loadError.message));
  }, []);

  const selectRun = async (selectedId: string) => {
    if (!selectedId) {
      setRunId("");
      setRun(null);
      setGraphsGenerated(false);
      return;
    }
    setError("");
    setGraphsGenerated(false);
    setRunId(selectedId);
    setIsRefreshing(true);
    try {
      const selectedRun = await requestJson<RunRecord>(
        `/api/simulations/${selectedId}`
      );
      setRun(selectedRun);
    } catch (selectionError) {
      setError((selectionError as Error).message);
    } finally {
      setIsRefreshing(false);
    }
  };

  const refreshSelectedRun = async () => {
    if (!runId) return;
    setError("");
    setIsRefreshing(true);
    try {
      const refreshed = await requestJson<RunRecord>(
        `/api/simulations/${runId}`
      );
      setRun(refreshed);
      await loadRuns();
    } catch (refreshError) {
      setError((refreshError as Error).message);
    } finally {
      setIsRefreshing(false);
    }
  };

  const startRun = async (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    setError("");
    setRun(null);
    setRunId("");
    setGraphsGenerated(false);
    setIsRunning(true);
    try {
      const started = await requestJson<{ runId: string; status: string }>(
        "/api/simulations",
        {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ ticks, mode, seed }),
        }
      );
      setRunId(started.runId);
      let completed: RunRecord;
      do {
        await new Promise((resolve) => window.setTimeout(resolve, 350));
        completed = await requestJson<RunRecord>(
          `/api/simulations/${started.runId}`
        );
        setRun(completed);
      } while (completed.status === "running");
      if (completed.status === "failed") {
        setError(completed.error || "The simulation failed.");
      }
      await loadRuns();
    } catch (runError) {
      setError((runError as Error).message);
    } finally {
      setIsRunning(false);
    }
  };

  const result = run?.status === "completed" ? run.result : null;
  const queueData = useMemo(
    () =>
      result?.timeSeries.map((point) => ({
        tick: point.tick,
        ...point.queues,
      })) ?? [],
    [result]
  );
  const outcomes = result
    ? [
        { outcome: "Generated", vehicles: result.summary.generated },
        { outcome: "Served", vehicles: result.summary.served },
        { outcome: "Blocked", vehicles: result.summary.blocked },
        { outcome: "Waiting", vehicles: result.summary.waiting },
      ]
    : [];
  const waitingData = useMemo(() => {
    const observations = (result?.events ?? []).filter(
      (item) => item.type === "DEPARTURE" && item.waitingTime !== null
    );
    const counts = new Map<number, number>();
    for (const observation of observations) {
      const wait = observation.waitingTime as number;
      counts.set(wait, (counts.get(wait) ?? 0) + 1);
    }
    return Array.from(counts.entries())
      .sort(([left], [right]) => left - right)
      .map(([waitingTime, vehicles]) => ({ waitingTime, vehicles }));
  }, [result]);
  const signalData = useMemo(
    () =>
      result?.timeSeries.map((point) => ({
        tick: point.tick,
        stateCode: stateCodes[point.state] ?? null,
      })) ?? [],
    [result]
  );
  const throughputData = useMemo(() => {
    if (!result) return [];
    const departures = new Map<number, number>();
    for (const item of result.events) {
      if (item.type === "DEPARTURE") {
        departures.set(item.tick, (departures.get(item.tick) ?? 0) + 1);
      }
    }
    let total = 0;
    return result.timeSeries.map((point) => {
      total += departures.get(point.tick) ?? 0;
      return { tick: point.tick, served: total };
    });
  }, [result]);
  const emergencyData = useMemo(() => {
    if (!result) return [];
    const byTick = new Map<
      number,
      { arrivals: number; served: number; preemptions: number }
    >();
    for (const point of result.timeSeries) {
      byTick.set(point.tick, { arrivals: 0, served: 0, preemptions: 0 });
    }
    for (const item of result.events) {
      if (item.type === "PREEMPTION") {
        const point = byTick.get(item.tick);
        if (point) point.preemptions += 1;
      } else if (item.emergency && item.type === "ARRIVAL") {
        const point = byTick.get(item.tick);
        if (point) point.arrivals += 1;
      } else if (item.emergency && item.type === "DEPARTURE") {
        const point = byTick.get(item.tick);
        if (point) point.served += 1;
      }
    }
    return Array.from(byTick.entries()).map(([tick, values]) => ({
      tick,
      ...values,
    }));
  }, [result]);
  const emergencyWaits =
    result?.events.filter(
      (item) =>
        item.type === "DEPARTURE" &&
        item.emergency &&
        item.waitingTime !== null
    ) ?? [];

  const selectedStatus = isRunning
    ? "Running"
    : run?.status === "completed"
      ? "Completed"
      : run?.status === "failed"
        ? "Failed"
        : "Ready";

  return (
    <main className="app-shell">
      <header className="topbar">
        <a className="brand" href="/" aria-label="Traffic Management Simulation home">
          <span className="brand-mark" aria-hidden="true">
            <svg viewBox="0 0 32 32" role="img">
              <path d="M7 25h18M10 25V8h12v17M13 12h6M13 16h6M13 20h6" />
              <circle cx="7" cy="8" r="2" />
              <circle cx="25" cy="8" r="2" />
            </svg>
          </span>
          <span>
            <strong>Traffic Management Simulation</strong>
            <small>Simulation Analytics Dashboard</small>
          </span>
        </a>
        <div className="topbar-meta">
          <span className={`status-pill status-${selectedStatus.toLowerCase()}`}>
            <span className="status-dot" />
            {selectedStatus}
          </span>
          {run && (
            <span className="topbar-run">
              {run.mode} · {run.status === "completed" ? run.ticks : "—"} ticks
            </span>
          )}
        </div>
      </header>

      <div className="page-content">
        <section className="hero">
          <div>
            <p className="eyebrow">TRAFFIC OPERATIONS / ANALYTICS</p>
            <h1>Simulation overview</h1>
            <p className="hero-copy">
              Explore real signal, queue and vehicle performance from your latest
              C++ simulation run.
            </p>
          </div>
          <div className="hero-summary" aria-live="polite">
            <span className="hero-summary-label">SELECTED RUN</span>
            <strong>{runId ? runId.slice(0, 8).toUpperCase() : "NO RUN SELECTED"}</strong>
            <span>{result ? `${result.summary.elapsedTicks} elapsed ticks` : "Run a simulation to begin"}</span>
          </div>
        </section>

        <section className="control-panel panel" aria-labelledby="controls-title">
          <div className="section-heading">
            <div>
              <p className="eyebrow">CONFIGURATION</p>
              <h2 id="controls-title">Simulation controls</h2>
            </div>
            {isRunning && (
              <span className="running-indicator">
                <span className="spinner" />
                Native simulation running
              </span>
            )}
          </div>
          <form className="control-form" onSubmit={startRun}>
            <label>
              Duration
              <span className="input-wrap">
                <input
                  type="number"
                  min="0"
                  max="1000"
                  value={ticks}
                  onChange={(event) => setTicks(Number(event.target.value))}
                  required
                />
                <span>ticks</span>
              </span>
            </label>
            <label>
              Signal timing
              <select
                value={mode}
                onChange={(event) =>
                  setMode(event.target.value as "fixed" | "adaptive")
                }
              >
                <option value="adaptive">Adaptive</option>
                <option value="fixed">Fixed</option>
              </select>
            </label>
            <label>
              Workload seed
              <input
                type="number"
                min="0"
                max="2147483647"
                value={seed}
                onChange={(event) => setSeed(Number(event.target.value))}
                required
              />
            </label>
            <button className="button button-primary run-button" disabled={isRunning}>
              {isRunning ? (
                <>
                  <span className="spinner spinner-light" />
                  Running…
                </>
              ) : (
                <>
                  <span aria-hidden="true">▶</span>
                  Run simulation
                </>
              )}
            </button>
          </form>
          <div className="secondary-controls">
            <label className="history-select">
              Previous runs
              <select
                value={runId}
                onChange={(event) => void selectRun(event.target.value)}
                aria-label="Select a previous simulation run"
              >
                <option value="">Select a completed run</option>
                {runs.map((record) => (
                  <option key={record.runId} value={record.runId}>
                    {new Date(record.createdAt).toLocaleString()} ·{" "}
                    {record.mode} · {record.ticks} ticks · {record.status}
                  </option>
                ))}
              </select>
            </label>
            <div className="action-group">
              <button
                className="button button-secondary"
                onClick={() => void refreshSelectedRun()}
                disabled={!runId || isRefreshing}
              >
                {isRefreshing ? "Refreshing…" : "↻ Refresh results"}
              </button>
              <button
                className="button button-graphs"
                onClick={() => setGraphsGenerated(true)}
                disabled={!result}
              >
                <span aria-hidden="true">▥</span>
                Generate graphs
              </button>
            </div>
          </div>
          {error && (
            <div className="error-banner" role="alert">
              <strong>Unable to complete that action.</strong>
              <span>{error}</span>
            </div>
          )}
        </section>

        {!result ? (
          <section className="empty-state panel">
            <div className="empty-illustration" aria-hidden="true">
              <span>●</span>
              <span>●</span>
              <span>●</span>
            </div>
            <h2>{run?.status === "failed" ? "Run did not complete" : "Your analytics will appear here"}</h2>
            <p>
              Configure a run and start the native traffic simulation. Summary
              metrics and recorded data charts will appear when it completes.
            </p>
          </section>
        ) : (
          <>
            <section className="kpi-grid" aria-label="Simulation key performance indicators">
              <KpiCard label="Vehicles generated" value={formatMetric(result.summary.generated)} tone="blue" />
              <KpiCard label="Vehicles served" value={formatMetric(result.summary.served)} tone="green" />
              <KpiCard label="Vehicles blocked" value={formatMetric(result.summary.blocked)} tone="orange" />
              <KpiCard label="Still waiting" value={formatMetric(result.summary.waiting)} tone="purple" />
              <KpiCard label="Average wait" value={waitLabel(result.summary.averageWaitingTime)} detail="Served vehicles" />
              <KpiCard label="Maximum wait" value={waitLabel(result.summary.maximumWaitingTime)} detail="Served vehicles" />
              <KpiCard label="Maximum queue" value={formatMetric(result.summary.maximumQueueLength)} detail="Vehicles / approach" />
              <KpiCard label="Throughput" value={formatMetric(result.summary.throughput, 2)} detail="Vehicles per tick" />
              <KpiCard label="Emergency served" value={formatMetric(result.summary.emergencyServed)} tone="red" />
              <KpiCard label="Signal pre-emptions" value={formatMetric(result.summary.preemptionCount)} tone="amber" detail="Executed interventions" />
            </section>

            <section className="panel summary-panel" aria-labelledby="summary-title">
              <div className="section-heading summary-heading">
                <div>
                  <p className="eyebrow">RUN VALIDATION</p>
                  <h2 id="summary-title">Run summary</h2>
                </div>
                <span className="run-id">{result.runId}</span>
              </div>
              <div className="summary-content">
                <div className="summary-details">
                  <SummaryItem label="Run mode" value={result.mode} />
                  <SummaryItem label="Elapsed time" value={`${result.summary.elapsedTicks} ticks`} />
                  <SummaryItem label="Workload seed" value={String(result.seed)} />
                  <SummaryItem label="Average emergency wait" value={waitLabel(result.summary.averageEmergencyWaitingTime)} />
                  <SummaryItem label="Maximum emergency wait" value={waitLabel(result.summary.maximumEmergencyWaitingTime)} />
                  <SummaryItem label="Vehicle accounting" value={`${result.summary.generated} = ${result.summary.served} + ${result.summary.blocked} + ${result.summary.waiting}`} />
                </div>
                <div className="validation-stack">
                  <ValidationBadge
                    passed={result.summary.vehicleCountInvariantPassed}
                    title="Vehicle accounting invariant"
                    detail="Generated = served + blocked + waiting"
                  />
                  <ValidationBadge
                    passed={result.summary.signalSafetyPassed}
                    title="Signal safety"
                    detail="Controller safety checks during the run"
                  />
                </div>
              </div>
            </section>

            {graphsGenerated ? (
              <section className="charts-section" aria-labelledby="charts-title">
                <div className="section-heading charts-heading">
                  <div>
                    <p className="eyebrow">RECORDED RUN DATA</p>
                    <h2 id="charts-title">Simulation graphs</h2>
                    <p className="chart-caption">
                      All charts are derived from {result.timeSeries.length} recorded
                      signal and queue snapshots and actual vehicle events.
                    </p>
                  </div>
                  <button
                    className="button button-secondary"
                    onClick={() => setGraphsGenerated(false)}
                  >
                    Hide graphs
                  </button>
                </div>
                <div className="chart-grid">
                  <ChartPanel title="Vehicle outcomes" subtitle="Actual end-of-run totals">
                    <ResponsiveContainer width="100%" height="100%">
                      <BarChart data={outcomes} margin={{ top: 8, right: 12, left: 0, bottom: 4 }}>
                        <CartesianGrid stroke="#e9eef4" vertical={false} />
                        <XAxis dataKey="outcome" tickLine={false} axisLine={false} />
                        <YAxis allowDecimals={false} tickLine={false} axisLine={false} />
                        <Tooltip />
                        <Bar dataKey="vehicles" name="Vehicles" fill="#397be6" radius={[5, 5, 0, 0]} />
                      </BarChart>
                    </ResponsiveContainer>
                  </ChartPanel>

                  <ChartPanel title="Queue length over time" subtitle="Vehicles waiting in each approach queue">
                    <ResponsiveContainer width="100%" height="100%">
                      <LineChart data={queueData} margin={{ top: 8, right: 14, left: 0, bottom: 4 }}>
                        <CartesianGrid stroke="#e9eef4" vertical={false} />
                        <XAxis dataKey="tick" type="number" domain={[0, result.summary.elapsedTicks]} tickLine={false} axisLine={false} />
                        <YAxis allowDecimals={false} tickLine={false} axisLine={false} />
                        <Tooltip labelFormatter={(value) => `Tick ${value}`} />
                        <Legend />
                        {Object.entries(approachColors).map(([approach, color]) => (
                          <Line key={approach} type="monotone" dataKey={approach} stroke={color} strokeWidth={2} dot={false} activeDot={{ r: 4 }} />
                        ))}
                      </LineChart>
                    </ResponsiveContainer>
                  </ChartPanel>

                  <ChartPanel title="Served-vehicle waiting times" subtitle="Empirical frequency from actual departure events">
                    {waitingData.length === 0 ? (
                      <ChartEmpty message="No vehicles were served in this run." />
                    ) : (
                      <ResponsiveContainer width="100%" height="100%">
                        <BarChart data={waitingData} margin={{ top: 8, right: 12, left: 0, bottom: 4 }}>
                          <CartesianGrid stroke="#e9eef4" vertical={false} />
                          <XAxis dataKey="waitingTime" tickLine={false} axisLine={false} label={{ value: "Waiting time (ticks)", position: "insideBottom", offset: -2 }} />
                          <YAxis allowDecimals={false} tickLine={false} axisLine={false} label={{ value: "Vehicles", angle: -90, position: "insideLeft" }} />
                          <Tooltip labelFormatter={(value) => `${value} ticks waiting`} />
                          <Bar dataKey="vehicles" name="Served vehicles" fill="#21a67a" radius={[5, 5, 0, 0]} />
                        </BarChart>
                      </ResponsiveContainer>
                    )}
                  </ChartPanel>

                  <ChartPanel title="Signal phase timeline" subtitle="Recorded controller state at each tick">
                    <ResponsiveContainer width="100%" height="100%">
                      <LineChart data={signalData} margin={{ top: 8, right: 14, left: 8, bottom: 4 }}>
                        <CartesianGrid stroke="#e9eef4" vertical={false} />
                        <XAxis dataKey="tick" type="number" domain={[0, result.summary.elapsedTicks]} tickLine={false} axisLine={false} />
                        <YAxis type="number" domain={[0, 5]} ticks={[0, 1, 2, 3, 4, 5]} tickFormatter={(value) => stateNames[value] ?? ""} tickLine={false} axisLine={false} width={78} />
                        <Tooltip labelFormatter={(value) => `Tick ${value}`} formatter={(value) => stateNames[Number(value)] ?? "Unknown state"} />
                        <Line type="stepAfter" dataKey="stateCode" name="Signal state" stroke="#30455f" strokeWidth={2} dot={false} activeDot={{ r: 4 }} />
                      </LineChart>
                    </ResponsiveContainer>
                  </ChartPanel>

                  <ChartPanel title="Cumulative throughput" subtitle="Cumulative vehicle departures from recorded events">
                    <ResponsiveContainer width="100%" height="100%">
                      <LineChart data={throughputData} margin={{ top: 8, right: 14, left: 0, bottom: 4 }}>
                        <CartesianGrid stroke="#e9eef4" vertical={false} />
                        <XAxis dataKey="tick" type="number" domain={[0, result.summary.elapsedTicks]} tickLine={false} axisLine={false} />
                        <YAxis allowDecimals={false} tickLine={false} axisLine={false} />
                        <Tooltip labelFormatter={(value) => `Tick ${value}`} />
                        <ReferenceLine y={result.summary.served} stroke="#b8c5d5" strokeDasharray="4 4" />
                        <Line type="stepAfter" dataKey="served" name="Cumulative vehicles served" stroke="#397be6" strokeWidth={2.5} dot={false} />
                      </LineChart>
                    </ResponsiveContainer>
                  </ChartPanel>

                  <ChartPanel title="Emergency performance" subtitle="Emergency arrivals, services, and executed pre-emptions by tick">
                    <ResponsiveContainer width="100%" height="100%">
                      <ComposedChart data={emergencyData} margin={{ top: 8, right: 12, left: 0, bottom: 4 }}>
                        <CartesianGrid stroke="#e9eef4" vertical={false} />
                        <XAxis dataKey="tick" type="number" domain={[0, result.summary.elapsedTicks]} tickLine={false} axisLine={false} />
                        <YAxis allowDecimals={false} tickLine={false} axisLine={false} />
                        <Tooltip labelFormatter={(value) => `Tick ${value}`} />
                        <Legend />
                        <Bar dataKey="arrivals" name="Emergency arrivals" fill="#ed9d48" />
                        <Bar dataKey="served" name="Emergency served" fill="#21a67a" />
                        <Line type="monotone" dataKey="preemptions" name="Executed pre-emptions" stroke="#cc5961" strokeWidth={2} dot={false} />
                      </ComposedChart>
                    </ResponsiveContainer>
                  </ChartPanel>

                  <section className="panel emergency-table-panel">
                    <div className="chart-title">
                      <h3>Emergency service observations</h3>
                      <p>Waiting times recorded for served emergency vehicles</p>
                    </div>
                    {emergencyWaits.length === 0 ? (
                      <ChartEmpty message="No emergency vehicles were served in this run." />
                    ) : (
                      <div className="table-scroll">
                        <table>
                          <thead>
                            <tr>
                              <th>Vehicle</th>
                              <th>Approach</th>
                              <th>Departure tick</th>
                              <th>Wait</th>
                            </tr>
                          </thead>
                          <tbody>
                            {emergencyWaits.map((observation) => (
                              <tr key={`${observation.vehicleId}-${observation.tick}`}>
                                <td>#{observation.vehicleId}</td>
                                <td>{observation.direction}</td>
                                <td>{observation.tick}</td>
                                <td>{observation.waitingTime} ticks</td>
                              </tr>
                            ))}
                          </tbody>
                        </table>
                      </div>
                    )}
                  </section>
                </div>
              </section>
            ) : (
              <section className="graph-prompt panel">
                <div className="graph-prompt-icon" aria-hidden="true">▥</div>
                <div>
                  <h2>Ready to explore this run?</h2>
                  <p>Generate interactive charts from the recorded simulation data.</p>
                </div>
                <button className="button button-graphs" onClick={() => setGraphsGenerated(true)}>
                  Generate graphs
                </button>
              </section>
            )}
          </>
        )}
        <footer className="footer">
          <span>Traffic Management Simulation</span>
          <span>Native C++ engine · Recorded run analytics</span>
        </footer>
      </div>
    </main>
  );
}

function KpiCard({
  label,
  value,
  detail,
  tone,
}: {
  label: string;
  value: string;
  detail?: string;
  tone?: string;
}) {
  return (
    <article className={`kpi-card ${tone ? `tone-${tone}` : ""}`}>
      <span className="kpi-label">{label}</span>
      <strong className="kpi-value">{value}</strong>
      <span className="kpi-detail">{detail || "\u00a0"}</span>
    </article>
  );
}

function SummaryItem({ label, value }: { label: string; value: string }) {
  return (
    <div className="summary-item">
      <span>{label}</span>
      <strong>{value}</strong>
    </div>
  );
}

function ValidationBadge({
  passed,
  title,
  detail,
}: {
  passed: boolean;
  title: string;
  detail: string;
}) {
  return (
    <div className={`validation-badge ${passed ? "passed" : "failed"}`} role="status">
      <span className="validation-icon" aria-hidden="true">{passed ? "✓" : "!"}</span>
      <span>
        <strong>{title}: {passed ? "PASS" : "FAIL"}</strong>
        <small>{detail}</small>
      </span>
    </div>
  );
}

function ChartPanel({
  title,
  subtitle,
  children,
}: {
  title: string;
  subtitle: string;
  children: React.ReactNode;
}) {
  return (
    <section className="panel chart-panel">
      <div className="chart-title">
        <h3>{title}</h3>
        <p>{subtitle}</p>
      </div>
      <div className="chart-area">{children}</div>
    </section>
  );
}

function ChartEmpty({ message }: { message: string }) {
  return <div className="chart-empty">{message}</div>;
}

export default App;
