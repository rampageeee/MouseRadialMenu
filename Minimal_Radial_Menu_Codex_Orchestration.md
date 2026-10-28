# Minimal Context-Aware Radial Menu — Codex Orchestration Brief

**Document status:** implementation brief / proposed design, not an existing implementation  
**Target platform:** Windows 11 x64 first  
**Working title:** *TinyRadial* (placeholder; do not spend implementation time on branding)  
**Intended license:** MIT (or another similarly permissive open-source license, with third-party notices preserved)  
**Audience:** Codex acting as implementation coordinator and engineering team

---

## 1. Mission

Build a genuinely small, portable, open-source Windows utility that displays **context-aware radial menus** for executing **named keyboard shortcuts**. Menus must support **nested submenus** and automatically adapt to the foreground application.

The intended user has a Logitech G502 HERO mouse and G213 keyboard, but the utility must **not depend on Logitech hardware, Logitech Gaming Software, G HUB, Electron, an embedded browser, cloud services, or a resident scripting runtime**. Any mouse button that can be mapped to the activation shortcut should work.

**Core proposition:** a tiny, responsive, locally configurable alternative to heavyweight radial-menu software. Compete on scope discipline, small footprint, reliability and usability—not on number of features.

The user will rely on Codex to implement and configure this tool and is not a programmer. Prioritize finished, verifiable deliverables and plain-English setup instructions over theoretical extensibility.

### Non-negotiable priorities

1. Correct and reliable input handling; the pointer and keyboard must never remain intentionally captured after the menu is dismissed.
2. Easy-to-understand configuration (JSON, documented with working examples), with no requirement to write code.
3. Application-aware root menu selection with a sensible global fallback.
4. Fast radial navigation and arbitrary **acyclic** submenu nesting.
5. Named **simultaneous keyboard chords** (e.g. `Ctrl+Shift+S`, `Alt+Enter`, `F13`), not a macro engine.
6. Native Windows executable, portable deployment, minimal idle resource use.
7. No dependence on persistent Logitech software after the activation key has been mapped in mouse onboard memory.

---

## 2. Scope boundaries

### MVP: must implement

- One resident Windows tray application, running as a single process.
- A configurable global activation shortcut (default proposal: `Ctrl+Alt+Space`; allow F13–F24 if available through the input device).
- At least **toggle** activation; prioritize **hold-to-open / release-to-execute** if the input prototype demonstrates reliable, non-leaking key-up detection.
- Circular menu with readable labels, selection highlight, center cancel zone, 4–8 commonly used items per menu, and sensible behavior for fewer items.
- Named actions that send a single keyboard combination; no arbitrarily sequenced actions.
- Submenus, including navigation back to a parent.
- Selection of a profile by foreground executable name, captured **before** the overlay appears.
- Global/default profile when no match exists; deterministic handling of duplicate/conflicting matches.
- Human-editable JSON configuration with validation, actionable error messages, and reload without restarting.
- Configurable cursor mode: **restore** original pointer position on dismissal. Investigate **virtual cursor** in a separate feasibility spike before committing it to MVP.
- Tray commands: enabled/disabled, reload, open config folder, exit.
- High-DPI and multi-monitor positioning (including negative virtual-screen coordinates).
- Release of any held modifiers, pointer restrictions, and hooks on cancellation and normal shutdown.
- Portable README, sample profiles for desktop and REAPER, and reproducible release build.

### Explicitly out of scope for MVP

- Macro recording; timed key sequences; branching automation; scripts or arbitrary executable launches.
- Plugins, web interfaces, Electron/Tauri, telemetry, sign-in, update daemon, cloud synchronization.
- A full graphical profile editor or theme editor.
- Process injection, kernel drivers, global Raw Input interception, anti-cheat workarounds.
- Guaranteed capture of mouse movement inside all games, especially those using Raw Input or exclusive fullscreen.
- Logitech-specific configuration utilities or firmware writes.
- Special integrations with REAPER's scripting API (can be an optional later enhancement).

**Do not silently expand scope.** Log tempting ideas under `docs/POST_MVP.md` instead of implementing them.

---

## 3. Size and performance budgets

These are **proposed engineering targets**, not established benchmark results:

