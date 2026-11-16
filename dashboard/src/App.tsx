import { useCallback, useEffect, useMemo, useState } from "react";
import { ApiError, api, isDemoMode } from "./api";
import type { ActivityEntry, AlertEntry, CustomRelayGroup, Device, PhysicalButtonMapping, PowerSample, ScheduleRule, Snapshot } from "./types";

type IconName = "overview" | "devices" | "energy" | "schedule" | "automation" | "activity" | "alerts" | "system" | "refresh" | "logout" | "menu" | "bolt" | "signal" | "shield" | "clock" | "palette";
type ThemeMode = "light" | "dark";
type AccentTheme = "ember" | "blue" | "violet";
type PageKey = "home" | "controls" | "setup" | "activity" | "about";
const pageItems: { key: PageKey; title: string }[] = [
  { key: "home", title: "Home" }, { key: "controls", title: "Controls" }, { key: "setup", title: "Setup" }, { key: "activity", title: "Activity" }, { key: "about", title: "About" },
];

const iconPaths: Record<IconName, string[]> = {
  overview: ["M4 4h6v6H4z", "M14 4h6v6h-6z", "M4 14h6v6H4z", "M14 14h6v6h-6z"],
  devices: ["M7 3h10v7H7z", "M9 21h6", "M12 10v4", "M5 14h14v4H5z"],
  energy: ["M13 2 5 14h7l-1 8 8-12h-7z"],
  schedule: ["M6 3v3", "M18 3v3", "M4 8h16", "M5 5h14v16H5z", "M9 12h2v2H9z", "M14 12h2v2h-2z"],
  automation: ["M4 7h10", "M14 4l3 3-3 3", "M20 17H10", "M10 14l-3 3 3 3"],
  activity: ["M3 12h4l2-5 4 10 2-5h6"],
  alerts: ["M12 3 2 21h20z", "M12 9v5", "M12 18h.01"],
  system: ["M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8z", "M4.9 4.9l2.2 2.2", "M16.9 16.9l2.2 2.2", "M19.1 4.9l-2.2 2.2", "M7.1 16.9l-2.2 2.2", "M12 2v3", "M12 19v3", "M2 12h3", "M19 12h3"],
  refresh: ["M20 7v5h-5", "M4 17v-5h5", "M6.1 8a7 7 0 0 1 11.2-2L20 9", "M17.9 16a7 7 0 0 1-11.2 2L4 15"],
  logout: ["M10 4H5v16h5", "M14 8l4 4-4 4", "M18 12H9"],
  menu: ["M4 7h16", "M4 12h16", "M4 17h16"],
  bolt: ["M13 2 5 14h7l-1 8 8-12h-7z"],
  signal: ["M5 15a10 10 0 0 1 14 0", "M8 18a6 6 0 0 1 8 0", "M12 21h.01"],
  shield: ["M12 3 20 6v6c0 5-3.4 8.6-8 10-4.6-1.4-8-5-8-10V6z", "m9 12 2 2 4-5"],
  clock: ["M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18z", "M12 7v6l4 2"],
  palette: ["M12 3a9 9 0 1 0 0 18h1a2 2 0 0 0 1.6-3.2 1.8 1.8 0 0 1 1.4-2.9H18a3 3 0 0 0 3-3A9 9 0 0 0 12 3z", "M7.5 10h.01", "M10 7.5h.01", "M14 7.5h.01", "M16.5 10h.01"],
};

function Icon({ name, size = 18 }: { name: IconName; size?: number }) {
  return <svg className="icon" aria-hidden="true" width={size} height={size} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.7" strokeLinecap="square" strokeLinejoin="miter">
    {iconPaths[name].map((path) => <path key={path} d={path} />)}
  </svg>;
}

const sourceLabels: Record<string, string> = {
  technician: "TECH", teacher: "TEACHER", physical: "PHYSICAL", automation: "SCHEDULE", system: "SYSTEM",
};

function formatPower(value?: number) {
  if (value == null || Number.isNaN(value)) return "—";
  if (value >= 1000) return `${(value / 1000).toFixed(2)} kW`;
  return `${Math.round(value)} W`;
}

function formatLogTime(timestampMs: number) {
  if (timestampMs < 1_000_000_000_000) return "Waiting for clock";
  return new Intl.DateTimeFormat(undefined, { dateStyle: "medium", timeStyle: "medium" }).format(timestampMs);
}

