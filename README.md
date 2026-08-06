# GTAHUB Addons

This branch develops OMPVoiceCore, a C++17 voice-chat system for an open.mp
server and the SA:MP 0.3.DL R1 client. Source is under `OMPVoiceCore/` and is
split into a 64-bit-capable server component, a Win32 ASI client, shared wire
protocol, Pawn include, tests, resources, and release packaging.

## Build

Visual Studio 2022 is the release toolchain. Visual Studio 2019 remains useful
for local compatibility checks because the open.mp SDK supports VS 2019+.

```powershell
cmake -S . -B out/vs2022-x86 -G "Visual Studio 17 2022" -A Win32
cmake --build out/vs2022-x86 --config Release --parallel
ctest --test-dir out/vs2022-x86 -C Release --output-on-failure
```

For the x64 server build, use `-A x64`; the 32-bit ASI target is intentionally
excluded from x64 configurations. Official 32-bit `bass.dll` and `bass_fx.dll`
must be provided at packaging/runtime because their licenses do not permit the
repository to treat them as ordinary open-source FetchContent dependencies.
