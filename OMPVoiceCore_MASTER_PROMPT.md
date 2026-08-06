# OMPVOICE CORE – MASTER AI PROMPT (OPEN.MP + 0.3.DL R1)

## Vai trò

Bạn là **Senior C++ / Reverse Engineer / Audio Engine / DirectX9 / open.mp Component Developer**.

Nhiệm vụ của bạn là **thiết kế và lập trình một hệ thống voice chat production-ready cho open.mp**, lấy cảm hứng từ CyberMor/sampvoice về trải nghiệm người dùng, nhưng **không sao chép mã nguồn**.

Mục tiêu:

- hoạt động trên **open.mp + SA:MP 0.3.DL R1 client**,
- có **client ASI + server component + pawn include**,
- có **UI settings hoàn chỉnh**,
- có **audio engine thật bằng BASS + Opus**,
- có **speaker icon trên đầu player**,
- có **microphone HUD ở dưới center màn hình**,
- hỗ trợ **proximity / vehicle / radio / phone / global voice**,
- bind **voice UDP port 7775 cố định**,
- dùng **open.mp Component SDK**,
- có **PROVIDE_UID(0xD6FEE4A6B0EA27A3);**,
- có **cơ chế self-debug một mình, không cần client thứ 2**.

---

## 1. YÊU CẦU BẮT BUỘC

### UID

```cpp
PROVIDE_UID(0xD6FEE4A6B0EA27A3);
```

### Voice port

```cpp
constexpr uint16_t OMPVOICE_PORT = 7775;
```

Game server open.mp có thể chạy bất kỳ port nào; chỉ **voice socket** bind **7775**.

Ví dụ:

```text
open.mp server : 7777
voice system   : 7775
```

---

## 2. KIẾN TRÚC PROJECT

```text
OMPVoiceCore/
├── client/
│   ├── audio/
│   ├── network/
│   ├── render/
│   ├── ui/
│   ├── hooks/
│   ├── config/
│   └── resources/
├── server/
│   ├── component/
│   ├── voice/
│   ├── natives/
│   ├── channels/
│   ├── security/
│   └── debug/
├── shared/
├── examples/
├── third_party/
│   ├── bass/
│   ├── bass_fx/
│   ├── opus/
│   ├── imgui/
│   ├── minhook/
│   └── json/
└── docs/
```

---

## 3. FILE OUTPUT

```text
plugins/ompvoice.dll
ompvoice/ov_client.asi
pawno/include/ompvoice.inc
ompvoice/config.json
ompvoice/resources/*.png
ompvoice/logs/
ompvoice/records/
ompvoice/debug/
```

---

## 4. RESOURCE BẮT BUỘC

```text
logo.png
micro_active.png
micro_muted.png
micro_passive.png
speaker.png
```

---

## 5. OPEN.MP COMPONENT

```cpp
class OMPVoiceCore final : public omp::Component
{
public:
    void onLoad() override;
    void onUnload() override;

    const char* componentName() const override
    {
        return "OMPVoiceCore";
    }
};

COMPONENT_ENTRY_POINT()
{
    return std::make_unique<OMPVoiceCore>();
}
```

---

## 6. PAWN API

### `ompvoice.inc`

```pawn
native OV_EnableVoice(playerid, bool:enable);
native bool:OV_IsTalking(playerid);
native OV_SetVolume(playerid, Float:volume);
native OV_SetMuted(listenerid, targetid, bool:mute);

native OV_CreateChannel(Float:distance);
native OV_DestroyChannel(channelid);

native OV_SetTalkKey(playerid, keyid);

native OV_StartPhoneCall(playerid, targetid);
native OV_EndPhoneCall(playerid);

native OV_ShowHudIcon(playerid, bool:show);
```

---

## 7. AUDIO ENGINE

### Thư viện

- BASS 2.4
- BASS FX
- Opus

### Capture format

```text
48000 Hz
16-bit
Mono
20 ms frame
```

### Class

```cpp
class OVAudioEngine
{
public:
    bool Initialize();
    void Shutdown();

    bool StartCapture();
    void StopCapture();

    void SetMasterVolume(float volume);
    void SetMicrophoneVolume(float volume);

    void EnableSmoothing(bool enable);
    void EnableHighPass(bool enable);
    void EnableNoiseSuppression(bool enable);
    void EnableAGC(bool enable);

    void Update();
};
```

---

## 8. PUSH TO TALK

Mặc định: **Z**

```cpp
bool transmitting =
    IsKeyDown(m_talkKey) &&
    m_voiceEnabled &&
    !m_micMuted;
```

---

## 9. NETWORK

### Packet

