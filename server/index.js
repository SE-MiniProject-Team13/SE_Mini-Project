const crypto = require("node:crypto");
const { spawn } = require("node:child_process");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const express = require("express");

const MAX_RUNS = 10;
const MAX_OUTPUT_BYTES = 16 * 1024 * 1024;

function runProcess(command, args, options = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, args, {
      cwd: options.cwd,
      windowsHide: true,
      shell: false,
      stdio: ["ignore", "pipe", "pipe"],
    });
    const output = [];
    const errors = [];
    let outputBytes = 0;
    let errorBytes = 0;
    let settled = false;
    let timeout;

    function fail(error) {
      if (!settled) {
        settled = true;
        clearTimeout(timeout);
        reject(error);
      }
    }

    child.stdout.on("data", (chunk) => {
      outputBytes += chunk.length;
      if (outputBytes > MAX_OUTPUT_BYTES) {
        child.kill();
        fail(new Error("Simulation output exceeded the allowed size."));
        return;
      }
      output.push(chunk);
    });
    child.stderr.on("data", (chunk) => {
      errorBytes += chunk.length;
      if (errorBytes <= MAX_OUTPUT_BYTES) {
        errors.push(chunk);
      }
    });
    child.on("error", fail);
    child.on("close", (code) => {
      if (settled) {
        return;
      }
      settled = true;
      clearTimeout(timeout);
      const stderr = Buffer.concat(errors).toString("utf8");
      if (code !== 0) {
        reject(new Error(stderr || `Process exited with code ${code}.`));
        return;
      }
      resolve(Buffer.concat(output).toString("utf8"));
    });
    timeout = setTimeout(() => {
      child.kill();
      fail(new Error("Process exceeded its execution time limit."));
    }, options.timeoutMs || 120000);
  });
}

function createNativeRunner(projectRoot) {
  const executable = path.join(
    os.tmpdir(),
    `tms-dashboard-analytics-${process.pid}.exe`
  );
  let buildPromise;

  process.once("exit", () => {
    try {
      fs.unlinkSync(executable);
    } catch (error) {
      if (error.code !== "ENOENT") {
        console.error("Could not remove temporary analytics executable:", error);
      }
    }
  });

  async function ensureBuilt() {
    if (!buildPromise) {
      buildPromise = runProcess(
        process.env.CXX || "g++",
        [
          "-std=c++11",
          "-Wall",
          "-Wextra",
          "-pedantic",
          "-Iinclude",
          "src/vehicle_queue.cpp",
          "src/signal_controller.cpp",
          "src/simulation.cpp",
          "src/analytics_main.cpp",
          "-o",
          executable,
        ],
        { cwd: projectRoot }
      ).catch((error) => {
        buildPromise = undefined;
        throw error;
      });
    }
    await buildPromise;
  }

  return async function runNativeSimulation({ runId, ticks, mode, seed }) {
    await ensureBuilt();
    const raw = await runProcess(
      executable,
      ["--analytics", runId, String(ticks), mode, String(seed)],
      { cwd: projectRoot }
    );
    return JSON.parse(raw);
  };
}

function isIntegerInRange(value, minimum, maximum) {
  return Number.isInteger(value) && value >= minimum && value <= maximum;
}

function validateRequest(body) {
  return (
    body !== null &&
    typeof body === "object" &&
    !Array.isArray(body) &&
    isIntegerInRange(body.ticks, 0, 1000) &&
    (body.mode === "fixed" || body.mode === "adaptive") &&
    isIntegerInRange(body.seed, 0, 2147483647)
  );
}

function validateResult(result, runId, mode, ticks, seed) {
  if (
    result === null ||
    typeof result !== "object" ||
    result.runId !== runId ||
    result.status !== "completed" ||
    result.mode !== mode ||
    result.seed !== seed ||
    result.summary === null ||
    typeof result.summary !== "object" ||
    !Array.isArray(result.timeSeries) ||
    !Array.isArray(result.events)
  ) {
    throw new Error("The simulation returned an invalid result.");
  }

  const summary = result.summary;
  const countFields = [
    "generated",
    "served",
    "blocked",
    "waiting",
    "maximumQueueLength",
    "emergencyServed",
    "preemptionCount",
    "elapsedTicks",
  ];
  const optionalMetrics = [
    "averageWaitingTime",
    "maximumWaitingTime",
    "throughput",
    "averageEmergencyWaitingTime",
    "maximumEmergencyWaitingTime",
  ];
  if (
    countFields.some(
      (field) => !Number.isInteger(summary[field]) || summary[field] < 0
    ) ||
    optionalMetrics.some(
      (field) =>
        summary[field] !== null &&
        (typeof summary[field] !== "number" || !Number.isFinite(summary[field]))
    ) ||
    typeof summary.vehicleCountInvariantPassed !== "boolean" ||
    typeof summary.signalSafetyPassed !== "boolean" ||
    summary.elapsedTicks !== ticks ||
    summary.generated !== summary.served + summary.blocked + summary.waiting ||
    summary.emergencyServed > summary.served ||
    result.timeSeries.length !== summary.elapsedTicks + 1
  ) {
    throw new Error("The simulation returned inconsistent statistics.");
  }

  let previousTick = -1;
  for (const point of result.timeSeries) {
    if (
      point === null ||
      typeof point !== "object" ||
      !Number.isInteger(point.tick) ||
      point.tick <= previousTick ||
      point.tick > summary.elapsedTicks ||
      typeof point.state !== "string" ||
      point.queues === null ||
      typeof point.queues !== "object" ||
      point.signals === null ||
      typeof point.signals !== "object" ||
      ["North", "South", "East", "West"].some(
        (approach) =>
          !Number.isInteger(point.queues[approach]) ||
          point.queues[approach] < 0 ||
          typeof point.signals[approach] !== "string"
      )
    ) {
      throw new Error("The simulation returned invalid time-series data.");
    }
    previousTick = point.tick;
  }

  const eventCounts = {
    ARRIVAL: 0,
    REJECTION: 0,
    DEPARTURE: 0,
    PREEMPTION: 0,
  };
  for (const event of result.events) {
    if (
      event === null ||
      typeof event !== "object" ||
      !Number.isInteger(event.tick) ||
      event.tick < 0 ||
      event.tick > summary.elapsedTicks ||
      !Number.isInteger(event.vehicleId) ||
      event.vehicleId < 1 ||
      !Object.prototype.hasOwnProperty.call(eventCounts, event.type) ||
      typeof event.direction !== "string" ||
      typeof event.emergency !== "boolean" ||
      (event.waitingTime !== null &&
        (!Number.isInteger(event.waitingTime) || event.waitingTime < 0))
    ) {
      throw new Error("The simulation returned invalid vehicle events.");
    }
    eventCounts[event.type] += 1;
  }
  if (
    eventCounts.ARRIVAL + eventCounts.REJECTION !== summary.generated ||
    eventCounts.REJECTION !== summary.blocked ||
    eventCounts.DEPARTURE !== summary.served ||
    eventCounts.PREEMPTION !== summary.preemptionCount
  ) {
    throw new Error("The simulation returned inconsistent vehicle events.");
  }
}

