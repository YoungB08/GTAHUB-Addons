# OMPVOICE CORE – MASTER CHECKLIST (ALL TASKS)

## Mục tiêu

Checklist này dùng để **theo dõi toàn bộ tiến độ phát triển OMPVoiceCore** theo từng module. Mỗi mục phải được đánh dấu **DONE / TESTED / VERIFIED** trước khi chuyển sang giai đoạn tiếp theo.

---

# 0. PROJECT INITIALIZATION

## Repository

- [x] Tạo Git repository
- [x] Tạo branch `main`
- [x] Tạo branch `dev`
- [x] Tạo `.gitignore` cho VS2022 + CMake
- [x] Tạo `README.md`
- [x] Tạo `LICENSE`

## Build system

- [x] CMakeLists.txt root
- [x] Win32 build
- [x] x64 build
- [ ] Visual Studio 2022 solution generate
- [x] Output thư mục `build/bin`
- [x] Output thư mục `build/lib`

---

# 1. THIRD PARTY DEPENDENCIES

## BASS

- [x] bass.lib
- [x] bass.dll
- [x] Include path đúng
- [x] Runtime copy sau build

## BASS FX

- [x] bass_fx.lib
- [x] bass_fx.dll

## Opus

- [x] opus.lib
- [x] Include path đúng

## ImGui

- [x] imgui core
- [x] imgui_impl_dx9
- [x] imgui_impl_win32

## MinHook

- [x] Compile static
- [ ] Hook test thành công

## JSON

- [x] nlohmann/json include
- [x] Parse test OK

---

# 2. SERVER COMPONENT CORE

## open.mp Component

- [x] `OMPVoiceCore` class
- [x] `onLoad()`
- [x] `onUnload()` (SDK không có callback này; lifecycle test xác nhận `free()` + destructor + `FreeLibrary`)
- [x] `componentName()`
- [x] `COMPONENT_ENTRY_POINT()`

## UID

- [x] `PROVIDE_UID(0xD6FEE4A6B0EA27A3);`

## Voice socket

- [x] UDP socket create
- [x] Bind port 7775
- [x] Error log khi bind fail
- [x] Graceful shutdown socket

---

# 3. PAWN NATIVE REGISTRATION

## Include file

- [x] `ompvoice.inc`
- [x] Include guard
- [x] Native declarations đầy đủ

## Native implementations

- [x] `OV_EnableVoice`
- [x] `OV_IsTalking`
- [x] `OV_SetVolume`
- [x] `OV_SetMuted`
- [x] `OV_CreateChannel`
- [x] `OV_DestroyChannel`
- [x] `OV_SetTalkKey`
- [x] `OV_StartPhoneCall`
- [x] `OV_EndPhoneCall`
- [x] `OV_ShowHudIcon`

## Registration

- [x] PawnManager registration
- [x] Native lookup test
- [x] Gamemode compile test

---

# 4. CLIENT INJECTION

## ASI loader

- [x] `DllMain`
- [x] Threaded initialization
- [x] Safe shutdown
- [ ] Unload support

## SA:MP detection

- [x] `samp.dll` detect
- [x] 0.3.DL R1 detect
- [x] Version fallback
- [x] Error message nếu unsupported

---

# 5. DIRECTX9 HOOK

## Hook setup

- [x] Get D3D9 device
- [x] Hook `EndScene`
- [x] Hook `Reset`
- [x] Hook restore on unload

## Rendering

- [x] Sprite renderer
- [x] Texture manager
- [x] Font rendering
- [x] Alpha blending đúng

## ALT+TAB

- [x] Invalidate device objects
- [x] Recreate device objects
- [x] Texture reload
- [ ] No crash after 20 ALT+TAB tests

---

# 6. RESOURCE SYSTEM

## PNG loading

- [x] `logo.png`
- [x] `micro_active.png`
- [x] `micro_muted.png`
- [x] `micro_passive.png`
- [x] `speaker.png`

## Fallback

- [x] Missing file detection
- [x] Error logging
- [x] Placeholder texture create
- [x] Continue running without crash

---

# 7. AUDIO ENGINE INITIALIZATION

## BASS init

