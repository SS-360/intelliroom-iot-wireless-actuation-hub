import type { ActivityEntry, AlertEntry, ControllerStatus, CustomRelayGroup, Device, PhysicalButtonMapping, RelayTimer, ScheduleRule, Snapshot } from "./types";

export class ApiError extends Error {
  status: number;

  constructor(message: string, status: number) {
    super(message);
    this.status = status;
  }
}

const canonicalDevices: Device[] = [
  [1, "light_1", "Channel 1"], [2, "light_2", "Channel 2"],
  [3, "light_3", "Channel 3"], [4, "light_4", "Channel 4"],
  [5, "fan_1", "Channel 5"], [6, "fan_2", "Channel 6"],
  [7, "fan_3", "Channel 7"], [8, "fan_4", "Channel 8"],
].map(([channel, id, name], index) => ({
  channel: channel as number,
  id: id as string,
  name: name as string,
  commandedState: [0, 1, 4].includes(index),
  source: index === 8 ? "physical" : index < 4 ? "teacher" : "automation",
  priority: index === 8 ? 3 : index < 4 ? 2 : 1,
  lastChangedAtMs: 180_000 - index * 7_400,
  confirmedPhysicalState: false,
}));

let mockDevices = canonicalDevices;
let mockGroups: CustomRelayGroup[] = Array.from({ length: 4 }, (_, slot) => ({ slot, name: "", channels: [] }));
let mockPhysicalButtons: PhysicalButtonMapping[] = [
  { index: 0, pin: 32, name: "Button 1", channels: [1, 2, 3, 4] },
  { index: 1, pin: 33, name: "Button 2", channels: [5, 6, 7, 8] },
  { index: 2, pin: 25, name: "Button 3", channels: [1, 3, 5, 7] },
  { index: 3, pin: 26, name: "Button 4", channels: [2, 4, 6, 8] },
];
let mockSchedules: ScheduleRule[] = Array.from({ length: 4 }, (_, slot) => ({ slot, name: "", channels: [], startTime: "08:00", endTime: "17:00", days: [1, 2, 3, 4, 5], startState: true, endState: false, enabled: false }));
let mockTimers: RelayTimer[] = Array.from({ length: 8 }, (_, index) => ({ channel: index + 1, remainingSeconds: 0 }));
let mockTimerEnds: Record<number, number> = {};
let lastMockScheduleMinute = "";
let mockLogs: ActivityEntry[] = [
  { timestampMs: Date.now() - 192_000, source: "teacher", target: "light_2", action: "ON" },
  { timestampMs: Date.now() - 166_000, source: "automation", target: "fan_1", action: "ON" },
  { timestampMs: Date.now() - 124_000, source: "physical", target: "light_1", action: "ON" },
  { timestampMs: Date.now() - 61_000, source: "system", target: "controller", action: "BOOT" },
];
const mockStartedAt = Date.now() - 228_000;

export const isDemoMode = import.meta.env.VITE_HARDWARE_MODE === "mock" ||
  ["localhost", "127.0.0.1"].includes(window.location.hostname);

async function requestJson<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(path, {
    credentials: "same-origin",
    headers: { "Content-Type": "application/json", ...init?.headers },
    ...init,
  });
  const body = await response.json().catch(() => ({})) as Record<string, unknown>;
  if (!response.ok) {
    const message = String(body.error ?? body.reason ?? `HTTP_${response.status}`);
    throw new ApiError(message.replaceAll("_", " "), response.status);
  }
  return body as T;
}

function mockStatus(): ControllerStatus {
  const elapsed = Date.now() - mockStartedAt;
  const power = 914 + Math.sin(elapsed / 18_000) * 110 + Math.sin(elapsed / 5_200) * 32;
  return {
    controllerOnline: true,
    ip: "192.168.137.45",
    wifiConnected: true,
    timeSynced: true,
    rssi: -52,
    mode: "remote",
    physicalAllOffLock: false,
    safetyInterlock: false,
    commandedOnCount: mockDevices.filter((device) => device.commandedState).length,
    relayMask: mockDevices.reduce((mask, device) => device.commandedState ? mask | (1 << (device.channel - 1)) : mask, 0),
    relayFeedbackAvailable: false,
    role: "technician",
    energy: {
      valid: true,
      timestampMs: elapsed,
      voltage: 229.4,
      current: power / 229.4,
      power,
      energy: 2.84,
      frequency: 50.0,
      powerFactor: 0.91,
    },
  };
}

