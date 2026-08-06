# OMPVOICE CORE – MASTER CHECKLIST (ALL TASKS)

## Mục tiêu

Checklist này dùng để **theo dõi toàn bộ tiến độ phát triển OMPVoiceCore** theo từng module. Mỗi mục phải được đánh dấu **DONE / TESTED / VERIFIED** trước khi chuyển sang giai đoạn tiếp theo.

---

# 0. PROJECT INITIALIZATION

## Repository

- [x] Tạo Git repository
- [x] Tạo branch `main`
- [ ] Tạo branch `dev`
- [x] Tạo `.gitignore` cho VS2022 + CMake
- [x] Tạo `README.md`
- [x] Tạo `LICENSE`

## Build system

- [x] CMakeLists.txt root
- [ ] Win32 build
- [ ] x64 build
- [ ] Visual Studio 2022 solution generate
- [x] Output thư mục `build/bin`
- [x] Output thư mục `build/lib`

---

# 1. THIRD PARTY DEPENDENCIES

## BASS

- [ ] bass.lib
- [ ] bass.dll
- [ ] Include path đúng
- [ ] Runtime copy sau build

## BASS FX

- [ ] bass_fx.lib
- [ ] bass_fx.dll

## Opus

- [ ] opus.lib
- [ ] Include path đúng

## ImGui

- [ ] imgui core
- [ ] imgui_impl_dx9
- [ ] imgui_impl_win32

## MinHook

- [ ] Compile static
- [ ] Hook test thành công

## JSON

- [x] nlohmann/json include
- [x] Parse test OK

---

# 2. SERVER COMPONENT CORE

## open.mp Component

- [x] `OMPVoiceCore` class
- [x] `onLoad()`
- [ ] `onUnload()` (SDK không có callback này; destructor + `free()` đảm nhiệm unload)
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
- [ ] Native declarations đầy đủ

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
- [ ] Native lookup test
- [ ] Gamemode compile test

---

# 4. CLIENT INJECTION

## ASI loader

- [x] `DllMain`
- [x] Threaded initialization
- [x] Safe shutdown
- [ ] Unload support

## SA:MP detection

- [x] `samp.dll` detect
- [ ] 0.3.DL R1 detect
- [ ] Version fallback
- [ ] Error message nếu unsupported

---

# 5. DIRECTX9 HOOK

## Hook setup

- [ ] Get D3D9 device
- [ ] Hook `EndScene`
- [ ] Hook `Reset`
- [ ] Hook restore on unload

## Rendering

- [ ] Sprite renderer
- [ ] Texture manager
- [ ] Font rendering
- [ ] Alpha blending đúng

## ALT+TAB

- [ ] Invalidate device objects
- [ ] Recreate device objects
- [ ] Texture reload
- [ ] No crash after 20 ALT+TAB tests

---

# 6. RESOURCE SYSTEM

## PNG loading

- [ ] `logo.png`
- [ ] `micro_active.png`
- [ ] `micro_muted.png`
- [ ] `micro_passive.png`
- [ ] `speaker.png`

## Fallback

- [x] Missing file detection
- [x] Error logging
- [x] Placeholder texture create
- [x] Continue running without crash

---

# 7. AUDIO ENGINE INITIALIZATION

## BASS init

- [ ] `BASS_Init`
- [ ] Output device enumerate
- [ ] Default device select
- [ ] Device change support

## Capture init

- [ ] `BASS_RecordInit`
- [ ] Input device enumerate
- [ ] Default mic select
- [ ] Start/stop capture stable

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
- [ ] Key capture UI
- [ ] Hold-to-talk stable

## State machine

- [x] Idle
- [x] Pressed
- [x] Transmitting
- [x] Released
- [ ] Timeout recovery

---

# 10. VOICE ACTIVATION

## Detection

- [ ] RMS threshold
- [ ] Configurable threshold
- [ ] Attack time
- [ ] Release time

## UI

- [x] Enable checkbox
- [x] Threshold slider
- [ ] Live level meter

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

- [ ] BASS FX create
- [x] 120 Hz cutoff
- [x] Toggle on/off

## Noise gate

- [x] Silence detection
- [x] Packet suppression
- [x] Adjustable threshold

## AGC

- [x] Automatic gain adjust
- [x] Stable target RMS
- [ ] No pumping artifacts

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
- [ ] Smooth playback
- [ ] No stutter under 5% loss

---

# 17. REMOTE STREAM MANAGEMENT

## Stream lifecycle

- [x] Create per player
- [x] Destroy on disconnect
- [ ] Timeout after silence
- [x] Memory cleanup

## Limits

- [ ] Max simultaneous voices = 8
- [ ] Priority sorting
- [ ] Closest player selection

---

# 18. PROXIMITY AUDIO

## Distance

- [x] Position fetch
- [x] Distance calculation
- [x] Volume attenuation
- [x] Pan calculation

## Validation

- [ ] 1m = full volume
- [ ] 25m = near zero
- [ ] No negative volume
- [ ] Stereo pan clamped

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