- [x] `BASS_Init`
- [x] Output device enumerate
- [x] Default device select
- [x] Device change support

## Capture init

- [x] `BASS_RecordInit`
- [x] Input device enumerate
- [x] Default mic select
- [x] Start/stop capture stable

---

# 8. MICROPHONE CAPTURE

## Format

- [x] 48000 Hz
- [x] 16-bit PCM
- [x] Mono
- [x] 20 ms frame

## Processing

- [x] RMS calculation
- [x] Peak calculation
- [x] Buffer queue
- [x] Overflow protection

---

# 9. PUSH TO TALK

## Input

- [x] Default key Z
- [x] Rebind support
- [x] Key capture UI
- [x] Hold-to-talk stable

## State machine

- [x] Idle
- [x] Pressed
- [x] Transmitting
- [x] Released
- [x] Timeout recovery

---

# 10. VOICE ACTIVATION

## Detection

- [x] RMS threshold
- [x] Configurable threshold
- [x] Attack time
- [x] Release time

## UI

- [x] Enable checkbox
- [x] Threshold slider
- [x] Live level meter

---

# 11. AUDIO EFFECTS

## Master volume

- [x] Slider 0–100
- [x] Real BASS volume apply
- [x] Save config

## Microphone gain

- [x] Gain multiplier
- [x] No clipping at 200%
- [x] Real-time update

## High-pass filter

- [x] BASS FX create
- [x] 120 Hz cutoff
- [x] Toggle on/off

## Noise gate

- [x] Silence detection
- [x] Packet suppression
- [x] Adjustable threshold

## AGC

- [x] Automatic gain adjust
- [x] Stable target RMS
- [x] No pumping artifacts

---

# 12. OPUS ENCODER

## Initialization

- [x] Encoder create
- [x] Bitrate 24000
- [x] Complexity 5
- [x] DTX enabled
- [x] FEC enabled

## Encode path

- [x] PCM → Opus
- [x] Frame size correct
- [x] Buffer reuse
- [x] Error handling

---

# 13. OPUS DECODER

## Initialization

- [x] Decoder create
- [x] 48 kHz config

## Decode path

- [x] Opus → PCM
- [x] PLC support
- [x] Corrupted packet handling
- [x] Stream reset support

---

# 14. NETWORK PROTOCOL

## Packet structures

- [x] Handshake
- [x] VoiceBegin
- [x] VoiceData
- [x] VoiceEnd
- [x] Ping
- [x] Pong

## Serialization

- [x] Little-endian consistency
- [x] Bounds checking
- [x] Version field
- [x] Magic validation

---

# 15. CONNECTION MANAGEMENT

## Handshake

- [x] Client send handshake
- [x] Server validate magic
- [x] Version validation
- [x] UID validation

## Reconnect

- [x] Detect disconnect
- [x] Retry 1s
- [x] Retry 3s
- [x] Retry 5s
- [x] Retry 10s

---

# 16. JITTER BUFFER

## Queue

- [x] Sequence ordering
- [x] Duplicate packet ignore
- [x] Late packet handling
- [x] Buffer underrun handling

## Playback

- [x] 40 ms target
- [x] Smooth playback
- [x] No stutter under 5% loss

---

# 17. REMOTE STREAM MANAGEMENT

## Stream lifecycle

- [x] Create per player
- [x] Destroy on disconnect
- [x] Timeout after silence
- [x] Memory cleanup

## Limits

- [x] Max simultaneous voices = 8
- [x] Priority sorting
- [x] Closest player selection

---

# 18. PROXIMITY AUDIO

## Distance

- [x] Position fetch
- [x] Distance calculation
- [x] Volume attenuation
- [x] Pan calculation

## Validation

- [x] 1m = full volume
- [x] 25m = near zero
- [x] No negative volume
- [x] Stereo pan clamped

---

# 19. GLOBAL CHANNEL

## Logic

- [x] `distance == -1.0f`
- [x] Skip distance checks
- [x] Full volume
- [x] Center pan

---

# 20. VEHICLE MODE

## Detection

- [x] Same vehicle check
- [x] Vehicle enter update
- [x] Vehicle exit update

## Audio