| Metric | Preferred target | Measurement |
|---|---:|---|
| Release `.exe` | ≤ 3 MB | Actual signed/unsigned x64 release file on disk |
| Absolute size review threshold | > 5 MB | Stop and explain what caused it before release |
| Idle private memory | ≤ 20 MiB | Windows Process Private Bytes after warm-up |
| Idle CPU | Near zero | CPU time over a controlled idle interval, not one Task Manager screenshot |
| Resident process count | 1 | Process enumeration |
| Menu opening | Feels immediate; aspirational < 50 ms | Instrument hotkey-to-first-visible-frame where practical |
| Startup | Aspirational < 200 ms | Measured, not assumed |
| Network activity | None | No networking code or update/background services |

Do not sacrifice input safety, maintainability, text rendering quality or correctness merely to shave off a few kilobytes. **Report measurements with environment, methodology, and results.** Do not claim compliance without measurements.

---

## 4. Implementation architecture

**Suggested stack:** C++20 + Win32 + Direct2D/DirectWrite + CMake + MSVC. This is a recommendation, not an excuse to use large third-party libraries. Prefer system-provided rendering and OS APIs over bundled engines. A small, license-compatible JSON parser is acceptable; prefer a single auditable dependency to implementing a fragile custom JSON parser.

Keep four components conceptually separate:

1. **Platform/input:** activation, cursor coordinates, relative movement feasibility, lifecycle, Windows hooks/Hotkey API, mouse buttons, cleanup.
2. **Context/config:** foreground-window snapshot; executable matching; JSON parsing, validation, cached profiles, atomic hot reload.
3. **Menu state machine:** selected item, submenu transitions, cancellation, dead zone, gesture thresholds, activation semantics.
4. **Renderer/action dispatch:** transparent topmost non-activating overlay; text and wedge rendering; synthesized key chords after menu closure.

One executable is preferred. Logical modules/classes may be separate, but avoid microservices, plugin interfaces and unnecessary abstraction layers.

### Windows specifics to validate in code

- `GetForegroundWindow` and `GetWindowThreadProcessId` for context. Capture a stable target window/process **before showing** the menu.
- A non-activating overlay if practical; ensure it does not steal application focus or become the target for its own injected keystrokes.
- `SendInput` for keyboard chords, with correct key-down order and reversed key-up order; release modifiers on all failure paths. `SendInput` is limited by Windows privilege/integrity isolation—do not claim elevated-window support from a normal process.
- Assess `RegisterHotKey` versus a narrowly scoped `WH_KEYBOARD_LL` hook for activation. `RegisterHotKey` alone is not necessarily adequate for detecting a held trigger's release; document the actual design trade-off.
- For desktop pointer handling, evaluate `ClipCursor`, low-level mouse hooks, and Raw Input (`WM_INPUT`). **Capturing relative motion is not the same as preventing other programs from receiving it.**
- Handle `Esc`, focus loss, display change, app exit, and overlay-creation failure as cancellation/cleanup paths.
- Make DPI awareness explicit (prefer per-monitor DPI awareness v2), and place menus within the usable bounds of the correct monitor.

---

## 5. Input and menu interaction contract

The UX is important enough to specify independently of implementation.

### Invocation

- Capture the foreground target app and pointer position.
- Show the correct radial menu centered near the pointer (adjust if close to screen edges).
- The cursor should not be *permanently* displaced by opening/using the menu.
- **Default MVP mode:** restore cursor to original position after selection/cancel.
- **Research mode:** virtual pointer selection driven by relative mouse deltas while desktop pointer stays still. This is desirable, but must not delay a dependable desktop MVP.

### Navigation

- A configurable central dead zone cancels selection.
- Menu segments show **names**, not cryptic shortcut strings; optionally show the shortcut as small supporting text.
- Highlight responds continuously and with minimal latency.
- Submenus open by a reliable gesture or short dwell (with hysteresis to avoid accidental transitions); also provide an explicit path back.
- Releasing an activation key over a **leaf action** runs exactly one shortcut. Releasing over the center cancels. Releasing over a submenu entry should not accidentally execute a leaf command.
- `Esc` always cancels and closes. Focus loss cancels by default rather than injecting shortcuts into an unrelated window.
- Trigger presses/releases should not leak unexpectedly into the underlying app; test this rather than assuming it.

### Action dispatch