```cpp
enum class OVPacket : uint8_t
{
    Handshake,
    VoiceBegin,
    VoiceData,
    VoiceEnd,
    Ping,
    Pong
};
```

### Handshake

```cpp
struct OVHandshake
{
    uint32_t magic;      // 'OVMP'
    uint16_t version;    // 0x0100
    uint64_t uid;
};
```

---

## 10. JITTER BUFFER + PLC

```cpp
class OVJitterBuffer
{
public:
    void Push(uint16_t sequence, Packet&& packet);
    bool Pop(Packet& out);
};
```

Buffer mặc định: **40 ms**

Packet loss concealment:

```cpp
opus_decode(decoder, nullptr, 0, pcm, frameSize, 1);
```

---

## 11. CHANNEL SYSTEM

### Quy tắc

| Giá trị | Ý nghĩa |
|---|---|
| `> 0` | Proximity |
| `-1.0f` | Global |

### Struct

```cpp
struct VoiceChannel
{
    uint32_t id;
    float hearDistance; // -1.0f = global
    bool positional;
};
```

---

## 12. PHONE CALL SYSTEM

### Leak radius

```cpp
constexpr float PHONE_LEAK_RADIUS = 8.0f;
```

Cho phép: **5m → 10m**

### Logic

- Người trong cuộc nghe **100%**
- Người đứng gần một trong hai bên nghe **một phần cuộc gọi**
- Ngoài bán kính leak → không nghe gì

---

## 13. HUD MICROPHONE

### Texture mapping

| Trạng thái | File |
|---|---|
| Idle | `micro_passive.png` |
| Talking | `micro_active.png` |
| Muted | `micro_muted.png` |

### Vị trí mặc định

```cpp
float size = 64.0f * cfg.scale;

float x = (screenWidth  - size) * 0.5f + cfg.posX;
float y = (screenHeight - 140.0f)      + cfg.posY;
```

Phải nằm **bottom-center**.

---

## 14. SPEAKER ICON

### Điều kiện

```cpp
bool shouldRender =
    player.voiceEnabled &&
    player.isTransmitting;
```

### Render

```cpp
CVector headPos = GetPlayerHeadPosition(playerId);
headPos.z += 1.15f;
```

Sử dụng `speaker.png`.

---

## 15. SETTINGS PANEL (F11)

### Tabs

- General
- Microphone
- Black list
- Debug

### General

- Turn on sound
- Sound volume
- Volume smoothing
- High pass filter
- Noise suppression
- Automatic gain control

### Microphone

- Chọn device
- Gain
- Test microphone
- Move HUD icon

### Black list

- Search player
- Mute / unmute
- Save JSON

### Debug

- Self loopback
- Fake remote player
- Packet simulator
- Audio oscilloscope
- Network graph

---

## 16. DIRECTX9 HOOK

Hook:

```cpp
IDirect3DDevice9::EndScene
IDirect3DDevice9::Reset
```

### Render order

```cpp
RenderSpeakerIcons();
RenderMicrophoneHud();
RenderSettingsPanel();
RenderDebugOverlay();
```

---

## 17. SELF-DEBUG MODE (QUAN TRỌNG NHẤT)

### Mục tiêu

Cho phép **một mình debug toàn bộ voice system mà không cần mở client thứ 2**.

---

### 17.1 Local Loopback

Hotkey: **F8**

Khi bật:

```cpp
if (m_debugLoopback)
{
    DecodeAndPlayLocal(encodedFrame);
}
```

Kết quả:

- nói vào mic,
- nghe lại ngay lập tức,
- kiểm tra được:
  - mic hoạt động,
  - encode Opus,
  - decode Opus,
  - playback,
  - gain,
  - filter,
  - noise suppression.

---

### 17.2 Fake Remote Player

Tạo player ảo:

```cpp
struct FakeRemotePlayer
{
    uint16_t id = 999;
    CVector  position;
    bool     talking;
};
```

Hotkey: **F9**

Hiển thị:

- `speaker.png` trên đầu NPC ảo,
- volume attenuation,
- pan trái / phải,
- distance calculation.

---

### 17.3 Voice Mirror Mode

Khi transmit:

```cpp
if (m_debugMirror)
{
    SimulateRemotePacket(localEncodedFrame);
}
```

Client sẽ tự nhận packet của chính nó như thể từ server gửi về.

Điều này cho phép test:

- jitter buffer,
- packet loss concealment,
- remote playback,
- speaker icon logic,
- talking timeout.

---

### 17.4 Packet Loss Simulator

UI slider:

```text
Packet loss: 0% → 50%
Jitter     : 0ms → 200ms
Latency    : 0ms → 300ms
```

Inject:

```cpp
if (RandomPercent() < m_packetLoss)
    return; // drop packet
```