- [x] Ignore distance
- [x] Full volume
- [x] Center pan

---

# 21. RADIO CHANNEL

## Effects

- [x] Band-pass filter
- [x] Compression
- [x] Static noise layer
- [x] Toggle effect per channel

## API

- [x] Create radio channel
- [x] Join radio channel
- [x] Leave radio channel

---

# 22. PHONE CALL SYSTEM

## Core

- [x] Start call
- [x] End call
- [x] Busy state
- [x] Disconnect cleanup

## Leak radius

- [x] Default 8m
- [x] Configurable 5–10m
- [x] Nearby listener hears partial audio
- [x] Far listener hears nothing

## Phone effect

- [x] 300–3400 Hz EQ
- [x] Slight compression
- [x] GSM-like sound

---

# 23. HUD MICROPHONE

## States

- [x] Passive icon
- [x] Active icon
- [x] Muted icon

## Position

- [x] Bottom-center default
- [x] Scale slider
- [x] X offset
- [x] Y offset
- [x] Drag & drop move mode

---

# 24. SPEAKER ICON

## Render conditions

- [x] Player transmitting
- [x] Not local player
- [x] In camera frustum
- [x] Distance visible

## Visuals

- [x] `speaker.png`
- [x] Pulse animation
- [x] Scale setting
- [x] Offset X/Y setting

---

# 25. SETTINGS WINDOW

## General tab

- [x] Enable sound
- [x] Master volume
- [x] Smoothing
- [x] High-pass filter
- [x] Noise suppression
- [x] AGC
- [x] Speaker icon options

## Microphone tab

- [x] Enable microphone
- [x] Input device combo
- [x] Mic volume slider
- [x] Test microphone
- [x] HUD icon controls

## Black list tab

- [x] Search field
- [x] Online players list
- [x] Mute toggle
- [x] Save blacklist

## Debug tab

- [x] Loopback toggle
- [x] Mirror mode toggle
- [x] Fake remote toggle
- [x] Packet simulator controls
- [x] Oscilloscope
- [x] Diagnostic button

---

# 26. SELF-DEBUG FEATURES

## Loopback mode (F8)

- [x] Encode local voice
- [x] Decode locally
- [x] Hear own processed voice
- [x] No network required

## Mirror mode

- [x] Simulate server echo
- [x] Pass through jitter buffer
- [x] Pass through decoder
- [x] Trigger remote playback path

## Fake remote player (F9)

- [x] Spawn fake remote entity
- [x] Render speaker icon
- [x] Simulate talking timeout
- [x] Test attenuation and pan

## Packet simulator

- [x] Packet loss slider
- [x] Latency slider
- [x] Jitter slider
- [x] Real packet dropping

## Oscilloscope

- [x] Waveform display
- [x] RMS graph
- [x] Peak meter
- [x] Update in real time

## Diagnostic command

- [x] `/ovdiag`
- [x] Generate text report
- [x] Check all subsystems
- [x] Save to `debug/diagnostic_report.txt`

---

# 27. LOGGING SYSTEM

## Files

- [x] `client.log`
- [x] `server.log`
- [x] `audio.log`
- [x] `network.log`

## Features

- [x] Timestamp
- [x] Thread name
- [x] Log level
- [x] File rotation

---

# 28. CRASH SAFETY

## Exception handling

- [x] Unhandled exception filter
- [x] Minidump generation
- [x] Stack trace logging
- [x] Module list logging

## Assertions

- [x] `OV_ASSERT`
- [x] Debug break in Debug build
- [x] Error log in Release build

---

# 29. THREADING

## Audio thread

- [x] Capture
- [x] Encode
- [x] Decode
- [x] Jitter processing

## Network thread

- [x] UDP receive
- [x] Packet queue
- [x] Reconnect logic

## Render thread

- [x] DX9 rendering
- [x] ImGui rendering
- [x] HUD rendering

## Validation

- [x] No BASS playback from network thread
- [x] Thread-safe queues
- [x] No deadlocks under stress test

---

# 30. MEMORY MANAGEMENT

## RAII

- [x] `std::unique_ptr`
- [x] `std::shared_ptr`
- [x] Custom handle wrappers

## Leak checks