- Close/hide overlay and release any mouse/cursor capture **before** injecting keys.
- Validate that the original target window still exists and is foreground/eligible. If context changed, cancel rather than dispatch into the wrong application.
- Emit modifiers and key-down/key-up events deterministically. Do not leave `Ctrl`, `Alt`, `Shift`, or `Win` logically pressed after interruption.
- Restrict action grammar to a **single chord** in MVP. `Ctrl+K, Ctrl+C` is a sequence, not one chord, and belongs in a future macro phase.
- Document limitations for elevated targets, anti-cheat-protected apps, layouts, and fullscreen games.

---

## 6. JSON configuration contract

Prefer simple, explicit JSON, with inline nested menus. Proposed format (Codex may adjust details **before** implementation, but must document and test the final schema):

`config.json`:

```json
{
  "schemaVersion": 1,
  "activation": {
    "hotkey": "Ctrl+Alt+Space",
    "behavior": "hold",
    "cursorMode": "restore"
  },
  "ui": {
    "deadZonePx": 28,
    "submenuDwellMs": 220,
    "showShortcuts": true
  },
  "profiles": [
    "profiles/default.json",
    "profiles/reaper.json",
    "profiles/blender.json"
  ]
}
```

`profiles/reaper.json`:

```json
{
  "schemaVersion": 1,
  "name": "REAPER",
  "match": { "executables": ["reaper.exe"] },
  "menu": {
    "label": "REAPER",
    "items": [
      { "label": "Undo", "keys": "Ctrl+Z" },
      { "label": "Save", "keys": "Ctrl+S" },
      {
        "label": "Editing",
        "submenu": {
          "label": "Editing",
          "items": [
            { "label": "Split", "keys": "S" },
            { "label": "Toggle snap", "keys": "Alt+S" }
          ]
        }
      }
    ]
  }
}
```

**Important:** the REAPER assignments above are **illustrative only**, not a claim that every shortcut matches the user's customized REAPER Action List. The built-in samples must identify and verify their shortcut assumptions.

### Schema rules

- Exactly one of `keys` or `submenu` for each item; every visible item has a nonempty `label`.
- Global profile matches when no app-specific profile does; matching is case-insensitive for executable filenames.
- Deterministic selection priority, no silent ambiguity; document duplicate-profile behavior.
- Support `Ctrl`, `Shift`, `Alt`, `Win`, common navigation keys, and `F1`–`F24`; reject unrecognized or ambiguous tokens.
- Reject malformed shortcuts before runtime dispatch.
- Validate nested menus, item count, missing files and unreasonable recursion depth. Inline nesting prevents references to cyclic submenu graphs, but cap depth defensively.
- `schemaVersion` enables future migration.
- Reload is **atomic**: parse and validate new config, then swap only after success; preserve last-known-good config if reload fails.
- Present parse errors with filenames and useful diagnostics. Never crash or silently load a broken partial profile.
- Store user-editable files next to the portable executable, unless the user deliberately selects a separate config directory.
- Save no telemetry or per-app browsing/activity history.

---

## 7. Codex orchestration: execution order and gates

**Coordinator:** Codex owns the integration plan, architecture decisions, build reproducibility, quality bar and final reporting. If multiple agents are available, split bounded workstreams below; otherwise execute them sequentially. Avoid agents editing overlapping files simultaneously. Each change should include tests or a documented manual test.

### Phase 0 — Repository and feasibility reconnaissance

**Before implementing features:**

- Check the existing repository, branch, README, license and build environment; never discard preexisting changes.
- If no repository exists, initialize a minimal CMake-based project with `/src`, `/tests`, `/profiles`, `/docs`, `/tools` as appropriate.
- Document dependency choices and their licenses.
- Spike one tiny Win32 prototype to answer:
  1. Can a global configurable hotkey reliably open/close the menu without leaking unwanted keypresses?
  2. Can the foreground target and its window be captured reliably before overlay display?
  3. Can a transparent, topmost, non-activating overlay render and dismiss cleanly?
  4. Can we restore pointer position and safely inject one key combination after closing?
  5. What happens to relative mouse movement when a game uses Raw Input?
- Use a simple test app for input trials and an actual game only as an optional compatibility check. Do not infer game isolation from pointer confinement.