function publicRun(run) {
  return {
    runId: run.runId,
    status: run.status,
    createdAt: run.createdAt,
    mode: run.mode,
    ticks: run.ticks,
    seed: run.seed,
    error: run.error,
    result: run.result,
  };
}

function createApp(options = {}) {
  const projectRoot = options.projectRoot || path.resolve(__dirname, "..");
  const runner = options.runner || createNativeRunner(projectRoot);
  const runs = new Map();
  let activeRunId = null;
  const app = express();

  app.use(express.json({ limit: "8kb" }));

  app.get("/api/simulations", (_request, response) => {
    const availableRuns = Array.from(runs.values())
      .sort((left, right) => right.createdAt.localeCompare(left.createdAt))
      .map((run) => ({
        runId: run.runId,
        status: run.status,
        createdAt: run.createdAt,
        mode: run.mode,
        ticks: run.ticks,
        seed: run.seed,
      }));
    response.json(availableRuns);
  });

  app.post("/api/simulations", (request, response) => {
    if (!validateRequest(request.body)) {
      response.status(400).json({
        error: "Provide ticks from 0 to 1000, mode fixed or adaptive, and a valid seed.",
      });
      return;
    }
    if (activeRunId !== null) {
      response.status(409).json({
        error: "A simulation is already running. Wait for it to finish before starting another.",
      });
      return;
    }

    const runId = crypto.randomUUID();
    const run = {
      runId,
      status: "running",
      createdAt: new Date().toISOString(),
      mode: request.body.mode,
      ticks: request.body.ticks,
      seed: request.body.seed,
      error: null,
      result: null,
    };
    runs.set(runId, run);
    activeRunId = runId;
    response.status(202).json({ runId, status: run.status });

    Promise.resolve()
      .then(() =>
        runner({
          runId,
          ticks: run.ticks,
          mode: run.mode,
          seed: run.seed,
        })
      )
      .then((result) => {
        validateResult(result, runId, run.mode, run.ticks, run.seed);
        run.status = "completed";
        run.result = result;
      })
      .catch((error) => {
        console.error(`Simulation ${runId} failed:`, error);
        run.status = "failed";
        run.error =
          "The simulation could not be built or completed. Check the backend logs.";
      })
      .finally(() => {
        if (activeRunId === runId) {
          activeRunId = null;
        }
        while (runs.size > MAX_RUNS) {
          const oldest = Array.from(runs.values()).find(
            (candidate) => candidate.runId !== activeRunId
          );
          if (!oldest) {
            break;
          }
          runs.delete(oldest.runId);
        }
      });
  });

  app.get("/api/simulations/:runId/timeseries", (request, response) => {
    const run = runs.get(request.params.runId);
    if (!run) {
      response.status(404).json({ error: "Simulation run not found." });
      return;
    }
    if (run.status !== "completed") {
      response.status(409).json({ error: "Simulation data is not ready yet." });
      return;
    }
    response.json({
      runId: run.runId,
      timeSeries: run.result.timeSeries,
      events: run.result.events,
    });
  });

  app.get("/api/simulations/:runId", (request, response) => {
    const run = runs.get(request.params.runId);
    if (!run) {
      response.status(404).json({ error: "Simulation run not found." });
      return;
    }
    response.json(publicRun(run));
  });

  app.use("/api", (_request, response) => {
    response.status(404).json({ error: "API endpoint not found." });
  });

  const builtFrontend = path.join(projectRoot, "web", "dist");
  if (fs.existsSync(builtFrontend)) {
    app.use(express.static(builtFrontend));
    app.get("*", (_request, response) => {
      response.sendFile(path.join(builtFrontend, "index.html"));
    });
  }

  app.use((error, _request, response, _next) => {
    if (error instanceof SyntaxError && "body" in error) {
      response.status(400).json({ error: "Request body must be valid JSON." });
      return;
    }
    if (error.status === 413) {
      response.status(413).json({ error: "Request body is too large." });
      return;
    }
    console.error("API request failed:", error);
    response.status(500).json({ error: "The request could not be completed." });
  });

  return app;
}

if (require.main === module) {
  const port = Number(process.env.API_PORT || 3001);
  const host = "127.0.0.1";
  createApp().listen(port, host, () => {
    console.log(`TMS analytics API listening at http://${host}:${port}`);
  });
}

module.exports = { createApp };