function formatTimer(seconds: number) {
  const hours = Math.floor(seconds / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  const remainder = seconds % 60;
  if (hours) return `${hours}h ${minutes}m remaining`;
  if (minutes) return `${minutes}m ${remainder}s remaining`;
  return `${remainder}s remaining`;
}

function targetLabel(target: string, devices: Device[]) {
  return devices.find((device) => device.id === target)?.name ?? target.replaceAll("_", " ");
}

function Brand() {
  return <a href="#home" className="brand" aria-label="IntelliRoom home">
    <span className="brand-mark" aria-hidden="true"><i /><i /><i /></span>
    <span><strong>IntelliRoom</strong><small>Relay and energy console</small></span>
  </a>;
}

function LoginScreen({ onAuthenticated }: { onAuthenticated: () => Promise<void> }) {
  const [username, setUsername] = useState("teacher");
  const [password, setPassword] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");

  async function submit(event: React.FormEvent) {
    event.preventDefault();
    setBusy(true);
    setError("");
    try {
      await api.login(username, password);
      await onAuthenticated();
    } catch (loginError) {
      setError(loginError instanceof Error ? loginError.message : "Unable to sign in");
    } finally {
      setBusy(false);
    }
  }

  return <main className="login-shell">
    <section className="login-scene" aria-label="IntelliRoom">
      <div className="login-scene-top"><Brand /><span>LOCAL CONTROL SYSTEM</span></div>
      <div className="login-scene-copy"><span>INTELLIROOM</span><h1>Control<br /><em>with clarity.</em></h1><p>Switch eight relay channels, monitor electrical load and review real controller activity.</p></div>
      <div className="login-relay-art" aria-hidden="true"><span className="relay-art-label">RELAY CHANNELS&nbsp;&nbsp; · &nbsp;&nbsp;8</span>{Array.from({ length: 8 }, (_, index) => <i key={index} className={index === 1 || index === 4 ? "active" : ""}><b>{String(index + 1).padStart(2, "0")}</b></i>)}</div>
      <small className="login-scene-foot">LOCAL CONTROL INTERFACE <span>•</span> ESP32</small>
    </section>
    <section className="login-panel" aria-labelledby="login-title">
      <div className="login-mobile-brand"><Brand /></div>
      <div className="login-copy">
        <span className="eyebrow">SECURE ACCESS</span>
        <h2 id="login-title">Sign in</h2>
        <p>Sign in to view the controller and manage relay access.</p>
      </div>
      <form onSubmit={submit}>
        <label>Username<input autoComplete="username" value={username} onChange={(event) => setUsername(event.target.value)} /></label>
        <label>Password<input type="password" autoComplete="current-password" value={password} onChange={(event) => setPassword(event.target.value)} /></label>
        {error ? <div className="form-error" role="alert">{error}</div> : null}
        <button className="button button-primary" disabled={busy}>{busy ? "Connecting…" : "Sign in"}</button>
      </form>
      <div className="login-foot"><span className="status-dot online" /> Controller connection is private to your network</div>
    </section>
  </main>;
}

function AppearanceControl({ theme, accent, onThemeChange, onAccentChange }: { theme: ThemeMode; accent: AccentTheme; onThemeChange: (value: ThemeMode) => void; onAccentChange: (value: AccentTheme) => void }) {
  return <details className="appearance-control">
    <summary aria-label="Customize appearance"><Icon name="palette" /><span>Appearance</span></summary>
    <div className="appearance-popover">
      <strong>Appearance</strong>
      <span className="appearance-label">Theme</span>
      <div className="choice-row" role="group" aria-label="Theme">
        {(["light", "dark"] as const).map((value) => <button key={value} type="button" className={theme === value ? "selected" : ""} aria-pressed={theme === value} onClick={() => onThemeChange(value)}>{value === "light" ? "Light" : "Dark"}</button>)}
      </div>
      <span className="appearance-label">Accent</span>
      <div className="accent-choices" role="group" aria-label="Accent color">
        {(["ember", "blue", "violet"] as const).map((value) => <button key={value} type="button" className={`accent-choice accent-${value} ${accent === value ? "selected" : ""}`} aria-label={`${value} accent`} aria-pressed={accent === value} title={`${value} accent`} onClick={() => onAccentChange(value)}><span /></button>)}
      </div>
    </div>
  </details>;
}

function Header({ snapshot, refreshing, onRefresh, onLogout, theme, accent, page, onNavigate, onThemeChange, onAccentChange }: { snapshot: Snapshot; refreshing: boolean; onRefresh: () => void; onLogout: () => void; theme: ThemeMode; accent: AccentTheme; page: PageKey; onNavigate: (page: PageKey) => void; onThemeChange: (value: ThemeMode) => void; onAccentChange: (value: AccentTheme) => void }) {
  const [now, setNow] = useState(() => new Date());
  useEffect(() => {
    const timer = window.setInterval(() => setNow(new Date()), 1000);
    return () => window.clearInterval(timer);
  }, []);

  return <header className="topbar">
    <div className="header-identity"><Brand /></div>
    <div className="topbar-actions">
      {isDemoMode ? <span className="chip demo">DEMO DATA</span> : null}
      <span className="chip"><span className={`status-dot ${snapshot.status.wifiConnected ? "online" : "offline"}`} />ESP32 {snapshot.status.wifiConnected ? "ONLINE" : "OFFLINE"}</span>
      <span className={`chip mode ${snapshot.status.mode}`}>{snapshot.status.mode.toUpperCase()}</span>
      <time dateTime={now.toISOString()}>{now.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })}</time>
      <AppearanceControl theme={theme} accent={accent} onThemeChange={onThemeChange} onAccentChange={onAccentChange} />
      <button className="icon-button" onClick={onRefresh} disabled={refreshing} aria-label="Refresh dashboard"><span className={refreshing ? "spinning" : ""}><Icon name="refresh" /></span></button>
      <button className="profile-button" onClick={onLogout} title="Sign out"><span>{snapshot.status.role.slice(0, 1).toUpperCase()}</span><b>{snapshot.status.role}</b><Icon name="logout" size={16} /></button>
    </div>
    <nav className="primary-nav" aria-label="Main pages">{pageItems.map((item) => <button key={item.key} type="button" className={page === item.key ? "active" : ""} aria-current={page === item.key ? "page" : undefined} onClick={() => onNavigate(item.key)}>{item.title}</button>)}</nav>
  </header>;
}

function MetricStrip({ snapshot }: { snapshot: Snapshot }) {
  const { status } = snapshot;
  const energy = status.energy;
  return <section className="metric-strip" aria-label="Live system metrics">
    <article><span>POWER NOW</span><strong>{formatPower(energy.power)}</strong><small>{energy.valid ? "PZEM LIVE READING" : "PZEM UNAVAILABLE"}</small></article>
    <article><span>ENERGY TODAY</span><strong>{energy.energy != null ? `${energy.energy.toFixed(2)} kWh` : "—"}</strong><small>CUMULATIVE METER</small></article>
    <article><span>RELAYS ON</span><strong>{status.commandedOnCount}<i>/8</i></strong><small>COMMANDED STATE</small></article>
    <article><span>LINE STATUS</span><strong>{energy.voltage != null ? `${energy.voltage.toFixed(1)} V` : "—"}</strong><small>{energy.frequency != null ? `${energy.frequency.toFixed(1)} HZ · PF ${energy.powerFactor?.toFixed(2)}` : "AWAITING SENSOR"}</small></article>
  </section>;
}