**Gate 0:** written feasibility notes (`docs/INPUT_FEASIBILITY.md`) and a minimal reproducible test. If universal raw-input isolation is not practical in user mode, mark it unsupported and proceed with desktop compatibility.

### Phase 1 — Core engine and configuration

**Workstream A: JSON/config**
- Implement parser, schema validation, profile matching, atomic reload, samples and tests.

**Workstream B: input/context**
- Implement trigger state, foreground snapshot, cursor restoration, cleanup and dispatch conditions.

**Workstream C: menu model**
- Implement deterministic state machine: open, hover, enter submenu, back, select, cancel, close.

Parallelism is appropriate only after an agreed interface is committed to the main branch.

**Gate 1:** headless tests for shortcut parser, profile selection, nested menu traversal, invalid config and cancellation pass. No GUI smoke test should be needed to validate the menu state machine.

### Phase 2 — Native overlay and end-to-end desktop behavior

- Integrate Direct2D/DirectWrite overlay.
- Implement pointer-based selection, dead zone, readable labels, submenu transition, ESC cancellation and monitor-edge repositioning.
- Wire action dispatch and verify key-down/up balance under repeated use.
- Add tray controls, hot reload and a clear minimal error-reporting mechanism (e.g. tray notification and log file only for errors).
- Test with Notepad, Explorer, browser, and REAPER; do not send synthetic shortcuts to the wrong window.

**Gate 2:** an actual portable `.exe` successfully performs a context-specific nested menu action, closes cleanly, and restores normal mouse/keyboard behavior.

### Phase 3 — Hardening and footprint optimization

- Test stress cases: 100+ rapid opens/closes, ESC spam, config edits while open, deleted/invalid config, app focus changes, window destruction, minimize, alt-tab, monitor unplug/reconfigure, mixed DPI, negative monitor coordinates.
- Verify that all modifier keys and pointer restrictions are released on every error path and normal shutdown. Provide an emergency-cancel strategy; do not promise recovery from every possible OS or process crash.
- Measure size, private memory, idle CPU, startup and open latency using a recorded method.
- Reduce dependencies, allocations and unnecessary polling. Rendering should be event-driven or active only while visible.
- Produce Windows Release builds through documented commands and automated CI where practical.

**Gate 3:** pass release checklist, publish actual measurements, and identify any missed targets explicitly.

### Phase 4 — Optional virtual-cursor experiment (isolated scope)

Only after the desktop MVP is stable:

- Add an opt-in experimental `cursorMode: "virtual"`.
- Use relative movement for menu selection and investigate freezing the ordinary desktop pointer.
- Test whether standard apps still receive inappropriate mouse movement; test at least one Raw Input game as a non-guaranteed compatibility case.
- Make the mode safely fall back to `restore` on unsupported conditions, and document limitations.
- Do **not** introduce kernel drivers, invasive hooks, process injection or anti-cheat bypass techniques.

**Gate 4:** feature only ships if it demonstrably behaves correctly in ordinary apps and exits cleanly. Game-camera isolation must be documented as unsupported or compatibility-dependent unless separately verified for a specific game.

---

## 8. Workstream ownership and integration rules

| Workstream | Owns | Must not modify without coordination | Deliverable |
|---|---|---|---|
| Coordinator/build | CMake, architecture contracts, CI, release notes | Module internals | Reproducible build + integration report |
| Config/context | JSON schema, loader, matching | Renderer or hooks | Validated config API + tests |
| Input/actions | Hotkeys, focus snapshot, cursor lifecycle, key injection | Config schema | Input abstraction + lifecycle tests |
| Menu/UI | State machine, wedges, label layout, DPI | Low-level input hooks | Smooth overlay + testable transitions |
| QA/docs | Test harnesses, measurement method, manual tests, README | Production logic except targeted fixes | Test evidence + setup guide |

Integration rules:

- Land a minimal shared interface before parallel implementation.
- Keep changes small and reviewable; build and test after each integration.
- Tests must exercise actual behavior, not only mocked success paths.
- Record unresolved Windows API uncertainty in `docs/KNOWN_LIMITATIONS.md` rather than masking it with a workaround.
- Do not add whole subsystems to address rare edge cases until a reproducible test demonstrates the need.

---

## 9. Acceptance test matrix

