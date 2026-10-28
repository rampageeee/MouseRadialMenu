# Build evidence and measurement record

Recorded 2026-10-28. These are build/headless checks on Linux, not Windows runtime acceptance.

## Environment and automated results

- Linux x86_64, kernel `6.8.0-139-generic`; native GCC 13.3.0; CMake 3.28.3.
- Windows x64 cross-toolchain: Ubuntu MinGW-w64 GCC 13-posix packages (`13.2.0-6ubuntu1+26.1`), MinGW headers/runtime 11.0.1, C++20, CMake Release, static libgcc/libstdc++ and runtime linkage, installed with `--strip`.
- Headless suite: **261 checks passed**, CTest 1/1 passed. Checks exercise actual JSON files, chord grammar, duplicate/invalid/missing config, bounded depth, snapshot replacement, case-insensitive matching, nested navigation/dwell/rearm/back/cancel, 1–8 segment geometry and 200 repeated state-machine cycles.
- AddressSanitizer + UndefinedBehaviorSanitizer suite: 1/1 passed with `ASAN_OPTIONS=detect_leaks=0`. LeakSanitizer cannot run under this environment's process tracing; its initial attempt stopped with the documented ptrace restriction. No leak-check result is claimed.
- Windows x64 cross-build: TinyRadial, InputProbe and core_tests built successfully. Windows binaries were not executed here. Native MSVC build/test and CI execution remain pending.
- PE import inspection: TinyRadial imports only `ADVAPI32`, `d2d1`, `DWrite`, `GDI32`, `KERNEL32`, `msvcrt`, `ole32`, `SHELL32`, `USER32` system DLLs. No external GCC, winpthread or application framework DLL is required by this artifact. This is static inspection, not evidence of all runtime behavior.

## Binary size

The packaged cross-built `TinyRadial.exe` is approximately **1.23 MB** (1.18 MiB), below the preferred 3,000,000-byte target. Exact bytes and SHA-256 for the final test artifact are in `BUILD_INFO.txt` beside the executable. The test-only InputProbe is approximately 0.98 MB and is never started by TinyRadial. Native MSVC output may differ and must be measured independently. No signed artifact is produced.

## Required Windows measurements: pending

| Metric | Target | Current result |
| --- | --- | --- |
| TinyRadial resident process count | 1 | Unmeasured on Windows |
| Idle Private Bytes after warm-up | <= 20 MiB | Unmeasured |
| Idle CPU | Near zero | Unmeasured |
| Startup | Aspirational < 200 ms | Unmeasured |
| Hotkey to first visible frame | Aspirational < 50 ms | Unmeasured |
| Network activity | None | No networking code/imports; runtime observation pending |

For idle evidence, launch TinyRadial, close InputProbe and the menu, wait 30 seconds, then run PowerShell:

```powershell
$RadialProcessId = (Get-Process TinyRadial).Id
.\tools\measure-idle.ps1 -RadialProcessId $RadialProcessId -Seconds 60
```

The script adds ten seconds of warm-up, samples Private Bytes each second, records interval CPU-time delta and reports CPU as a percentage of one logical core. Divide by logical processor count if comparing against whole-machine Task Manager percentages. It reports hash, bytes, OS version and process count. Copy the output into the host acceptance record with CPU/GPU, display scale and compiler information.

Startup and opening latency need separate evidence: record the measurement endpoints, method and uncertainty. High-frame-rate video of hotkey activation and overlay appearance can provide a coarse opening estimate; instrumented QPC timestamps or ETW are preferable for precise endpoints. Do not use `Start-Process` return time or `WaitForInputIdle` alone as evidence of first visible frame. No latency compliance is claimed until measured.

Re-run the [manual matrix](MANUAL_TESTS.md) for keyboard safety and host behavior. Close the optional probe before measuring resident resources.

## Hold/release follow-up

The opt-in hold implementation extends the suite to **294 checks**, passed in fresh native Debug and ASan/UBSan builds on the same Linux host. Sanitizer execution uses `ASAN_OPTIONS=detect_leaks=0`; no leak-check result is claimed. Windows x64 cross-compilation passes. Earlier artifact hashes, sizes and 261-check records above describe the pre-hold build. Hold mode retains a keyboard observer while enabled; its Windows input behavior and idle overhead still require measurement.