function Toggle({ checked, disabled, label, onChange }: { checked: boolean; disabled: boolean; label: string; onChange: () => void }) {
  return <button type="button" className={`toggle ${checked ? "on" : "off"}`} role="switch" aria-checked={checked} aria-label={label} disabled={disabled} onClick={onChange}>
    <span /><b>{checked ? "ON" : "OFF"}</b>
  </button>;
}

function RelayTile({ device, pending, onToggle }: { device: Device; pending: boolean; onToggle: (device: Device) => void }) {
  return <button type="button" aria-label={`Turn ${device.name} ${device.commandedState ? "off" : "on"}`} aria-pressed={device.commandedState} className={`relay-tile ${device.commandedState ? "is-on" : ""}`} disabled={pending} onClick={() => onToggle(device)}>
    <span className="relay-tile-number">{String(device.channel).padStart(2, "0")}</span>
    <strong>{device.name}</strong>
  </button>;
}

function DeviceGroup({ title, devices, pending, onToggle }: { title: string; devices: Device[]; pending: string; onToggle: (device: Device) => void }) {
  return <section className="device-group">
    <header><div><h2>{title}</h2></div><small>{devices.filter((device) => device.commandedState).length} / {devices.length} on</small></header>
    <div className="relay-grid">{devices.map((device) => <RelayTile key={device.id} device={device} pending={pending === device.id || pending === "all"} onToggle={onToggle} />)}</div>
  </section>;
}

function RelayStatusGrid({ devices }: { devices: Device[] }) {
  return <section className="status-grid-panel panel" aria-label="Relay status">
    <header className="panel-heading"><div><span>LIVE STATUS</span><h2>Relay status</h2></div><small>{devices.filter((device) => device.commandedState).length} of {devices.length} active</small></header>
    <div className="status-grid">{devices.map((device) => <article aria-label={`${device.name}, channel ${device.channel}, commanded ${device.commandedState ? "on" : "off"}`} className={`status-cell ${device.commandedState ? "is-on" : ""}`} key={device.id}>
      <span className="status-cell-number">{String(device.channel).padStart(2, "0")}</span>
      <strong>{device.name}</strong>
    </article>)}</div>
  </section>;
}

function PhysicalButtonEditor({ button, group, devices, pending, canEdit, onChange, onGroupControl }: { button: PhysicalButtonMapping; group: CustomRelayGroup; devices: Device[]; pending: boolean; canEdit: boolean; onChange: (button: PhysicalButtonMapping) => void; onGroupControl: (group: CustomRelayGroup, state: boolean) => void }) {
  const toggle = (channel: number) => onChange({ ...button, channels: button.channels.includes(channel) ? button.channels.filter((item) => item !== channel) : [...button.channels, channel].sort((a, b) => a - b) });
  return <article className="physical-button-editor">
    <header><span>BUTTON {button.index + 1} <i /> GPIO {button.pin}</span><small>{button.channels.length} selected</small></header>
    <label className="physical-name-label">Combination name<input value={button.name} maxLength={47} disabled={!canEdit} onChange={(event) => onChange({ ...button, name: event.target.value })} placeholder={`Button ${button.index + 1}`} /></label>
    <div className="physical-channel-grid" aria-label={`Channels assigned to ${button.name}`}>
      {devices.map((device) => <label key={device.channel} className={button.channels.includes(device.channel) ? "selected" : ""} title={device.name}><input type="checkbox" checked={button.channels.includes(device.channel)} disabled={!canEdit} onChange={() => toggle(device.channel)} /><b>{String(device.channel).padStart(2, "0")}</b><span>{device.name}</span></label>)}
    </div>
    <footer className="combo-actions"><span>Same group is assigned to this button</span><div className="group-control-buttons"><button type="button" disabled={pending || group.channels.length === 0} onClick={() => onGroupControl(group, true)}>Group on</button><button type="button" disabled={pending || group.channels.length === 0} onClick={() => onGroupControl(group, false)}>Group off</button></div></footer>
  </article>;
}

function PhysicalButtonsPanel({ buttons, groups, devices, pending, canEdit, onSave, onGroupControl }: { buttons: PhysicalButtonMapping[]; groups: CustomRelayGroup[]; devices: Device[]; pending: boolean; canEdit: boolean; onSave: (buttons: PhysicalButtonMapping[]) => void; onGroupControl: (group: CustomRelayGroup, state: boolean) => void }) {
  const [draft, setDraft] = useState(buttons);
  const changed = JSON.stringify(draft) !== JSON.stringify(buttons);
  useEffect(() => { if (!changed) setDraft(buttons); }, [buttons, changed]);
  return <section className="physical-buttons panel">
    <header className="panel-heading"><div><span>BUTTONS + GROUPS / ONE SETUP</span><h2>Relay combinations</h2></div><small>4 COMBINATIONS</small></header>
    <p className="physical-help">Each named relay group is also assigned to its physical button. Buttons work only in Manual mode; web controls work in both Manual and Remote. A button press turns its selected channels off if any are on, otherwise it turns them on. The same combination can be run here.</p>
    <div className="physical-button-grid">{draft.map((button) => <PhysicalButtonEditor key={button.index} button={button} group={groups.find((item) => item.slot === button.index) ?? { slot: button.index, name: button.name, channels: button.channels }} devices={devices} pending={Boolean(pending)} canEdit={canEdit} onGroupControl={onGroupControl} onChange={(updated) => setDraft((current) => current.map((item) => item.index === updated.index ? updated : item))} />)}</div>
    {canEdit ? <footer className="physical-actions"><span>{changed ? "Unsaved group and button setup" : "Group and button setup saved"}</span><button type="button" className="button button-primary" disabled={!changed || pending || draft.some((button) => !button.name.trim())} onClick={() => onSave(draft)}>Save combinations</button></footer> : null}
  </section>;
}

