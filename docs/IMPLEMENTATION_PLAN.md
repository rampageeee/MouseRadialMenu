# Implementation plan and acceptance gates

The repository initially contained only the MIT license and orchestration brief, on `main`, with no uncommitted changes. Work preserves both. The initial implementation is C++20 + Win32 + Direct2D/DirectWrite + CMake; one pinned header-only JSON dependency, no resident runtimes or networking.

## Execution plan

1. **Repository and input feasibility.** Inspect toolchain and license; create an input observer and a reproducible Windows trial. Select toggle/restore as the dependable initial contract. Record uncertainties and unsupported Raw Input isolation.
2. **Core and configuration.** Define portable interfaces, strict single-chord parser, immutable config snapshots, profile selection and menu navigation. Prove them with headless behavior tests.
3. **Native integration.** Add layered non-activating overlay, scoped input controls, original-target validation, cursor restoration, deferred balanced dispatch and tray commands. Produce a portable test executable.
4. **Hardening and delivery.** Run available build/tests, review cancellation/error paths, package examples/docs/probe, provide native Windows CI and footprint measurement tools. Host validation completes the remaining gates.
5. **Optional virtual cursor.** Deferred until desktop acceptance; no implementation in this change.

The user confirmed on 2026-10-28 that they will perform testing on their Windows 11 host. Source implementation and cross-compilation can proceed here while runtime acceptance remains explicitly pending. This is not a declaration that the brief's Windows gates have passed.

## Architecture and ownership

`src/core` owns chord grammar, config/profile loading, immutable snapshots, and deterministic menu navigation without OS headers. `src/win32/platform` owns target capture, integrity checks and chord injection. `overlay` owns Direct2D/DirectWrite and layered bitmap resources. `main` coordinates hotkey registration, scoped hooks, tray, cancellation and dispatch timers. `tools/InputProbe` is an optional separate test program, never started by TinyRadial.

The coordinator implements production modules sequentially. A bounded QA workstream supplies only the probe/manual docs and a read-only input safety review; no parallel editing of production interfaces or module files.

## Gate status

| Gate | Implemented | Tested | Unverified / limitations | Next |
| --- | --- | --- | --- | --- |
| 0: feasibility | InputProbe, toggle design, input feasibility notes | Probe cross-compiles; source review | Actual Windows activation leakage, non-activation and injection; no game isolation | Run safe probe procedure on host |
| 1: core | Chords, config validation/matching/reload snapshots, nested state machine | 261 headless checks pass on Linux, including ASan/UBSan; Windows test binary cross-built | MSVC-native execution awaits host/CI; LeakSanitizer unavailable under environment tracing | Run Windows CTest |
| 2: desktop | Layered overlay, tray, restore, scope/focus/privilege checks and dispatch | Full Windows x64 cross-build; callback/lifetime review | All interactive Windows acceptance, including Notepad/Explorer/browser/REAPER | Complete manual checklist |
| 3: hardening | Stress/model tests, bounded cleanup, CI, package and measurement scripts | Automated core stress, release file size/import inspection | Actual private bytes, idle CPU, startup, opening latency, mixed DPI and devices | Record Windows results |
| 4: virtual cursor | Deferred | None | Unsupported | Revisit only after desktop acceptance |

## Input decisions

- `RegisterHotKey` with `MOD_NOREPEAT` supplies default toggle activation and reserves the configured chord in hold mode. Opt-in hold uses a physical-event keyboard observer and a core gesture-ownership state machine; no idle polling.
- Keyboard/mouse control hooks handle the open menu and drain consumed ups; Esc observation remains while awaiting modifier release. Hold retains its keyboard hook while enabled and until an owned primary up after cancellation/disable. Callbacks only update input state, capture pointer/target identity, and post messages; they never render or inject.
- A foreground observer cancels on a change away from the target, even if focus returns before a timer tick. Window/process identity and target privilege are revalidated before dispatch.
- Cursor restoration, no confinement/capture, no Raw Input interception in production. Active-menu 16 ms hit testing; dispatch wait is bounded to two seconds; no idle timers.
- Reload cancels any open menu/pending action, validates every file, reserves a changed hotkey, then swaps. Failure retains old config and hotkey.
- Actions cannot equal the activation hotkey. Physical modifiers and primary keys must be released before an action. An interrupted `SendInput` batch releases only accepted downs, retries remaining ups twice, and reports failure honestly.

Completed code is awaiting host acceptance; the MVP definition of done is still open until Windows behavior and measurements are recorded. Source publication is at the user's request. Generated builds and test ZIPs are excluded from source control; no verified release has been published.

## Hold/release implementation follow-up

Opt-in `activation.behavior: "hold"` is now implemented, with toggle still the default. Primary release selects a leaf once and cancels over center/outside/submenus. Dwell/click submenu navigation and center rearm work while holding. Cancellation invalidates queued gesture messages but preserves ownership of the outstanding primary up. Release coordinates/time are captured in the observer so delayed processing cannot select from a later pointer position. Physical modifiers pass normally; action dispatch still waits for their release. New core tests cover ownership, cancellation, repeats, mode changes and menu-release semantics. The dedicated hold-mode Windows matrix in `MANUAL_TESTS.md` remains required. Virtual cursor, REAPER scripting and G502-specific work remain deferred.

Local verification: fresh native Debug build and fresh ASan/UBSan build each pass **294 checks** (CTest 1/1), with leak detection disabled for the sanitizer run because of the environment's tracing restriction. Windows x64 cross-compilation succeeds. [CI run 40692469446](https://github.com/rampageeee/MouseRadialMenu/actions/runs/40692469446) for hold implementation commit `ffda5ee` passed Linux core tests and native Windows/MSVC build, CTest and packaging. Interactive hold acceptance remains pending regardless of compilation/CI results. The earlier gate table records the initial pre-hold implementation.