const wait = (duration = 160) => new Promise((resolve) => window.setTimeout(resolve, duration));

async function getMockSnapshot(): Promise<Snapshot> {
  await wait(90);
  const now = new Date();
  const minuteKey = `${now.toDateString()}-${now.getHours()}:${now.getMinutes()}`;
  if (minuteKey !== lastMockScheduleMinute) {
    lastMockScheduleMinute = minuteKey;
    const weekday = now.getDay();
    const time = `${String(now.getHours()).padStart(2, "0")}:${String(now.getMinutes()).padStart(2, "0")}`;
    const previousWeekday = (weekday + 6) % 7;
    for (const rule of mockSchedules) {
      const overnight = rule.endTime < rule.startTime;
      const isStart = rule.enabled && rule.startTime === time && rule.days.includes(weekday);
      const isEnd = rule.enabled && rule.endTime === time && rule.days.includes(overnight ? previousWeekday : weekday);
      if (!isStart && !isEnd) continue;
      const state = isStart ? rule.startState : rule.endState;
      mockDevices = mockDevices.map((device) => rule.channels.includes(device.channel) ? { ...device, commandedState: state } : device);
      addMockLog("automation", rule.name, isStart ? (state ? "SCHEDULE_START_ON" : "SCHEDULE_START_OFF") : (state ? "SCHEDULE_END_ON" : "SCHEDULE_END_OFF"));
    }
  }
  for (const [channelText, deadline] of Object.entries(mockTimerEnds)) {
    const channel = Number(channelText);
    if (deadline <= Date.now()) {
      mockDevices = mockDevices.map((device) => device.channel === channel ? { ...device, commandedState: false } : device);
      delete mockTimerEnds[channel];
    }
  }
  mockTimers = mockTimers.map((timer) => ({ ...timer, remainingSeconds: Math.max(0, Math.ceil(((mockTimerEnds[timer.channel] ?? 0) - Date.now()) / 1000)) }));
  return {
    status: mockStatus(),
    devices: mockDevices.map((device) => ({ ...device })),
    customGroups: mockGroups.map((group) => ({ ...group, channels: [...group.channels] })),
    physicalButtons: mockPhysicalButtons.map((button) => ({ ...button, channels: [...button.channels] })),
    schedules: mockSchedules.map((rule) => ({ ...rule, channels: [...rule.channels], days: [...rule.days] })),
    timers: mockTimers.map((timer) => ({ ...timer })),
    logs: mockLogs.map((entry) => ({ ...entry })),
    alerts: [],
  };
}

function addMockLog(source: ActivityEntry["source"], target: string, action: string) {
  mockLogs = [...mockLogs, { timestampMs: Date.now(), source, target, action }].slice(-24);
}