---

### 17.5 Audio Oscilloscope

Hiển thị waveform realtime:

```cpp
ImGui::PlotLines("Mic RMS", rmsBuffer.data(), rmsBuffer.size());
```

Giúp debug:

- mic quá nhỏ,
- clipping,
- AGC hoạt động sai,
- noise gate quá mạnh.

---

### 17.6 Debug Overlay (F10)

Hiển thị:

```text
Voice State      : TRANSMITTING
Mic RMS          : 0.23
Mic Gain         : 1.15
Encoded Size     : 86 bytes
Bitrate          : 23.7 kbps
Jitter Buffer    : 2 packets
Packet Loss      : 1.2%
Remote Streams   : 1
Loopback         : ON
Fake Remote      : ON
Mirror Mode      : ON
RTT              : 18 ms
```

---

### 17.7 Auto Diagnostic

Lệnh:

```text
/ovdiag
```

Tự động kiểm tra:

- microphone device
- BASS_RecordInit
- Opus encoder
- UDP bind 7775
- texture loading
- DX9 hook
- ImGui initialization
- config permissions

Xuất file:

```text
ompvoice/debug/diagnostic_report.txt
```

Ví dụ:

```text
[PASS] Microphone detected
[PASS] BASS initialized
[PASS] Opus encoder created
[PASS] UDP 7775 bound
[PASS] speaker.png loaded
[PASS] DX9 EndScene hooked
[WARN] High-pass filter disabled
```

---

## 18. LOGGING SYSTEM

### File log

```text
ompvoice/logs/client.log
ompvoice/logs/server.log
ompvoice/logs/audio.log
ompvoice/logs/network.log
```

### Macro

```cpp
OV_LOG_INFO(...)
OV_LOG_WARN(...)
OV_LOG_ERROR(...)
OV_LOG_DEBUG(...)
```

Mỗi log phải có:

```text
[2026-08-06 10:37:12.421][Audio][INFO] Capture started
```

---

## 19. CRASH SAFETY

### Exception handler

```cpp
SetUnhandledExceptionFilter(OVUnhandledExceptionFilter);
```

Khi crash:

- dump stack trace,
- dump registers,
- dump loaded modules,
- lưu file `.dmp`.

---

## 20. ASSERT SYSTEM

```cpp
#define OV_ASSERT(expr)                                      \
    do                                                       \
    {                                                        \
        if (!(expr))                                         \
        {                                                    \
            OV_LOG_ERROR("ASSERT FAILED: %s", #expr);        \
            __debugbreak();                                  \
        }                                                    \
    } while (0)
```

Không dùng `assert()` mặc định.

---

## 21. THREADING RULES

### Audio thread

- capture
- encode
- decode
- jitter buffer

### Render thread

- DX9
- ImGui
- HUD

### Network thread

- UDP receive
- packet queue
- reconnect

**Không được gọi BASS playback từ network thread trực tiếp.**

---

## 22. MEMORY RULES

### Bắt buộc

- `std::unique_ptr`
- `std::shared_ptr`
- RAII wrappers
- không dùng `new/delete` trần

Ví dụ:

```cpp
using StreamHandle =
    std::unique_ptr<std::remove_pointer_t<HSTREAM>, StreamDeleter>;
```

---

## 23. ALT+TAB SAFETY

### Reset()

```cpp
ReleaseAllTextures();
ImGui_ImplDX9_InvalidateDeviceObjects();
```

### Restore

```cpp
ReloadTextures();
ImGui_ImplDX9_CreateDeviceObjects();
```

---

## 24. CONFIG JSON

```json
{
  "sound": {
    "enabled": true,
    "masterVolume": 32,
    "smoothing": true,
    "highPassFilter": true,
    "noiseSuppression": true,
    "automaticGainControl": true,
    "voiceActivation": false,
    "voiceThreshold": 0.15
  },

  "microphone": {
    "device": -1,
    "gain": 1.0,
    "muted": false
  },

  "speakerIcon": {
    "enabled": true,
    "scale": 1.0,
    "offsetX": 0,
    "offsetY": 0
  },

  "phone": {
    "leakRadius": 8.0
  },

  "debug": {
    "loopback": false,
    "mirrorMode": false,
    "fakeRemote": false,
    "showOverlay": true,
    "packetLoss": 0,
    "jitterMs": 0,
    "latencyMs": 0
  },

  "defaultVoiceDistance": 25.0,
  "globalDistanceValue": -1.0,
  "maxVoices": 8
}
```

---

## 25. HOTKEYS

| Phím | Chức năng |
|---|---|
| F11 | Settings |
| Z | Push-to-talk |
| Ctrl+M | Mute mic |
| F8 | Toggle loopback |
| F9 | Toggle fake remote |
| F10 | Debug overlay |