- [x] Client connect/disconnect 100 times
- [x] Open/close settings 100 times
- [ ] Start/stop capture 100 times
- [ ] No growing memory usage

---

# 31. CONFIG SYSTEM

## Load

- [x] Create default config if missing
- [x] Parse JSON safely
- [x] Validate ranges
- [x] Apply defaults on invalid values

## Save

- [x] Save on change
- [x] Pretty-print JSON
- [x] Atomic write
- [x] Backup previous config

## Hot reload

- [x] `/ovreload`
- [x] Runtime apply volume
- [x] Runtime apply filters
- [x] Runtime apply icon settings

---

# 32. PERFORMANCE TESTING

## CPU

- [ ] Idle < 2%
- [ ] One active voice < 4%
- [ ] Eight active voices < 10%

## Network

- [x] Average < 8 KB/s per speaker
- [x] Stable under 100 ms latency
- [x] Stable under 5% packet loss

## Latency

- [x] Capture < 5 ms
- [x] Encode < 2 ms
- [x] Decode < 1 ms
- [x] End-to-end < 80 ms

---

# 33. STRESS TESTS

## Client

- [ ] Hold PTT for 10 minutes
- [ ] ALT+TAB spam 50 times
- [ ] Change microphone while connected
- [x] Disconnect network cable simulation

## Server

- [x] 50 fake voice clients
- [x] Rapid connect/disconnect
- [x] Global channel spam
- [x] Phone call spam

---

# 34. RELEASE PACKAGING

## Files

- [x] `ompvoice.dll`
- [x] `ov_client.asi`
- [x] `ompvoice.inc`
- [x] `bass.dll`
- [x] `bass_fx.dll`
- [x] `README.md`
- [x] `INSTALL.md`
- [x] `CHANGELOG.md`

## Verification

- [ ] Clean Windows 10 test
- [ ] Clean Windows 11 test
- [ ] Fresh GTA SA + open.mp install test

---

# 35. FINAL ACCEPTANCE

## Must pass ALL

### Core

- [x] Component loads
- [x] UID valid
- [x] UDP 7775 bound
- [x] Handshake works
- [x] Reconnect works

### Audio

- [ ] Capture works
- [x] Encode works
- [x] Decode works
- [x] Effects work
- [x] 3D audio works

### UI

- [ ] All tabs functional
- [ ] No ImGui crashes
- [x] Settings persist

### Render

- [ ] HUD icon correct
- [ ] Speaker icon correct
- [ ] No DX9 device leaks

### Self-debug

- [ ] Loopback works
- [ ] Mirror mode works
- [ ] Fake remote works
- [x] Packet simulator works
- [ ] Oscilloscope works
- [x] Diagnostic report works

### Stability

- [ ] No crash after 1 hour idle
- [ ] No crash after 1 hour voice spam
- [ ] No memory leaks detected
- [ ] No handle leaks detected

---

# RELEASE STATUS

| Module | Status | Tested | Verified |
|---|---|---|---|
| Server Core | ☐ | ☐ | ☐ |
| Client Injection | ☐ | ☐ | ☐ |
| DX9 Renderer | ☐ | ☐ | ☐ |
| Audio Engine | ☐ | ☐ | ☐ |
| Opus Codec | ☐ | ☐ | ☐ |
| Network | ☐ | ☐ | ☐ |
| Channels | ☐ | ☐ | ☐ |
| Phone Call | ☐ | ☐ | ☐ |
| HUD | ☐ | ☐ | ☐ |
| Speaker Icon | ☐ | ☐ | ☐ |
| Settings UI | ☐ | ☐ | ☐ |
| Self-Debug Tools | ☐ | ☐ | ☐ |
| Logging | ☐ | ☐ | ☐ |
| Crash Safety | ☐ | ☐ | ☐ |
| Packaging | ☐ | ☐ | ☐ |

---

## READY FOR RELEASE

- [ ] Tất cả checklist đều **DONE**
- [ ] Tất cả mục **TESTED**
- [ ] Tất cả mục **VERIFIED**
- [ ] Không còn crash hoặc memory leak
- [ ] Có thể debug hoàn toàn với **1 client duy nhất**