| Test | Expected result |
|---|---|
| Launch with valid config | Tray process starts and remains idle; no networking |
| Open in REAPER | REAPER menu chosen based on captured `reaper.exe` foreground process |
| Open elsewhere | Default menu chosen |
| Named chord | Correct keys down/up; a single chord per leaf action |
| Nested menu | Submenu opens, back navigates correctly, leaf runs once |
| Center cancel / Esc | No shortcut emitted; overlay closes |
| Focus changes while open | Cancel safely; do not inject into newly foreground app |
| Invalid JSON on reload | Detailed error and old valid config still active |
| Held modifiers | No stuck Shift/Ctrl/Alt/Win after run or cancellation |
| Config changed while menu open | Defined behavior (prefer apply after dismissal), no half-updated menu |
| Mixed DPI / multiple screens | Fully visible overlay near activation position; correct pointer restore |
| Rapid repetition | No crashes, state leaks or duplicate actions |
| Target running elevated | Documented refusal/limitation without unsafe escalation |
| Tray exit | Hooks, input state and confinement released; process terminates |
| Size/memory/CPU | Measurements reported, including any missed budget |
| Game with Raw Input | Behavior documented accurately; never claim universal isolation |

### Manual validation notes

- Test with a conventional keyboard hotkey before requiring any G502 reconfiguration.
- Test a physical mouse button mapped to a keyboard chord **after** the keyboard path works.
- Include one scenario where a game uses Raw Input; note both visibility of overlay and camera behavior.
- Do not perform automated key injection into unsaved user documents during testing.

---

## 10. User setup and documentation deliverables

The release package should include:

```text
TinyRadial/
  TinyRadial.exe
  config.json
  profiles/
    default.json
    reaper.json
    blender.json
  README.md
  LICENSE
  THIRD_PARTY_NOTICES.md  (if required)
```

`README.md` must cover:

1. Unzip and run (no installer, account or admin privilege required for ordinary use).
2. How to select an activation chord and optionally map it to the G502 HERO's onboard button using compatible Logitech tooling once.
3. How to rename a menu segment and change a shortcut in JSON.
4. How to create an app-specific profile from an example.
5. How to enter and exit submenus, cancel, reload, disable and quit.
6. How to recover from invalid config or failed input capture.
7. Known limitations: elevated apps, exclusive fullscreen, Raw Input games and app-specific shortcuts.
8. Size, startup, CPU and memory measurements using a repeatable method.

Use screenshots/GIF demonstrations only after the functional software is implemented; they are not prerequisites for the MVP.

---

## 11. Definition of done

The MVP is complete only when:

- A fresh checkout builds with the documented Windows toolchain.
- The executable runs independently of Electron, Node.js, Python, Logitech Gaming Software, G HUB and network access.
- A single activation key opens a radial menu, context matching selects a profile, a submenu can be navigated, and a named chord is executed into the intended foreground application.
- ESC and central cancellation work; pointer position is restored according to config; modifiers do not remain held.
- Invalid configuration is handled without losing the last-known-good settings.
- The executable and measured idle overhead are reported, not guessed.
- The code is published with an open-source license and clear user instructions.
- Unverified features and platform restrictions are identified explicitly.

**Stop condition:** Do not progress into macros, a GUI editor, themes, plugins, auto-updates, or universal game-input isolation as part of the MVP.

---

## 12. Instruction to Codex — first response and ongoing reporting

On receiving this brief:

1. Summarize the minimal architecture and identify assumptions requiring verification; do **not** request the user to make routine implementation decisions.
2. Inspect existing files/repository status before creating or modifying code.
3. Write a short implementation plan with phases and acceptance gates, then start with the Phase 0 input feasibility prototype.
4. Implement incrementally; measure actual behavior rather than claiming theoretical support.
5. At each gate, report **implemented / tested / unverified / known limitations / next phase**.
6. Keep the MVP lean and reject unnecessary dependencies, including Electron and embedded browser frameworks.
7. If some requirement is not reliably achievable using ordinary Windows user-mode APIs, explain precisely what failed and offer the least invasive fallback.

**Product principle:** The fastest radial menu is the one the user barely notices until it is needed. Prioritize predictable behavior over visual complexity and a small, understandable implementation over feature accumulation.