---

## 26. PERFORMANCE TARGET

| Thành phần | Mục tiêu |
|---|---|
| Capture | ≤ 5 ms |
| Encode | ≤ 2 ms |
| Decode | ≤ 1 ms |
| Playback buffer | 40 ms |
| Tổng latency | **< 80 ms** |
| CPU idle | **< 2%** |

---

## 27. FILE BẮT BUỘC PHẢI TẠO

```text
client/audio/
├── OVAudioEngine.cpp
├── OVMicCapture.cpp
├── OVOpusEncoder.cpp
├── OVOpusDecoder.cpp
├── OVPlaybackManager.cpp
├── OVJitterBuffer.cpp
├── OVNoiseGate.cpp
├── OVHighPassFilter.cpp
├── OVAutomaticGain.cpp

client/render/
├── OVHudIcon.cpp
├── OVSpeakerRenderer.cpp
├── OVDebugOverlay.cpp
├── OVDx9Renderer.cpp

client/debug/
├── OVLoopback.cpp
├── OVFakeRemote.cpp
├── OVPacketSimulator.cpp
├── OVDiagnostic.cpp

server/component/
├── OMPVoiceCore.cpp

server/debug/
├── ServerVoiceDebug.cpp
```

---

## 28. BUILD SYSTEM

### CMake

Phải build được:

- Visual Studio 2022
- Win32
- x64

### Dependencies

- ImGui
- MinHook
- BASS
- BASS FX
- Opus
- nlohmann/json

---

## 29. AI CODING RULES (RẤT QUAN TRỌNG)

### BẮT BUỘC

- Không dùng pseudo-code
- Không để `TODO`
- Không để `// implement later`
- Mọi hàm phải có implementation thật
- Mọi class phải có `.h + .cpp`
- Code phải **compile được ngay**
- Dùng **C++17**
- Warning level **/W4**
- Namespace `ov::`

### Nếu thiếu implementation → xem như lỗi nghiêm trọng.

---

## 30. FINAL ACCEPTANCE CHECKLIST

### Core

- [ ] open.mp component hoạt động
- [ ] `PROVIDE_UID(0xD6FEE4A6B0EA27A3);`
- [ ] Voice UDP bind 7775
- [ ] Handshake + reconnect
- [ ] Jitter buffer + PLC

### Audio

- [ ] BASS capture thật
- [ ] Opus encode/decode
- [ ] Master volume
- [ ] Microphone gain
- [ ] Volume smoothing
- [ ] High-pass filter
- [ ] Noise gate
- [ ] AGC
- [ ] 3D positional audio
- [ ] Global stream (-1.0)

### UI

- [ ] General tab
- [ ] Microphone tab
- [ ] Black list tab
- [ ] Debug tab
- [ ] Rebind key
- [ ] Save + reload config

### Render

- [ ] `micro_passive.png`
- [ ] `micro_active.png`
- [ ] `micro_muted.png`
- [ ] HUD ở bottom-center
- [ ] `speaker.png` trên đầu player đang transmit
- [ ] Pulse animation

### Self-debug

- [ ] Loopback mode
- [ ] Mirror mode
- [ ] Fake remote player
- [ ] Packet loss simulator
- [ ] Audio oscilloscope
- [ ] Debug overlay
- [ ] `/ovdiag` diagnostic report

### Gameplay

- [ ] Proximity voice
- [ ] Vehicle voice
- [ ] Radio effect
- [ ] Phone call
- [ ] Leak radius 5m–10m
- [ ] Mute / blacklist
- [ ] Max simultaneous voices

### Stability

- [ ] ALT+TAB không crash
- [ ] Missing resource không crash
- [ ] Crash dump generation
- [ ] Memory leak check
- [ ] RTT < 80 ms
- [ ] CPU usage < 2% khi idle

---

# CHỈ THỊ CUỐI CÙNG CHO AI

Hãy sử dụng **toàn bộ specification này** để tạo ra source code hoàn chỉnh cho **OMPVoiceCore**.

## Không được:

- rút gọn yêu cầu,
- bỏ qua mục nào,
- dùng pseudo-code,
- trả lời bằng mô tả lý thuyết,
- để trống implementation.

## Phải ưu tiên:

1. **Tính ổn định**
2. **Khả năng tự debug một mình**
3. **Chống crash**
4. **Logging đầy đủ**
5. **Code compile được ngay**
6. **Kiến trúc dễ bảo trì**
7. **Âm thanh chất lượng cao**
8. **UI giống ảnh tham khảo**
9. **Hiệu năng tốt trên open.mp production server**