function DeviceNamesPanel({ devices, pending, canEdit, onSave }: { devices: Device[]; pending: boolean; canEdit: boolean; onSave: (names: Record<string, string>) => void }) {
  const [names, setNames] = useState<Record<string, string>>({});
  const changed = devices.some((device) => (names[device.id] ?? device.name) !== device.name);
  useEffect(() => { if (!changed) setNames(Object.fromEntries(devices.map((device) => [device.id, device.name]))); }, [devices, changed]);
  return <section className="panel config-panel">
    <header className="panel-heading"><div><span>CHANNEL LABELS</span><h2>Relay names</h2></div></header>
    <div className="relay-name-grid">{devices.map((device) => <label key={device.id}><span>CH {String(device.channel).padStart(2, "0")}</span><input maxLength={47} value={names[device.id] ?? device.name} disabled={!canEdit} onChange={(event) => setNames((current) => ({ ...current, [device.id]: event.target.value }))} /></label>)}</div>
    {canEdit ? <footer className="physical-actions"><span>Names are saved on the controller</span><button type="button" className="button button-primary" disabled={!changed || pending || devices.some((device) => !(names[device.id] ?? device.name).trim())} onClick={() => onSave(Object.fromEntries(devices.map((device) => [device.id, (names[device.id] ?? device.name).trim()] )))}>Save names</button></footer> : null}
  </section>;
}

const weekDays = ["Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"];
function SchedulePanel({ schedules, devices, pending, canEdit, timeSynced, onSave }: { schedules: ScheduleRule[]; devices: Device[]; pending: boolean; canEdit: boolean; timeSynced: boolean; onSave: (rules: ScheduleRule[]) => void }) {
  const [draft, setDraft] = useState(schedules);
  const changed = JSON.stringify(draft) !== JSON.stringify(schedules);
  useEffect(() => { if (!changed) setDraft(schedules); }, [schedules, changed]);
  function update(slot: number, patch: Partial<ScheduleRule>) { setDraft((current) => current.map((rule) => rule.slot === slot ? { ...rule, ...patch } : rule)); }
  return <section className="panel schedule-panel">
    <header className="panel-heading"><div><span>LOCAL CONTROLLER TIME</span><h2>Schedules</h2></div><small>{timeSynced ? "CLOCK SYNCED" : "WAITING FOR TIME"}</small></header>
    <p className="physical-help">Schedules run on the ESP32 at the selected local time while Remote mode is active. They require the controller clock to sync over Wi-Fi.</p>
    <div className="schedule-list">{draft.map((rule) => <article className={`schedule-rule ${rule.enabled ? "enabled" : ""}`} key={rule.slot}>
      <div className="schedule-rule-head"><b>RULE {String(rule.slot + 1).padStart(2, "0")}</b><label className="schedule-enabled"><input type="checkbox" checked={rule.enabled} onChange={(event) => update(rule.slot, { enabled: event.target.checked })} disabled={!canEdit} />Enabled</label></div>
      <div className="schedule-fields"><label className="wide-field">Schedule name<input value={rule.name} maxLength={31} placeholder="e.g. Opening hours" disabled={!canEdit} onChange={(event) => update(rule.slot, { name: event.target.value })} /></label><label>Start time<input type="time" value={rule.startTime} disabled={!canEdit} onChange={(event) => update(rule.slot, { startTime: event.target.value })} /></label><label>At start<select value={rule.startState ? "on" : "off"} disabled={!canEdit} onChange={(event) => update(rule.slot, { startState: event.target.value === "on" })}><option value="on">Turn selected relays on</option><option value="off">Turn selected relays off</option></select></label><label>End time<input type="time" value={rule.endTime} disabled={!canEdit} onChange={(event) => update(rule.slot, { endTime: event.target.value })} /></label><label>At end<select value={rule.endState ? "on" : "off"} disabled={!canEdit} onChange={(event) => update(rule.slot, { endState: event.target.value === "on" })}><option value="on">Turn selected relays on</option><option value="off">Turn selected relays off</option></select></label></div>
      <div className="schedule-days" role="group" aria-label={`Days for ${rule.name || `rule ${rule.slot + 1}`}`}>{weekDays.map((day, index) => <label key={day} className={rule.days.includes(index) ? "selected" : ""}><input type="checkbox" checked={rule.days.includes(index)} disabled={!canEdit} onChange={() => update(rule.slot, { days: rule.days.includes(index) ? rule.days.filter((item) => item !== index) : [...rule.days, index].sort() })} /><span>{day}</span></label>)}</div>
      <div className="schedule-channels" aria-label="Channels controlled by this schedule">{devices.map((device) => <label key={device.channel} className={rule.channels.includes(device.channel) ? "selected" : ""}><input type="checkbox" checked={rule.channels.includes(device.channel)} disabled={!canEdit} onChange={() => update(rule.slot, { channels: rule.channels.includes(device.channel) ? rule.channels.filter((item) => item !== device.channel) : [...rule.channels, device.channel].sort((a, b) => a - b) })} /><span>{String(device.channel).padStart(2, "0")}</span></label>)}</div>
      <small className="schedule-targets">{rule.channels.length ? rule.channels.map((channel) => devices.find((device) => device.channel === channel)?.name).join(" · ") : "Select relay channels"}</small>
    </article>)}</div>
    {canEdit ? <footer className="physical-actions"><span>{changed ? "Unsaved schedules" : "Saved on the ESP32"}</span><button className="button button-primary" type="button" disabled={!changed || pending || draft.some((rule) => rule.startTime === rule.endTime || (rule.enabled && (!rule.name.trim() || rule.channels.length === 0 || rule.days.length === 0)))} onClick={() => onSave(draft)}>Save schedules</button></footer> : null}
  </section>;
}

