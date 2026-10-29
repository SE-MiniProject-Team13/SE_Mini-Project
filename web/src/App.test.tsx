// @vitest-environment jsdom

import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { afterEach, describe, expect, it, vi } from "vitest";
import App from "./App";

const firstRunId = "11111111-1111-4111-8111-111111111111";
const unavailableRunId = "22222222-2222-4222-8222-222222222222";

function jsonResponse(payload: unknown, status = 200): Response {
  return new Response(JSON.stringify(payload), {
    status,
    headers: { "Content-Type": "application/json" },
  });
}

function completedFirstRun() {
  return {
    runId: firstRunId,
    status: "completed",
    mode: "fixed",
    ticks: 1,
    seed: 7,
    error: null,
    result: {
      runId: firstRunId,
      status: "completed",
      mode: "fixed",
      seed: 7,
      summary: {
        generated: 2,
        served: 0,
        blocked: 1,
        waiting: 1,
        averageWaitingTime: null,
        maximumWaitingTime: null,
        maximumQueueLength: 1,
        throughput: 0,
        emergencyServed: 0,
        averageEmergencyWaitingTime: null,
        maximumEmergencyWaitingTime: null,
        preemptionCount: 0,
        vehicleCountInvariantPassed: true,
        signalSafetyPassed: true,
        elapsedTicks: 1,
      },
      timeSeries: [
        {
          tick: 0,
          state: "NS_GREEN",
          preemptionActive: false,
          queues: { North: 1, South: 0, East: 0, West: 0 },
          signals: {
            North: "GREEN",
            South: "GREEN",
            East: "RED",
            West: "RED",
          },
        },
        {
          tick: 1,
          state: "NS_GREEN",
          preemptionActive: false,
          queues: { North: 1, South: 0, East: 0, West: 0 },
          signals: {
            North: "GREEN",
            South: "GREEN",
            East: "RED",
            West: "RED",
          },
        },
      ],
      events: [
        {
          tick: 0,
          vehicleId: 1,
          type: "ARRIVAL",
          direction: "North",
          emergency: true,
          waitingTime: null,
        },
        {
          tick: 0,
          vehicleId: 2,
          type: "REJECTION",
          direction: "North",
          emergency: true,
          waitingTime: null,
        },
      ],
    },
  };
}

afterEach(() => {
  vi.unstubAllGlobals();
});

describe("selected run analytics", () => {
  it("counts rejected emergency attempts and clears old results if the next run cannot load", async () => {
    let resolveUnavailableRun: ((response: Response) => void) | undefined;
    vi.stubGlobal(
      "fetch",
      vi.fn((input: RequestInfo | URL) => {
        const url = String(input);
        if (url === "/api/simulations") {
          return Promise.resolve(
            jsonResponse([
              {
                runId: firstRunId,
                status: "completed",
                createdAt: "2026-01-01T00:00:00.000Z",
                mode: "fixed",
                ticks: 1,
                seed: 7,
              },
              {
                runId: unavailableRunId,
                status: "completed",
                createdAt: "2026-01-02T00:00:00.000Z",
                mode: "fixed",
                ticks: 1,
                seed: 8,
              },
            ])
          );
        }
        if (url === `/api/simulations/${firstRunId}`) {
          return Promise.resolve(jsonResponse(completedFirstRun()));
        }
        if (url === `/api/simulations/${unavailableRunId}`) {
          return new Promise<Response>((resolve) => {
            resolveUnavailableRun = resolve;
          });
        }
        throw new Error(`Unexpected request: ${url}`);
      })
    );

    render(<App />);
    const selector = screen.getByRole("combobox", {
      name: "Select a previous simulation run",
    });
    await waitFor(() => expect(selector.querySelectorAll("option")).toHaveLength(3));

    fireEvent.change(selector, { target: { value: firstRunId } });
    await screen.findByText("Vehicles generated");
    expect(screen.getByText("2", { selector: ".kpi-value" })).toBeTruthy();

    fireEvent.click(screen.getAllByRole("button", { name: "Generate graphs" })[0]);
    expect(
      await screen.findByText(
        "Arrival attempts: 2 · accepted: 1 · rejected: 1 · served: 0 · pre-emptions: 0"
      )
    ).toBeTruthy();

    fireEvent.change(selector, { target: { value: unavailableRunId } });
    await screen.findByText("Loading selected run…");
    expect(screen.queryByText("Vehicles generated")).toBeNull();
    expect(screen.queryByText("Simulation graphs")).toBeNull();
    expect(screen.queryByText(firstRunId)).toBeNull();
    expect(resolveUnavailableRun).toBeTypeOf("function");

    resolveUnavailableRun?.(jsonResponse({ error: "Run not found." }, 404));
    expect(await screen.findByText("Could not load selected run")).toBeTruthy();
    expect((await screen.findByRole("alert")).textContent).toContain(
      "Run not found."
    );
    expect(screen.queryByText("Vehicles generated")).toBeNull();
    expect(screen.queryByText("2", { selector: ".kpi-value" })).toBeNull();
    expect(screen.queryByText("Simulation graphs")).toBeNull();
    expect(
      screen.getByText(unavailableRunId.slice(0, 8).toUpperCase())
    ).toBeTruthy();
    expect(screen.queryByText(firstRunId)).toBeNull();
  });
});
