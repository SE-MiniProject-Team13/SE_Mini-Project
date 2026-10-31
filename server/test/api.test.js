const assert = require("node:assert/strict");
const { afterEach, test } = require("node:test");
const { createApp } = require("../index");

const servers = [];

async function startServer(runner) {
  const server = createApp({ runner }).listen(0, "127.0.0.1");
  servers.push(server);
  await new Promise((resolve) => server.once("listening", resolve));
  return `http://127.0.0.1:${server.address().port}`;
}

async function postRun(baseUrl, body) {
  return fetch(`${baseUrl}/api/simulations`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
}

async function waitForStatus(baseUrl, runId, expectedStatus) {
  for (let attempt = 0; attempt < 30; attempt += 1) {
    const response = await fetch(`${baseUrl}/api/simulations/${runId}`);
    const run = await response.json();
    if (run.status === expectedStatus) return run;
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw new Error(`Run did not reach ${expectedStatus}.`);
}

function validResult({ runId, ticks, mode, seed }) {
  return {
    runId,
    status: "completed",
    mode,
    seed,
    summary: {
      generated: 1,
      served: 1,
      blocked: 0,
      waiting: 0,
      averageWaitingTime: 0,
      maximumWaitingTime: 0,
      maximumQueueLength: 1,
      throughput: ticks === 0 ? null : 1 / ticks,
      emergencyServed: 0,
      averageEmergencyWaitingTime: null,
      maximumEmergencyWaitingTime: null,
      preemptionCount: 0,
      vehicleCountInvariantPassed: true,
      signalSafetyPassed: true,
      elapsedTicks: ticks,
    },
    timeSeries: Array.from({ length: ticks + 1 }, (_unused, tick) => ({
      tick,
      state: "NS_GREEN",
      queues: { North: 0, South: 0, East: 0, West: 0 },
      signals: { North: "GREEN", South: "GREEN", East: "RED", West: "RED" },
    })),
    events: [
      {
        tick: 0,
        vehicleId: 1,
        type: "ARRIVAL",
        direction: "North",
        emergency: false,
        waitingTime: null,
      },
      {
        tick: ticks === 0 ? 0 : ticks,
        vehicleId: 1,
        type: "DEPARTURE",
        direction: "North",
        emergency: false,
        waitingTime: 0,
      },
    ],
  };
}

afterEach(async () => {
  await Promise.all(
    servers.splice(0).map(
      (server) =>
        new Promise((resolve, reject) => {
          server.close((error) => (error ? reject(error) : resolve()));
        })
    )
  );
});

test("rejects invalid simulation inputs", async () => {
  let called = false;
  const baseUrl = await startServer(async () => {
    called = true;
  });
  const response = await postRun(baseUrl, {
    ticks: -1,
    mode: "shell",
    seed: 0,
  });

  assert.equal(response.status, 400);
  assert.match((await response.json()).error, /ticks from 0 to 1000/);
  assert.equal(called, false);
});

test("returns completed data and time-series from a successful run", async () => {
  const baseUrl = await startServer(async (request) => validResult(request));
  const started = await postRun(baseUrl, {
    ticks: 2,
    mode: "fixed",
    seed: 7,
  });
  assert.equal(started.status, 202);
  const { runId } = await started.json();
  const completed = await waitForStatus(baseUrl, runId, "completed");

  assert.equal(completed.result.summary.generated, 1);
  assert.equal(completed.result.timeSeries.length, 3);
  const timeSeriesResponse = await fetch(
    `${baseUrl}/api/simulations/${runId}/timeseries`
  );
  const timeSeries = await timeSeriesResponse.json();
  assert.equal(timeSeries.timeSeries.length, 3);
});

test("reports process failures without returning machine details", async () => {
  const originalError = console.error;
  console.error = () => {};
  try {
    const baseUrl = await startServer(async () => {
      throw new Error("compiler failed at C:\\private\\machine");
    });
    const started = await postRun(baseUrl, {
      ticks: 3,
      mode: "adaptive",
      seed: 12,
    });
    const { runId } = await started.json();
    const failed = await waitForStatus(baseUrl, runId, "failed");

    assert.match(failed.error, /could not be built or completed/);
    assert.doesNotMatch(failed.error, /private|machine|compiler/);
  } finally {
    console.error = originalError;
  }
});

test("prevents overlapping runs", async () => {
  let release;
  const pending = new Promise((resolve) => {
    release = resolve;
  });
  const baseUrl = await startServer(async (request) => {
    await pending;
    return validResult(request);
  });
  const first = await postRun(baseUrl, {
    ticks: 1,
    mode: "fixed",
    seed: 1,
  });
  const { runId } = await first.json();
  const second = await postRun(baseUrl, {
    ticks: 1,
    mode: "fixed",
    seed: 2,
  });

  assert.equal(second.status, 409);
  release();
  await waitForStatus(baseUrl, runId, "completed");
});