function TimerPanel({ devices, timers, pending, onSet }: { devices: Device[]; timers: Snapshot["timers"]; pending: string; onSet: (channel: number, seconds: number) => void }) {
  const [durations, setDurations] = useState<Record<number, number>>({});
  const [units, setUnits] = useState<Record<number, "seconds" | "minutes" | "hours">>({});
  return <section className="panel timer-panel">
    <header className="panel-heading"><div><span>ONE-SHOT AUTO-OFF</span><h2>Relay timers</h2></div><small>UP TO 24 HOURS</small></header>
    <p className="physical-help">Starting a timer turns that relay on and commands it off when the countdown ends.</p>
    <div className="timer-grid">{devices.map((device) => {
      const timer = timers.find((item) => item.channel === device.channel)?.remainingSeconds ?? 0;
      const amount = durations[device.channel] ?? 30;
      const unit = units[device.channel] ?? "minutes";
      const seconds = amount * (unit === "hours" ? 3600 : unit === "minutes" ? 60 : 1);
      return <article className="timer-row" key={device.channel}><div><b>{device.name}</b><small>{timer ? formatTimer(timer) : device.commandedState ? "On · no timer" : "Off"}</small></div>{timer ? <button type="button" className="quiet-action" disabled={Boolean(pending)} onClick={() => onSet(device.channel, 0)}>Cancel</button> : <><input aria-label={`Timer duration for ${device.name}`} type="number" min="1" max={unit === "hours" ? 24 : unit === "minutes" ? 1440 : 86400} value={amount} onChange={(event) => setDurations((current) => ({ ...current, [device.channel]: Number(event.target.value) }))} /><select aria-label={`Timer unit for ${device.name}`} value={unit} onChange={(event) => setUnits((current) => ({ ...current, [device.channel]: event.target.value as "seconds" | "minutes" | "hours" }))}><option value="seconds">seconds</option><option value="minutes">minutes</option><option value="hours">hours</option></select><button type="button" className="button button-secondary" disabled={Boolean(pending) || amount < 1 || seconds > 86400} onClick={() => onSet(device.channel, seconds)}>Start</button></>}</article>;
    })}</div>
  </section>;
}

function AboutPage() {
  return <div className="about-page">
    <section className="about-intro"><span className="eyebrow">INTELLIROOM · PROJECT</span><h1>Control and energy,<br /><em>made legible.</em></h1><p>A compact eight channel switching and monitoring prototype. The ESP32 hosts the dashboard and coordinates an Uno relay driver, physical buttons, LCD and AC meter.</p></section>
    <section className="about-section"><span className="eyebrow">01 · PROJECT OVERVIEW</span><h2>A connected load control prototype</h2><p>IntelliRoom brings relay commands, live electrical readings, schedules and event history into one local interface. It is designed to make the state and energy use of connected loads easier to inspect.</p></section>
    <section className="about-section"><span className="eyebrow">02 · SDG ALIGNMENT</span><h2>Relevant sustainability goals</h2><div className="about-columns"><p><b>SDG 7 · Affordable and Clean Energy</b><br />Energy readings can help users observe electricity use and make informed choices.</p><p><b>SDG 9 · Industry, Innovation and Infrastructure</b><br />The prototype explores accessible connected control and monitoring.</p><p><b>SDG 11 · Sustainable Cities and Communities</b><br />Responsive load control is a small building block for more aware spaces.</p><p><b>SDG 12 · Responsible Consumption</b><br />Schedules and measured usage support more deliberate operation.</p></div><small>These are project themes, not a claim of SDG certification or measured impact.</small></section>
    <section className="about-section"><span className="eyebrow">03 · THE APPLICATION</span><h2>One interface for operating and understanding</h2><p>The dashboard separates read-only system status from relay controls. Technicians can name channels, assign relay combinations to buttons, create timed schedules and timers, and review timestamped activity. Live energy readings come from the PZEM meter.</p></section>
    <section className="about-section"><span className="eyebrow">04 · APPROACH &amp; WORKFLOW</span><h2>From input to observable result</h2><ol className="workflow-list"><li>Physical buttons or the authenticated dashboard request a relay state.</li><li>The ESP32 applies mode and safety rules, then sends the relay mask to the Uno.</li><li>The Uno switches the eight relay inputs; the PZEM measures the AC load circuit.</li><li>The dashboard and LCD present command state, meter readings, network status and event history.</li></ol><p>Relay status is commanded state. The one-way ESP32-to-Uno link does not confirm the physical contact position.</p></section>
    <section className="about-section"><span className="eyebrow">05 · COMPONENTS USED</span><h2>Prototype hardware</h2><ul className="component-list"><li>ESP32 Dev Module — Wi-Fi, web API, schedules, button input and display control.</li><li>Arduino Uno — drives the single 8 channel relay bank.</li><li>8 channel 5 V relay module — switches the demo loads.</li><li>PZEM-004T v3.0 — voltage, current, power, energy, frequency and power factor.</li><li>20×4 I²C LCD, four momentary buttons and one SPDT mode switch.</li><li>External regulated 5 V supply, 8 socket junction/gang box and 16 A MCB.</li></ul><p>The demonstration uses plug-in loads on the junction box. It is not wired into a household switch box.</p></section>
    <section className="about-section"><span className="eyebrow">06 · DETAILED CONNECTIONS</span><h2>Signal and power reference</h2><div className="connection-grid"><article><h3>ESP32, display, meter and controls</h3><p>LCD SDA → GPIO21; SCL → GPIO22; VCC → external 5 V rail; GND → common ground.</p><p>PZEM TX → ESP32 GPIO16 (RX); PZEM RX ← ESP32 GPIO14 (TX); VCC → 5 V rail; GND → common ground.</p><p>ESP32 5 V and GND → 5 V and ground rails. GPIO32, GPIO33, GPIO25 and GPIO26 each connect to a momentary switch that returns to GND (internal pull-ups enabled).</p><p>GPIO27 → center leg of SPDT; outer legs → 3.3 V and GND. GPIO17 TX → Uno D0/RX for the relay command link.</p></article><article><h3>Uno, relay bank and demo loads</h3><p>Uno D2–D9 → relay IN1–IN8. Relay VCC → external 5 V rail; relay GND and Uno GND → common ground.</p><p>Use the external regulated supply for the 5 V rail. The ESP32, Uno, LCD, PZEM and relay logic require a shared ground.</p><p>Relay outputs feed the 8 socket demo junction/gang box. The demo supply includes a 16 A MCB. This is a prototype arrangement, not a household installation.</p></article></div></section>
  </div>;
}

