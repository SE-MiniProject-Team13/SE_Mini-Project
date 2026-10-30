import json
import tkinter as tk
from tkinter import ttk
from pathlib import Path
from collections import deque

STATE_FILE = Path(__file__).resolve().parent / "dashboard_state.json"
DIRECTIONS = ["North", "East", "South", "West"]
COLORS = {"GREEN": "#22c55e", "YELLOW": "#facc15", "RED": "#ef4444"}

class TrafficDashboard:
    def __init__(self, root):
        self.root = root
        self.root.title("Traffic Management Simulation | Live Dashboard")
        self.root.geometry("1120x760")
        self.root.minsize(900, 650)
        self.root.configure(bg="#eef2f7")
        self.history = deque(maxlen=60)
        self.last_tick = None

        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TFrame", background="#eef2f7")
        style.configure("TLabel", background="#eef2f7", fg="#172033", font=("Segoe UI", 10))
        style.configure("Title.TLabel", font=("Segoe UI", 19, "bold"))
        style.configure("Metric.TLabel", font=("Segoe UI", 18, "bold"))
        style.configure("Small.TLabel", fg="#64748b", font=("Segoe UI", 9))

        header = ttk.Frame(root, padding=(18, 14))
        header.pack(fill="x")
        ttk.Label(header, text="TRAFFIC CONTROL CENTER", style="Title.TLabel").pack(side="left")
        self.connection = ttk.Label(header, text="Waiting for C++ simulation…")
        self.connection.pack(side="right")

        metrics = ttk.Frame(root, padding=(14, 0, 14, 12))
        metrics.pack(fill="x")
        self.metric_values = {}
        for key, title in [
            ("tick", "SIMULATION TICK"),
            ("served", "VEHICLES SERVED"),
            ("waiting", "VEHICLES WAITING"),
            ("average_wait", "AVERAGE WAIT (TICKS)")
        ]:
            card = tk.Frame(metrics, bg="white", highlightbackground="#dbe2ea", highlightthickness=1)
            card.pack(side="left", fill="both", expand=True, padx=4)
            tk.Label(card, text=title, bg="white", fg="#64748b", font=("Segoe UI", 9, "bold")).pack(anchor="w", padx=12, pady=(10, 2))
            value = tk.Label(card, text="—", bg="white", fg="#172033", font=("Segoe UI", 19, "bold"))
            value.pack(anchor="w", padx=12, pady=(0, 10))
            self.metric_values[key] = value

        middle = ttk.Frame(root, padding=(14, 0, 14, 12))
        middle.pack(fill="both", expand=True)
        road_frame = tk.Frame(middle, bg="white", highlightbackground="#dbe2ea", highlightthickness=1)
        road_frame.pack(side="left", fill="both", expand=True, padx=(4, 8))
        tk.Label(road_frame, text="LIVE INTERSECTION", bg="white", fg="#172033",
                 font=("Segoe UI", 11, "bold")).pack(anchor="w", padx=12, pady=10)
        self.road = tk.Canvas(road_frame, bg="#f8fafc", highlightthickness=0, height=440)
        self.road.pack(fill="both", expand=True, padx=8, pady=(0, 8))
        self.road.bind("<Configure>", lambda _event: self.draw_intersection(self.current_state))

        side = tk.Frame(middle, bg="white", width=290, highlightbackground="#dbe2ea", highlightthickness=1)
        side.pack(side="right", fill="y", padx=(8, 4))
        side.pack_propagate(False)
        tk.Label(side, text="SIGNAL & QUEUE STATUS", bg="white", fg="#172033",
                 font=("Segoe UI", 11, "bold")).pack(anchor="w", padx=12, pady=10)
        self.signal_labels = {}
        self.queue_labels = {}
        self.queue_bars = {}
        for direction in DIRECTIONS:
            row = tk.Frame(side, bg="white")
            row.pack(fill="x", padx=12, pady=8)
            lamp = tk.Canvas(row, width=22, height=22, bg="white", highlightthickness=0)
            lamp.pack(side="left")
            lamp_id = lamp.create_oval(3, 3, 19, 19, fill="#94a3b8", outline="")
            name = tk.Label(row, text=direction, bg="white", fg="#172033",
                            font=("Segoe UI", 10, "bold"), width=7, anchor="w")
            name.pack(side="left", padx=5)
            count = tk.Label(row, text="Queue: —", bg="white", fg="#334155", width=10, anchor="e")
            count.pack(side="right")
            self.signal_labels[direction] = (lamp, lamp_id)
            self.queue_labels[direction] = count
            bar = tk.Canvas(side, height=9, bg="#e2e8f0", highlightthickness=0)
            bar.pack(fill="x", padx=16, pady=(0, 5))
            self.queue_bars[direction] = bar

        self.phase_label = tk.Label(side, text="Signal phase: —", bg="white", fg="#334155",
                                    font=("Segoe UI", 10), justify="left")
        self.phase_label.pack(anchor="w", padx=12, pady=10)
        self.preempt_label = tk.Label(side, text="", bg="white", fg="#b45309",
                                      font=("Segoe UI", 9, "bold"))
        self.preempt_label.pack(anchor="w", padx=12)

        chart_frame = tk.Frame(root, bg="white", highlightbackground="#dbe2ea", highlightthickness=1)
        chart_frame.pack(fill="x", padx=18, pady=(0, 16))
        tk.Label(chart_frame, text="QUEUE LENGTH TREND", bg="white", fg="#172033",
                 font=("Segoe UI", 11, "bold")).pack(anchor="w", padx=12, pady=(10, 0))
        self.chart = tk.Canvas(chart_frame, height=130, bg="white", highlightthickness=0)
        self.chart.pack(fill="x", padx=10, pady=6)
        self.current_state = None
        self.root.after(300, self.refresh)

    def draw_intersection(self, state):
        c = self.road
        c.delete("all")
        w = max(c.winfo_width(), 400)
        h = max(c.winfo_height(), 300)
        cx, cy = w / 2, h / 2
        road_w = min(w * 0.42, 210)
        road_h = min(h * 0.72, 310)
        # Roads and junction
        c.create_rectangle(cx-road_w/2, 8, cx+road_w/2, h-8, fill="#555d66", outline="#303740", width=2)
        c.create_rectangle(8, cy-road_h/2, w-8, cy+road_h/2, fill="#555d66", outline="#303740", width=2)
        # Dashed lane markings
        for y in range(15, h-10, 22):
            c.create_line(cx, y, cx, min(y+11, h-10), fill="#f8fafc", width=2)
        for x in range(15, w-10, 22):
            c.create_line(x, cy, min(x+11, w-10), cy, fill="#f8fafc", width=2)
        # Stop lines
        for x1, y1, x2, y2 in [
            (cx-road_w/2+8, cy-road_h/2, cx+road_w/2-8, cy-road_h/2),
            (cx-road_w/2+8, cy+road_h/2, cx+road_w/2-8, cy+road_h/2),
            (cx-road_w/2, cy-road_h/2+8, cx-road_w/2, cy+road_h/2-8),
            (cx+road_w/2, cy-road_h/2+8, cx+road_w/2, cy+road_h/2-8),
        ]:
            c.create_line(x1, y1, x2, y2, fill="white", width=4)
        # Labels and signal lamps positioned at each approach
        positions = {
            "North": (cx + road_w/2 + 28, cy-road_h/2 + 25),
            "South": (cx - road_w/2 - 28, cy+road_h/2 - 25),
            "East": (cx+road_w/2-25, cy+road_h/2+28),
            "West": (cx-road_w/2+25, cy-road_h/2-28),
        }
        queues = (state or {}).get("queues", {})
        lights = (state or {}).get("lights", {})
        capacity = max(1, int((state or {}).get("capacity", 100)))
        for direction, (x, y) in positions.items():
            color = COLORS.get(lights.get(direction, "Red"), "#94a3b8")
            c.create_oval(x-12, y-12, x+12, y+12, fill=color, outline="white", width=2)
            q = queues.get(direction, 0)
            c.create_text(x, y + (24 if direction in ("North", "West") else -24),
                          text=f"{direction}  |  Queue {q}", fill="#172033",
                          font=("Segoe UI", 9, "bold"))
            # Small queue blocks, capped visually but using real count in label
            for i in range(min(int(q), 8)):
                if direction == "North":
                    bx, by = cx + 20 + (i % 2) * 17, cy-road_h/2-18-(i//2)*17
                elif direction == "South":
                    bx, by = cx - 37 + (i % 2) * 17, cy+road_h/2+4+(i//2)*17
                elif direction == "East":
                    bx, by = cx+road_w/2+4+(i//2)*17, cy+15+(i%2)*17
                else:
                    bx, by = cx-road_w/2-18-(i//2)*17, cy-32+(i%2)*17
                c.create_rectangle(bx-5, by-5, bx+5, by+5, fill="#93c5fd", outline="#1d4ed8")
        c.create_text(cx, cy, text="4-WAY\nJUNCTION", fill="white", font=("Segoe UI", 10, "bold"))

    def draw_chart(self):
        c = self.chart
        c.delete("all")
        w = max(c.winfo_width(), 500)
        h = 130
        left, right, top, bottom = 34, w-12, 10, h-24
        c.create_line(left, top, left, bottom, fill="#94a3b8")
        c.create_line(left, bottom, right, bottom, fill="#94a3b8")
        for i in range(4):
            y = top + (bottom-top)*i/3
            c.create_line(left, y, right, y, fill="#e2e8f0", dash=(2, 3))
        if len(self.history) < 2:
            c.create_text(w/2, h/2, text="Queue trend will appear as the simulation advances",
                          fill="#64748b", font=("Segoe UI", 9))
            return
        max_value = max(1, max(sum(point.values()) for point in self.history))
        points = []
        for index, point in enumerate(self.history):
            x = left + (right-left)*index/(len(self.history)-1)
            y = bottom - (bottom-top)*sum(point.values())/max_value
            points.extend([x, y])
        c.create_line(*points, fill="#2563eb", width=3, smooth=True)
        c.create_text(6, top, anchor="nw", text=str(max_value), fill="#64748b", font=("Segoe UI", 8))
        c.create_text(6, bottom, anchor="sw", text="0", fill="#64748b", font=("Segoe UI", 8))

    def refresh(self):
        try:
            if STATE_FILE.exists():
                # A read may coincide with C++ rewriting the file; retry next refresh if incomplete.
                state = json.loads(STATE_FILE.read_text(encoding="utf-8"))
                self.current_state = state
                tick = state.get("tick")
                if tick != self.last_tick:
                    self.history.append(state.get("queues", {}))
                    self.last_tick = tick
                self.connection.config(text=f"Connected • tick {tick}/{state.get('duration', '?')}",foreground="#15803d")
                for key in ("tick", "served", "waiting"):
                    self.metric_values[key].config(text=str(state.get(key, 0)))
                self.metric_values["average_wait"].config(text=f"{state.get('average_wait', 0):.2f}")
                queues = state.get("queues", {})
                lights = state.get("lights", {})
                capacity = max(1, int(state.get("capacity", 100)))
                for direction in DIRECTIONS:
                    lamp, lamp_id = self.signal_labels[direction]
                    lamp.itemconfig(lamp_id, fill=COLORS.get(lights.get(direction), "#94a3b8"))
                    count = int(queues.get(direction, 0))
                    self.queue_labels[direction].config(text=f"Queue: {count}")
                    bar = self.queue_bars[direction]
                    bar.delete("all")
                    bar.update_idletasks()
                    bw = max(80, bar.winfo_width())
                    bar.create_rectangle(0, 1, bw, 8, fill="#e2e8f0", outline="")
                    fill_width = min(bw, bw * count / capacity)
                    bar.create_rectangle(0, 1, fill_width, 8,
                                          fill="#ef4444" if count / capacity >= 0.8 else "#22c55e",
                                          outline="")
                self.phase_label.config(text=(
                    f"Phase: {state.get('signal_state', '—')}\n"
                    f"Ticks remaining: {state.get('ticks_remaining', '—')}\n"
                    f"Mode: {state.get('mode', '—')}\n"
                    f"Throughput: {state.get('throughput', 0):.3f} vehicles/tick"
                ))
                self.preempt_label.config(text="EMERGENCY PRE-EMPTION ACTIVE" if state.get("preemption") else "")
                self.draw_intersection(state)
                self.draw_chart()
            else:
                self.connection.config(text="Waiting for dashboard_state.json…", foreground="#b45309")
        except (OSError, ValueError, TypeError, KeyError):
            pass
        self.root.after(300, self.refresh)

def main():
    root = tk.Tk()
    TrafficDashboard(root)
    root.mainloop()

if __name__ == "__main__":
    main()
