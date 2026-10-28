# Dependencies and reproducibility

Production: C++20 standard library, system Win32/User32/GDI/Shell/Advapi/Ole32, Direct2D and DirectWrite. No network code, embedded browser, runtime interpreter, plugins or package-manager step at app launch.

The only vendored source dependency is nlohmann/json 3.11.3 (MIT). Full source/license are in `third_party/nlohmann`. The pinned source tag and SHA-256 are in `THIRD_PARTY_NOTICES.md`. Builds use the local header, never FetchContent or a floating version.

Primary release toolchain: Visual Studio 2022 C++ Build Tools, Windows SDK and CMake >= 3.22, x64, Release, static MSVC CRT. `tools/build-release.ps1` and `.github/workflows/build.yml` encode the build/test/package steps. The build is reproducible as a procedure, not a claim of byte-for-byte identical binaries across compiler versions.

The initial locally built test artifact uses GCC MinGW-w64 13-posix (Ubuntu packages), targeting x86_64 Windows with statically linked libgcc/libstdc++/winpthreads. GCC runtime libraries are covered by GPLv3 with the GCC Runtime Library Exception; MinGW-w64 runtime licensing includes permissive notices and component exceptions. These toolchain runtimes are not additional app frameworks. Consult toolchain license material shipped alongside the cross-built ZIP. MSVC-generated packages do not use these GCC runtime libraries.

Linux headless verification needs a C++20 compiler and CMake; Windows cross-building can use MinGW-w64 with `-DCMAKE_SYSTEM_NAME=Windows -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++`. Native Windows builds are preferred for releases. No cross-toolchain binaries or downloaded package archives belong in source control.