function CustomGroupEditor({ group, devices, pending, onSave }: { group: CustomRelayGroup; devices: Device[]; pending: boolean; onSave: (slot: number, name: string, channels: number[]) => void }) {
  const [name, setName] = useState(group.name);
  const [channels, setChannels] = useState<number[]>(group.channels);
  useEffect(() => { setName(group.name); setChannels(group.channels); }, [group.name, group.channels.join(",")]);

  function toggleChannel(channel: number) {
    setChannels((current) => current.includes(channel) ? current.filter((value) => value !== channel) : [...current, channel].sort((a, b) => a - b));
  }

  return <form className="group-editor" onSubmit={(event) => { event.preventDefault(); onSave(group.slot, name.trim(), channels); }}>
    <label>Group {group.slot + 1}<input value={name} maxLength={47} placeholder="Name this group" onChange={(event) => setName(event.target.value)} /></label>
    <fieldset><legend>Include channels</legend><div className="channel-choices">{devices.map((device) => <label key={device.channel} className={channels.includes(device.channel) ? "checked" : ""}><input type="checkbox" checked={channels.includes(device.channel)} onChange={() => toggleChannel(device.channel)} /><span>CH {device.channel}</span><small>{device.name}</small></label>)}</div></fieldset>
    <div className="group-editor-actions"><button className="button button-primary" type="submit" disabled={pending || !name.trim() || channels.length === 0}>Save group</button>{group.name ? <button className="quiet-action" type="button" disabled={pending} onClick={() => onSave(group.slot, "", [])}>Clear group</button> : null}</div>
  </form>;
}

function CustomGroupsPanel({ groups, devices, canEdit, pending, onSave, onControl }: { groups: CustomRelayGroup[]; devices: Device[]; canEdit: boolean; pending: string; onSave: (slot: number, name: string, channels: number[]) => void; onControl: (group: CustomRelayGroup, state: boolean) => void }) {
  const savedGroups = groups.filter((group) => group.name && group.channels.length > 0);
  return <section className="custom-groups panel" id="custom-groups">
    <header className="panel-heading"><div><span>RELAY SETUP</span><h2>Custom groups</h2></div><small>{savedGroups.length} saved</small></header>
    {savedGroups.length ? <div className="saved-groups">{savedGroups.map((group) => <article className="saved-group" key={group.slot}>
      <div><strong>{group.name}</strong><small>{group.channels.map((channel) => devices.find((device) => device.channel === channel)?.name ?? `Channel ${channel}`).join(" · ")}</small></div>
      <div className="group-control-buttons"><button type="button" disabled={Boolean(pending)} onClick={() => onControl(group, true)}>On</button><button type="button" disabled={Boolean(pending)} onClick={() => onControl(group, false)}>Off</button></div>
    </article>)}</div> : <p className="group-empty">No custom groups saved yet.</p>}
    {canEdit ? <details className="group-management"><summary>{savedGroups.length ? "Edit groups" : "Create a group"}</summary><div className="group-editor-grid">{groups.map((group) => <CustomGroupEditor key={group.slot} group={group} devices={devices} pending={pending === `save-group:${group.slot}`} onSave={onSave} />)}</div></details> : null}
  </section>;
}

function PowerChart({ samples, valid }: { samples: PowerSample[]; valid: boolean }) {
  const geometry = useMemo(() => {
    const values = samples.length > 1 ? samples : [{ at: Date.now() - 60_000, watts: samples[0]?.watts ?? 0 }, ...samples];
    const max = Math.max(1200, ...values.map((sample) => sample.watts));
    const min = Math.min(0, ...values.map((sample) => sample.watts));
    const spread = Math.max(1, max - min);
    const points = values.map((sample, index) => {
      const x = (index / Math.max(1, values.length - 1)) * 600;
      const y = 180 - ((sample.watts - min) / spread) * 145;
      return `${x.toFixed(1)},${y.toFixed(1)}`;
    }).join(" ");
    return { points, max };
  }, [samples]);

  return <section className="power-panel panel" id="energy">
    <header className="panel-heading"><div><span>LIVE METER / 02</span><h2>Power draw</h2></div><small>{valid ? "2 SEC SENSOR POLL" : "NO VALID READING"}</small></header>
    <div className="chart-wrap">
      <div className="chart-axis"><span>{Math.round(geometry.max)} W</span><span>{Math.round(geometry.max / 2)} W</span><span>0 W</span></div>
      <svg viewBox="0 0 600 190" preserveAspectRatio="none" role="img" aria-label="Recent live power readings">
        <g className="grid-lines"><line x1="0" y1="35" x2="600" y2="35" /><line x1="0" y1="108" x2="600" y2="108" /><line x1="0" y1="180" x2="600" y2="180" /></g>
        <polyline className="power-line-shadow" points={geometry.points} />
        <polyline className="power-line" points={geometry.points} />
      </svg>
      <div className="chart-times"><span>EARLIER</span><span>LIVE</span></div>
    </div>
  </section>;
}

function PriorityPanel() {
  return <section className="priority-panel panel" id="priority">
    <header className="panel-heading"><div><span>CONTROL LOGIC / 03</span><h2>Operational priority</h2></div></header>
    <ol className="priority-list">
      <li><b>03</b><div><strong>Physical controls</strong><span>HIGHEST · ALWAYS AUTHORITATIVE</span></div></li>
      <li><b>02</b><div><strong>Teacher commands</strong><span>CLASSROOM OPERATIONS</span></div></li>
      <li><b>01</b><div><strong>Automation</strong><span>WHEN REMOTE MODE IS ENABLED</span></div></li>
    </ol>
    <p className="truth-note"><Icon name="shield" size={16} /> Interface shows commanded state. The relay link is one-way; physical feedback is not available.</p>
  </section>;
}

