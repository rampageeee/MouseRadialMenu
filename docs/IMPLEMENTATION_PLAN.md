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

- `RegisterHotKey` with `MOD_NOREPEAT`: toggle only. No guessed hold detection or always-on keyboard polling.
- Keyboard/mouse control hooks only while menu open and briefly while draining consumed ups; Esc observation remains while a pending action awaits modifier release. Hook callbacks post messages and never render or inject.
- A foreground observer cancels on a change away from the target, even if focus returns before a timer tick. Window/process identity and target privilege are revalidated before dispatch.
- Cursor restoration, no confinement/capture, no Raw Input interception in production. Active-menu 16 ms hit testing; dispatch wait is bounded to two seconds; no idle timers.
- Reload cancels any open menu/pending action, validates every file, reserves a changed hotkey, then swaps. Failure retains old config and hotkey.
- Actions cannot equal the activation hotkey. Physical modifiers and primary keys must be released before an action. An interrupted `SendInput` batch releases only accepted downs, retries remaining ups twice, and reports failure honestly.

Completed code is awaiting host acceptance; the MVP definition of done is still open until Windows behavior and measurements are recorded. Source publication is at the user's request. Generated builds and test ZIPs are excluded from source control; no verified release has been published.
