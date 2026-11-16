export type ControlSource = "technician" | "teacher" | "physical" | "automation" | "system";

export interface Device {
  id: string;
  name: string;
  channel: number;
  commandedState: boolean;
  source: ControlSource;
  priority: number;
  lastChangedAtMs: number;
  confirmedPhysicalState: false;
}

export interface CustomRelayGroup {
  slot: number;
  name: string;
  channels: number[];
}

export interface PhysicalButtonMapping {
  index: number;
  pin: number;
  name: string;
  channels: number[];
}

export interface ScheduleRule {
  slot: number;
  name: string;
  channels: number[];
  startTime: string;
  endTime: string;
  days: number[];
  startState: boolean;
  endState: boolean;
  enabled: boolean;
}

export interface RelayTimer {
  channel: number;
  remainingSeconds: number;
}

export interface EnergyReading {
  valid: boolean;
  timestampMs: number;
  voltage?: number;
  current?: number;
  power?: number;
  energy?: number;
  frequency?: number;
  powerFactor?: number;
}

export interface ControllerStatus {
  controllerOnline: boolean;
  ip: string;
  wifiConnected: boolean;
  timeSynced: boolean;
  rssi: number;
  mode: "remote" | "manual";
  physicalAllOffLock: boolean;
  safetyInterlock: boolean;
  commandedOnCount: number;
  relayMask: number;
  relayFeedbackAvailable: false;
  role: "technician" | "teacher";
  energy: EnergyReading;
}

export interface ActivityEntry {
  timestampMs: number;
  source: ControlSource;
  target: string;
  action: string;
}

export interface AlertEntry {
  severity: "INFO" | "WARNING" | "CRITICAL";
  message: string;
}

export interface Snapshot {
  status: ControllerStatus;
  devices: Device[];
  customGroups: CustomRelayGroup[];
  physicalButtons: PhysicalButtonMapping[];
  schedules: ScheduleRule[];
  timers: RelayTimer[];
  logs: ActivityEntry[];
  alerts: AlertEntry[];
}

export interface PowerSample {
  at: number;
  watts: number;
}