function ActivityPanel({ logs, devices }: { logs: ActivityEntry[]; devices: Device[] }) {
  const orderedLogs = [...logs].reverse();
  return <section className="activity-panel panel" id="activity">
    <header className="panel-heading"><div><span>EVENT STREAM / 04</span><h2>Recent activity</h2></div><small>{logs.length} EVENTS BUFFERED</small></header>
    <div className="activity-table">
      {orderedLogs.length === 0 ? <p className="empty-state">No activity recorded since boot.</p> : orderedLogs.map((entry, index) => <div className="activity-row" key={`${entry.timestampMs}-${entry.target}-${index}`}>
        <time dateTime={entry.timestampMs >= 1_000_000_000_000 ? new Date(entry.timestampMs).toISOString() : undefined}>{formatLogTime(entry.timestampMs)}</time>
        <span className={`activity-source source-${entry.source}`}>{sourceLabels[entry.source] ?? entry.source}</span>
        <strong>{targetLabel(entry.target, devices)}</strong>
        <b>{entry.action.replaceAll("_", " ")}</b>
      </div>)}
    </div>
  </section>;
}

function Alerts({ alerts }: { alerts: AlertEntry[] }) {
  if (alerts.length === 0) return null;
  return <section className="alert-stack" id="alerts" aria-label="System alerts">
    {alerts.map((alert, index) => <div className={`alert alert-${alert.severity.toLowerCase()}`} key={`${alert.message}-${index}`}><Icon name="alerts" /><strong>{alert.severity}</strong><span>{alert.message}</span></div>)}
  </section>;
}