export const api = {
  async login(username: string, password: string) {
    if (isDemoMode) return { authenticated: true, role: "technician" as const };
    return requestJson<{ authenticated: true; role: "technician" | "teacher" }>("/api/v1/auth/login", {
      method: "POST",
      body: JSON.stringify({ username, password }),
    });
  },

  async logout() {
    if (isDemoMode) return;
    await requestJson("/api/v1/auth/logout", { method: "POST", body: "{}" });
  },

  async snapshot(): Promise<Snapshot> {
    if (isDemoMode) return getMockSnapshot();
    const [status, devices, logs, alerts, customGroups, physicalButtons, schedules, timers] = await Promise.all([
      requestJson<ControllerStatus>("/api/v1/status"),
      requestJson<Device[]>("/api/v1/devices"),
      requestJson<ActivityEntry[]>("/api/v1/logs"),
      requestJson<AlertEntry[]>("/api/v1/alerts"),
      requestJson<CustomRelayGroup[]>("/api/v1/groups/custom"),
      requestJson<PhysicalButtonMapping[]>("/api/v1/buttons/custom"),
      requestJson<ScheduleRule[]>("/api/v1/schedules"),
      requestJson<RelayTimer[]>("/api/v1/timers"),
    ]);
    return { status, devices, customGroups, physicalButtons, schedules, timers, logs, alerts };
  },

  async setDevice(device: Device, state: boolean) {
    if (isDemoMode) {
      await wait();
      mockDevices = mockDevices.map((item) => item.id === device.id
        ? { ...item, commandedState: state, source: "technician", priority: 1, lastChangedAtMs: Date.now() - mockStartedAt }
        : item);
      addMockLog("technician", device.id, state ? "ON" : "OFF");
      return;
    }
    await requestJson(`/api/v1/devices/${device.id}/control`, { method: "POST", body: JSON.stringify({ state }) });
  },

  async renameDevice(id: string, name: string) {
    if (isDemoMode) {
      await wait();
      mockDevices = mockDevices.map((item) => item.id === id ? { ...item, name } : item);
      return;
    }
    await requestJson(`/api/v1/devices/${id}/name`, { method: "POST", body: JSON.stringify({ name }) });
  },

  async saveCustomGroup(slot: number, name: string, channels: number[]) {
    if (isDemoMode) {
      await wait();
      mockGroups = mockGroups.map((group) => group.slot === slot ? { slot, name, channels: [...channels] } : group);
      return;
    }
    await requestJson(`/api/v1/groups/custom/${slot}`, { method: "POST", body: JSON.stringify({ name, channels }) });
  },

  async setCustomGroup(slot: number, state: boolean) {
    const group = mockGroups.find((candidate) => candidate.slot === slot);
    if (isDemoMode && group) {
      await wait();
      mockDevices = mockDevices.map((device) => group.channels.includes(device.channel)
        ? { ...device, commandedState: state, source: "technician", priority: 1, lastChangedAtMs: Date.now() - mockStartedAt }
        : device);
      addMockLog("technician", group.name, state ? "GROUP_ON" : "GROUP_OFF");
      return;
    }
    await requestJson(`/api/v1/groups/custom/${slot}/control`, { method: "POST", body: JSON.stringify({ state }) });
  },

  async savePhysicalButtons(buttons: PhysicalButtonMapping[]) {
    if (isDemoMode) {
      await wait();
      mockPhysicalButtons = buttons.map((button) => ({ ...button, channels: [...button.channels] }));
      mockGroups = buttons.map((button) => ({ slot: button.index, name: button.name, channels: [...button.channels] }));
      return;
    }
    await requestJson("/api/v1/buttons/custom", { method: "POST", body: JSON.stringify({ buttons: buttons.map(({ name, channels }) => ({ name, channels })) }) });
  },

  async saveSchedules(schedules: ScheduleRule[]) {
    if (isDemoMode) {
      await wait();
      mockSchedules = schedules.map((rule) => ({ ...rule, channels: [...rule.channels], days: [...rule.days] }));
      return;
    }
    await requestJson("/api/v1/schedules", { method: "POST", body: JSON.stringify({ schedules }) });
  },

  async setTimer(channel: number, durationSeconds: number) {
    if (isDemoMode) {
      await wait();
      if (durationSeconds > 0) {
        mockDevices = mockDevices.map((device) => device.channel === channel ? { ...device, commandedState: true, source: "teacher", lastChangedAtMs: Date.now() - mockStartedAt } : device);
        mockTimerEnds[channel] = Date.now() + durationSeconds * 1000;
      } else delete mockTimerEnds[channel];
      mockTimers = mockTimers.map((timer) => timer.channel === channel ? { ...timer, remainingSeconds: durationSeconds } : timer);
      addMockLog("teacher", `Channel ${channel}`, durationSeconds ? "TIMER_STARTED" : "TIMER_CANCELLED");
      return;
    }
    await requestJson(`/api/v1/timers/${channel}`, { method: "POST", body: JSON.stringify({ seconds: durationSeconds }) });
  },

  async setGroup(group: "lights" | "fans", state: boolean) {
    if (isDemoMode) {
      await wait();
      const range = group === "lights" ? [1, 4] : [5, 8];
      mockDevices = mockDevices.map((item) => item.channel >= range[0] && item.channel <= range[1]
        ? { ...item, commandedState: state, source: "technician", priority: 1, lastChangedAtMs: Date.now() - mockStartedAt }
        : item);
      addMockLog("technician", group, state ? "GROUP_ON" : "GROUP_OFF");
      return;
    }
    await requestJson(`/api/v1/groups/${group}`, { method: "POST", body: JSON.stringify({ state }) });
  },

  async setAll(state: boolean) {
    if (isDemoMode) {
      await wait();
      mockDevices = mockDevices.map((item) => ({ ...item, commandedState: state, source: "technician", priority: 1, lastChangedAtMs: Date.now() - mockStartedAt }));
      addMockLog("technician", "all_devices", state ? "ALL_ON" : "ALL_OFF");
      return;
    }
    await requestJson(`/api/v1/system/all-${state ? "on" : "off"}`, { method: "POST", body: "{}" });
  },
};
