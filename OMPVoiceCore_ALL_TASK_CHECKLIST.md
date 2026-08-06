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

- [ ] `OMPVoiceCore` class
- [ ] `onLoad()`
- [ ] `onUnload()`
- [ ] `componentName()`
- [ ] `COMPONENT_ENTRY_POINT()`

## UID

- [ ] `PROVIDE_UID(0xD6FEE4A6B0EA27A3);`

## Voice socket

- [ ] UDP socket create
- [ ] Bind port 7775
- [ ] Error log khi bind fail
- [ ] Graceful shutdown socket

---

# 3. PAWN NATIVE REGISTRATION

## Include file

- [ ] `ompvoice.inc`
- [ ] Include guard
- [ ] Native declarations đầy đủ

## Native implementations

- [ ] `OV_EnableVoice`
- [ ] `OV_IsTalking`
- [ ] `OV_SetVolume`
- [ ] `OV_SetMuted`
- [ ] `OV_CreateChannel`
- [ ] `OV_DestroyChannel`
- [ ] `OV_SetTalkKey`
- [ ] `OV_StartPhoneCall`
- [ ] `OV_EndPhoneCall`
- [ ] `OV_ShowHudIcon`

## Registration

- [ ] PawnManager registration
- [ ] Native lookup test
- [ ] Gamemode compile test

---

# 4. CLIENT INJECTION

## ASI loader

- [ ] `DllMain`
- [ ] Threaded initialization
- [ ] Safe shutdown
- [ ] Unload support

## SA:MP detection

- [ ] `samp.dll` detect
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

- [ ] Missing file detection
- [ ] Error logging
- [ ] Placeholder texture create
- [ ] Continue running without crash

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

- [ ] 48000 Hz
- [ ] 16-bit PCM
- [ ] Mono
- [ ] 20 ms frame

## Processing

- [ ] RMS calculation
- [ ] Peak calculation
- [ ] Buffer queue
- [ ] Overflow protection

---

# 9. PUSH TO TALK

## Input

- [ ] Default key Z
- [ ] Rebind support
- [ ] Key capture UI
- [ ] Hold-to-talk stable

## State machine

- [ ] Idle
- [ ] Pressed
- [ ] Transmitting
- [ ] Released
- [ ] Timeout recovery

---

# 10. VOICE ACTIVATION

## Detection

- [ ] RMS threshold
- [ ] Configurable threshold
- [ ] Attack time
- [ ] Release time

## UI

- [ ] Enable checkbox
- [ ] Threshold slider
- [ ] Live level meter

---

# 11. AUDIO EFFECTS

## Master volume

- [ ] Slider 0–100
- [ ] Real BASS volume apply
- [ ] Save config

## Microphone gain

- [ ] Gain multiplier
- [ ] No clipping at 200%
- [ ] Real-time update

## High-pass filter

- [ ] BASS FX create
- [ ] 120 Hz cutoff
- [ ] Toggle on/off

## Noise gate

- [ ] Silence detection
- [ ] Packet suppression
- [ ] Adjustable threshold

## AGC

- [ ] Automatic gain adjust
- [ ] Stable target RMS
- [ ] No pumping artifacts

---

# 12. OPUS ENCODER

## Initialization

- [ ] Encoder create
- [ ] Bitrate 24000
- [ ] Complexity 5
- [ ] DTX enabled
- [ ] FEC enabled

## Encode path

- [ ] PCM → Opus
- [ ] Frame size correct
- [ ] Buffer reuse
- [ ] Error handling

---

# 13. OPUS DECODER

## Initialization

- [ ] Decoder create
- [ ] 48 kHz config

## Decode path

- [ ] Opus → PCM
- [ ] PLC support
- [ ] Corrupted packet handling
- [ ] Stream reset support

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

- [ ] Client send handshake
- [ ] Server validate magic
- [ ] Version validation
- [ ] UID validation

## Reconnect

- [ ] Detect disconnect
- [ ] Retry 1s
- [ ] Retry 3s
- [ ] Retry 5s
- [ ] Retry 10s

---

# 16. JITTER BUFFER

## Queue

- [ ] Sequence ordering
- [ ] Duplicate packet ignore
- [ ] Late packet handling
- [ ] Buffer underrun handling

## Playback

- [ ] 40 ms target
- [ ] Smooth playback
- [ ] No stutter under 5% loss

---

# 17. REMOTE STREAM MANAGEMENT

## Stream lifecycle