- [ ] Band-pass filter
- [ ] Compression
- [ ] Static noise layer
- [ ] Toggle effect per channel

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
- [ ] Configurable 5–10m
- [x] Nearby listener hears partial audio
- [x] Far listener hears nothing

## Phone effect

- [ ] 300–3400 Hz EQ
- [ ] Slight compression
- [ ] GSM-like sound

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
- [ ] Drag & drop move mode

---

# 24. SPEAKER ICON

## Render conditions

- [ ] Player transmitting
- [ ] Not local player
- [ ] In camera frustum
- [ ] Distance visible

## Visuals

- [ ] `speaker.png`
- [ ] Pulse animation
- [ ] Scale setting
- [ ] Offset X/Y setting

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
- [ ] Input device combo
- [x] Mic volume slider
- [x] Test microphone
- [x] HUD icon controls

## Black list tab

- [x] Search field
- [ ] Online players list
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

- [ ] Simulate server echo
- [ ] Pass through jitter buffer
- [ ] Pass through decoder
- [ ] Trigger remote playback path

## Fake remote player (F9)

- [ ] Spawn fake remote entity
- [ ] Render speaker icon
- [ ] Simulate talking timeout
- [ ] Test attenuation and pan

## Packet simulator

- [x] Packet loss slider
- [x] Latency slider
- [x] Jitter slider
- [ ] Real packet dropping

## Oscilloscope

- [ ] Waveform display
- [x] RMS graph
- [ ] Peak meter
- [x] Update in real time

## Diagnostic command

- [ ] `/ovdiag`
- [x] Generate text report
- [x] Check all subsystems
- [x] Save to `debug/diagnostic_report.txt`

---

# 27. LOGGING SYSTEM

## Files

- [ ] `client.log`
- [ ] `server.log`
- [ ] `audio.log`
- [ ] `network.log`

## Features

- [x] Timestamp
- [x] Thread name
- [x] Log level
- [x] File rotation

---

# 28. CRASH SAFETY

## Exception handling

- [ ] Unhandled exception filter
- [ ] Minidump generation
- [ ] Stack trace logging
- [ ] Module list logging

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

- [ ] DX9 rendering
- [ ] ImGui rendering
- [ ] HUD rendering

## Validation

- [ ] No BASS playback from network thread
- [ ] Thread-safe queues
- [ ] No deadlocks under stress test

---

# 30. MEMORY MANAGEMENT

## RAII

- [x] `std::unique_ptr`
- [ ] `std::shared_ptr`
- [x] Custom handle wrappers

## Leak checks

- [ ] Client connect/disconnect 100 times
- [ ] Open/close settings 100 times
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

- [ ] Save on change
- [x] Pretty-print JSON
- [x] Atomic write
- [x] Backup previous config

## Hot reload

- [ ] `/ovreload`
- [ ] Runtime apply volume
- [ ] Runtime apply filters
- [ ] Runtime apply icon settings

---

# 32. PERFORMANCE TESTING

## CPU

- [ ] Idle < 2%
- [ ] One active voice < 4%
- [ ] Eight active voices < 10%

## Network

- [ ] Average < 8 KB/s per speaker
- [ ] Stable under 100 ms latency
- [ ] Stable under 5% packet loss

## Latency

- [ ] Capture < 5 ms
- [ ] Encode < 2 ms
- [ ] Decode < 1 ms
- [ ] End-to-end < 80 ms

---

# 33. STRESS TESTS

## Client

- [ ] Hold PTT for 10 minutes
- [ ] ALT+TAB spam 50 times
- [ ] Change microphone while connected
- [ ] Disconnect network cable simulation

## Server

- [ ] 50 fake voice clients
- [ ] Rapid connect/disconnect
- [ ] Global channel spam
- [ ] Phone call spam

---

# 34. RELEASE PACKAGING

## Files

- [x] `ompvoice.dll`
- [x] `ov_client.asi`
- [x] `ompvoice.inc`
- [ ] `bass.dll`
- [ ] `bass_fx.dll`
- [x] `README.md`
- [ ] `INSTALL.md`
- [ ] `CHANGELOG.md`

## Verification

- [ ] Clean Windows 10 test
- [ ] Clean Windows 11 test
- [ ] Fresh GTA SA + open.mp install test

---

# 35. FINAL ACCEPTANCE

## Must pass ALL

### Core

- [ ] Component loads
- [ ] UID valid
- [ ] UDP 7775 bound
- [ ] Handshake works
- [ ] Reconnect works

### Audio

- [ ] Capture works
- [ ] Encode works
- [ ] Decode works
- [ ] Effects work
- [ ] 3D audio works

### UI

- [ ] All tabs functional
- [ ] No ImGui crashes
- [ ] Settings persist

### Render

- [ ] HUD icon correct
- [ ] Speaker icon correct
- [ ] No DX9 device leaks

### Self-debug

- [ ] Loopback works
- [ ] Mirror mode works
- [ ] Fake remote works
- [ ] Packet simulator works
- [ ] Oscilloscope works
- [ ] Diagnostic report works

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