function Dashboard({ initial, onUnauthenticated, theme, accent, onThemeChange, onAccentChange }: { initial: Snapshot; onUnauthenticated: () => void; theme: ThemeMode; accent: AccentTheme; onThemeChange: (value: ThemeMode) => void; onAccentChange: (value: AccentTheme) => void }) {
  const [snapshot, setSnapshot] = useState(initial);
  const [page, setPage] = useState<PageKey>(() => {
    const value = window.location.hash.slice(1) as PageKey;
    return pageItems.some((item) => item.key === value) ? value : "home";
  });
  const [refreshing, setRefreshing] = useState(false);
  const [pending, setPending] = useState("");
  const [toast, setToast] = useState("");
  const [powerSamples, setPowerSamples] = useState<PowerSample[]>(() => {
    if (!isDemoMode || initial.status.energy.power == null) return [];
    const now = Date.now();
    return Array.from({ length: 18 }, (_, index) => ({ at: now - (17 - index) * 8_000, watts: 870 + Math.sin(index * 0.72) * 120 + index * 5 }));
  });

  const refresh = useCallback(async (quiet = false) => {
    if (!quiet) setRefreshing(true);
    try {
      const next = await api.snapshot();
      setSnapshot(next);
      if (next.status.energy.valid && next.status.energy.power != null) {
        setPowerSamples((current) => [...current, { at: Date.now(), watts: next.status.energy.power! }].slice(-24));
      }
    } catch (error) {
      if (error instanceof ApiError && error.status === 401) onUnauthenticated();
      else setToast(error instanceof Error ? error.message : "Refresh failed");
    } finally {
      if (!quiet) setRefreshing(false);
    }
  }, [onUnauthenticated]);

  useEffect(() => {
    const timer = window.setInterval(() => void refresh(true), 5_000);
    return () => window.clearInterval(timer);
  }, [refresh]);

  const navigate = useCallback((next: PageKey) => {
    setPage(next);
    window.history.replaceState(null, "", `#${next}`);
  }, []);
  useEffect(() => {
    const onHashChange = () => {
      const value = window.location.hash.slice(1) as PageKey;
      if (pageItems.some((item) => item.key === value)) setPage(value);
    };
    window.addEventListener("hashchange", onHashChange);
    return () => window.removeEventListener("hashchange", onHashChange);
  }, []);

  useEffect(() => {
    if (!toast) return;
    const timer = window.setTimeout(() => setToast(""), 3_200);
    return () => window.clearTimeout(timer);
  }, [toast]);

  const execute = useCallback(async (key: string, operation: () => Promise<void>, success: string) => {
    setPending(key);
    setToast("");
    try {
      await operation();
      await refresh(true);
      setToast(success);
    } catch (error) {
      setToast(error instanceof Error ? error.message : "Command failed");
    } finally {
      setPending("");
    }
  }, [refresh]);

  async function logout() {
    try { await api.logout(); } finally { onUnauthenticated(); }
  }

  const toggleDevice = (device: Device) => void execute(device.id, () => api.setDevice(device, !device.commandedState), `${device.name}: commanded ${device.commandedState ? "OFF" : "ON"}`);
  const saveDeviceNames = (names: Record<string, string>) => void execute("device-names", async () => {
    for (const device of snapshot.devices) if (names[device.id] !== device.name) await api.renameDevice(device.id, names[device.id]);
  }, "Relay names saved");
  const controlCustomGroup = (group: CustomRelayGroup, state: boolean) => void execute(`custom-group:${group.slot}`, () => api.setCustomGroup(group.slot, state), `${group.name} commanded ${state ? "ON" : "OFF"}`);
  const savePhysicalButtons = (buttons: PhysicalButtonMapping[]) => void execute("physical-buttons", async () => {
    for (const button of buttons) await api.saveCustomGroup(button.index, button.channels.length ? button.name : "", button.channels);
    await api.savePhysicalButtons(buttons);
  }, "Relay groups and button combinations saved");
  const saveSchedules = (schedules: ScheduleRule[]) => void execute("schedules", () => api.saveSchedules(schedules), "Schedules saved on the ESP32");
  const setTimer = (channel: number, seconds: number) => void execute(`timer:${channel}`, () => api.setTimer(channel, seconds), seconds ? `Timer started for channel ${channel}` : `Timer cancelled for channel ${channel}`);

  const pageHeading: Record<PageKey, [string, string]> = {
    home: ["System overview", "Live controller status, relay commands and energy readings."],
    controls: ["Relay controls", "Switch an individual channel and watch the live power reading."],
    setup: ["Controller setup", "Name channels, define button groups, and set schedules or auto-off timers."],
    activity: ["Activity log", "Recent commands and controller events since the last restart."],
    about: ["About IntelliRoom", "Project hardware, demo arrangement and connection reference."],
  };

  return <div className="app-shell" data-theme={theme} data-accent={accent}>
    <div className="workspace">
      <Header snapshot={snapshot} refreshing={refreshing} onRefresh={() => void refresh()} onLogout={() => void logout()} theme={theme} accent={accent} page={page} onNavigate={navigate} onThemeChange={onThemeChange} onAccentChange={onAccentChange} />
      <main className={`main-content page-${page}`}>
        {page !== "about" ? <section className="page-intro"><div><span className="eyebrow">INTELLIROOM / {pageItems.findIndex((item) => item.key === page) + 1}</span><h1>{pageHeading[page][0]}</h1><p>{pageHeading[page][1]}</p></div>{page === "home" ? <span className="last-updated"><i /> Updating live</span> : null}</section> : null}
        {page === "home" ? <>
          <Alerts alerts={snapshot.alerts} />
          <MetricStrip snapshot={snapshot} />
          <div className="home-status-layout"><RelayStatusGrid devices={snapshot.devices} /><PowerChart samples={powerSamples} valid={snapshot.status.energy.valid} /><section className="system-panel panel"><header className="panel-heading"><div><span>CONTROLLER</span><h2>System link</h2></div><span className={`status-dot ${snapshot.status.wifiConnected ? "online" : "offline"}`} /></header><dl><div><dt>Wi-Fi signal</dt><dd>{snapshot.status.rssi} dBm</dd></div><div><dt>Controller address</dt><dd>{snapshot.status.ip}</dd></div><div><dt>Operating mode</dt><dd>{snapshot.status.mode.toUpperCase()}</dd></div><div><dt>Interlock</dt><dd>{snapshot.status.safetyInterlock ? "ACTIVE" : "CLEAR"}</dd></div></dl></section></div>
        </> : null}
        {page === "controls" ? <div className="controls-page-content"><PowerChart samples={powerSamples} valid={snapshot.status.energy.valid} /><DeviceGroup title="Individual relays" devices={snapshot.devices} pending={pending} onToggle={toggleDevice} /></div> : null}
        {page === "setup" ? <div className="setup-page-content">
          <DeviceNamesPanel devices={snapshot.devices} pending={pending === "device-names"} canEdit={snapshot.status.role === "technician"} onSave={saveDeviceNames} />
          <PhysicalButtonsPanel buttons={snapshot.physicalButtons} groups={snapshot.customGroups} devices={snapshot.devices} pending={pending === "physical-buttons" || pending.startsWith("custom-group:")} canEdit={snapshot.status.role === "technician"} onSave={savePhysicalButtons} onGroupControl={controlCustomGroup} />
          <SchedulePanel schedules={snapshot.schedules} devices={snapshot.devices} pending={pending === "schedules"} canEdit={snapshot.status.role === "technician"} timeSynced={snapshot.status.timeSynced} onSave={saveSchedules} />
          <TimerPanel devices={snapshot.devices} timers={snapshot.timers} pending={pending} onSet={setTimer} />
        </div> : null}
        {page === "activity" ? <ActivityPanel logs={snapshot.logs} devices={snapshot.devices} /> : null}
        {page === "about" ? <AboutPage /> : null}
      </main>
    </div>
    {toast ? <div className="toast" role="status" aria-live="polite">{toast}</div> : null}
  </div>;
}

export default function App() {
  const [state, setState] = useState<"checking" | "authenticated" | "unauthenticated">("checking");
  const [initial, setInitial] = useState<Snapshot | null>(null);
  const [theme, setTheme] = useState<ThemeMode>(() => localStorage.getItem("intelliroom-theme") === "dark" ? "dark" : "light");
  const [accent, setAccent] = useState<AccentTheme>(() => {
    const value = localStorage.getItem("intelliroom-accent");
    return value === "blue" || value === "violet" ? value : "ember";
  });

  const updateTheme = (value: ThemeMode) => { localStorage.setItem("intelliroom-theme", value); setTheme(value); };
  const updateAccent = (value: AccentTheme) => { localStorage.setItem("intelliroom-accent", value); setAccent(value); };

  useEffect(() => {
    document.documentElement.dataset.theme = theme;
    document.documentElement.dataset.accent = accent;
    document.documentElement.style.colorScheme = theme;
  }, [theme, accent]);

  const authenticate = useCallback(async () => {
    try {
      const snapshot = await api.snapshot();
      setInitial(snapshot);
      setState("authenticated");
    } catch (error) {
      if (error instanceof ApiError && error.status === 401) setState("unauthenticated");
      else setState("unauthenticated");
    }
  }, []);

  useEffect(() => { void authenticate(); }, [authenticate]);

  if (state === "checking") return <div className="boot-screen"><span className="brand-mark" aria-hidden="true"><i /><i /><i /></span><strong>INTELLIROOM</strong><small>OPENING LOCAL CONSOLE</small></div>;
  if (state === "unauthenticated" || !initial) return <LoginScreen onAuthenticated={authenticate} />;
  return <Dashboard initial={initial} onUnauthenticated={() => { setInitial(null); setState("unauthenticated"); }} theme={theme} accent={accent} onThemeChange={updateTheme} onAccentChange={updateAccent} />;
}