- [ ] Create per player
- [ ] Destroy on disconnect
- [ ] Timeout after silence
- [ ] Memory cleanup

## Limits

- [ ] Max simultaneous voices = 8
- [ ] Priority sorting
- [ ] Closest player selection

---

# 18. PROXIMITY AUDIO

## Distance

- [ ] Position fetch
- [ ] Distance calculation
- [ ] Volume attenuation
- [ ] Pan calculation

## Validation

- [ ] 1m = full volume
- [ ] 25m = near zero
- [ ] No negative volume
- [ ] Stereo pan clamped

---

# 19. GLOBAL CHANNEL

## Logic

- [ ] `distance == -1.0f`
- [ ] Skip distance checks
- [ ] Full volume
- [ ] Center pan

---

# 20. VEHICLE MODE

## Detection

- [ ] Same vehicle check
- [ ] Vehicle enter update
- [ ] Vehicle exit update

## Audio

- [ ] Ignore distance
- [ ] Full volume
- [ ] Center pan

---

# 21. RADIO CHANNEL

## Effects

- [ ] Band-pass filter
- [ ] Compression
- [ ] Static noise layer
- [ ] Toggle effect per channel

## API

- [ ] Create radio channel
- [ ] Join radio channel
- [ ] Leave radio channel

---

# 22. PHONE CALL SYSTEM

## Core

- [ ] Start call
- [ ] End call
- [ ] Busy state
- [ ] Disconnect cleanup

## Leak radius

- [ ] Default 8m
- [ ] Configurable 5–10m
- [ ] Nearby listener hears partial audio
- [ ] Far listener hears nothing

## Phone effect

- [ ] 300–3400 Hz EQ
- [ ] Slight compression
- [ ] GSM-like sound

---

# 23. HUD MICROPHONE

## States

- [ ] Passive icon
- [ ] Active icon
- [ ] Muted icon

## Position

- [ ] Bottom-center default
- [ ] Scale slider
- [ ] X offset
- [ ] Y offset
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

- [ ] Enable sound
- [ ] Master volume
- [ ] Smoothing
- [ ] High-pass filter
- [ ] Noise suppression
- [ ] AGC
- [ ] Speaker icon options

## Microphone tab

- [ ] Enable microphone
- [ ] Input device combo
- [ ] Mic volume slider
- [ ] Test microphone
- [ ] HUD icon controls

## Black list tab

- [ ] Search field
- [ ] Online players list
- [ ] Mute toggle
- [ ] Save blacklist

## Debug tab

- [ ] Loopback toggle
- [ ] Mirror mode toggle
- [ ] Fake remote toggle
- [ ] Packet simulator controls
- [ ] Oscilloscope
- [ ] Diagnostic button

---

# 26. SELF-DEBUG FEATURES

## Loopback mode (F8)

- [ ] Encode local voice
- [ ] Decode locally
- [ ] Hear own processed voice
- [ ] No network required

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

- [ ] Packet loss slider
- [ ] Latency slider
- [ ] Jitter slider
- [ ] Real packet dropping

## Oscilloscope

- [ ] Waveform display
- [ ] RMS graph
- [ ] Peak meter
- [ ] Update in real time

## Diagnostic command

- [ ] `/ovdiag`
- [ ] Generate text report
- [ ] Check all subsystems
- [ ] Save to `debug/diagnostic_report.txt`

---

# 27. LOGGING SYSTEM

## Files

- [ ] `client.log`
- [ ] `server.log`
- [ ] `audio.log`
- [ ] `network.log`

## Features

- [ ] Timestamp
- [ ] Thread name
- [ ] Log level
- [ ] File rotation

---

# 28. CRASH SAFETY

## Exception handling

- [ ] Unhandled exception filter
- [ ] Minidump generation
- [ ] Stack trace logging
- [ ] Module list logging

## Assertions

- [ ] `OV_ASSERT`
- [ ] Debug break in Debug build
- [ ] Error log in Release build

---

# 29. THREADING

## Audio thread

- [ ] Capture
- [ ] Encode
- [ ] Decode
- [ ] Jitter processing

## Network thread

- [ ] UDP receive
- [ ] Packet queue
- [ ] Reconnect logic

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

- [ ] `std::unique_ptr`
- [ ] `std::shared_ptr`
- [ ] Custom handle wrappers

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

- [ ] `ompvoice.dll`
- [ ] `ov_client.asi`
- [ ] `ompvoice.inc`
- [ ] `bass.dll`
- [ ] `bass_fx.dll`
- [ ] `README.md`
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
