# GTAHUB-Addons — HUB-Core

Client-side ASI plugin cho **SA-MP 0.3.DL** viết bằng C++/Win32.  
Build ra `rcgame.asi`, load tự động qjua ASI Loader khi GTA SA khởi động.

---

## Tính năng

| Module | Mô tả |
|---|---|
| **Custom Nametag** | Nametag 4 hàng: badge, tên stroke, HP/armour bar, ID/ping capsule |
| **Vehicle Limit Patch** | Nâng giới hạn xe SAMP từ 611 → 8000 |
| **RakNet Network** | Nhận role data (Admin/VIP/Icon) từ server qua custom packet |

---

## Cấu trúc file

```
HUB-Core/
├── dllmain.cpp        Entry point, PatchVehicleLimit, khởi động các module
│
├── D3DHelper.h        Draw primitives (FilledRect, BorderRect, ProgressBar, TextStroke)
├── W2S.h              World-to-Screen (GTA SA CalcScreenCoors 0x71DA00)
├── TextureCache.h     Async URL texture loader (download bg thread, create render thread)
├── PlayerData.h       Struct PlayerRoleData + global g_Players[1004]
│
├── Nametag.h          Declarations
├── Nametag.cpp        Render nametag 4 hàng cho tất cả player mỗi frame
│
├── D3DHook.h          Declarations
├── D3DHook.cpp        VMT hook IDirect3DDevice9::EndScene (index 42)
│
├── Network.h          Declarations + protocol documentation
└── Network.cpp        Hook RakClientInterface::Receive, parse packet 220/221
```

---

## Kiến trúc nametag

```
         centerX (từ WorldToScreen)
              │
   ┌──────────┼──────────────────────┐
   │  ADMIN   │  [ICON]  │    VIP   │  ← Hàng 1: Badges (chỉ hiện nếu có role)
   └──────────┴──────────┴──────────┘
              PlayerName              ← Hàng 2: Cyan #33CCFF + stroke đen 8 hướng
   ┌──────────────┐ ┌──────────────┐
   │ ████░ HP    │ │ ████░ Armour │  ← Hàng 3: Progress bars (đỏ | bạc)
   └──────────────┘ └──────────────┘
   ┌──────────────────────────────┐
   │      ID: 42   |   12ms      │  ← Hàng 4: Capsule info
   └──────────────────────────────┘
```

Vị trí `centerX / topY` được tính từ `GetBonePosition(8)` (đầu player) + offset 0.28f,  
rồi chiếu qua `CalcScreenCoors` (`gta_sa.exe+0x71DA00`).

---

## Giao thức mạng (Network Protocol)

### Packet IDs

| ID | Hướng | Mô tả |
|---|---|---|
| `220` | Server → Client | Toàn bộ nametag data (icon + tags) của 1 player |
| `221` | Client → Server | Client yêu cầu server resync data |

### Cấu trúc Packet 220 — `PACKET_NAMETAG_DATA`

| Offset | Size | Type | Mô tả |
|---|---|---|---|
| 0 | 1 | `BYTE` | Packet ID = `220` |
| 1 | 2 | `WORD` | targetPlayerID (0–1003) |
| 3 | 1 | `BYTE` | iconUrlLen (0 = không có icon) |
| 4 | N | `char[]` | iconUrl (không null-terminated) |
| 4+N | 1 | `BYTE` | tagCount (0–2, tối đa 2 tag) |
| per tag | 1 | `BYTE` | textLen |
| per tag | M | `char[]` | text badge (không null-terminated) |
| per tag | 4 | `DWORD` | colorARGB (D3DCOLOR `0xAARRGGBB`) |
| per tag | 1 | `BYTE` | stroke (`0`=tắt, `1`=bật) |

> **Pawn color**: Pawn dùng `0xRRGGBBAA`, plugin C++ phải đổi sang `0xAARRGGBB` trước khi gửi.

### Cấu trúc Packet 221 — `PACKET_REQUEST_ROLES`

| Offset | Size | Type | Mô tả |
|---|---|---|---|
| 0 | 1 | `BYTE` | Packet ID = `221` |

### Ví dụ server plugin (C++)

```cpp
// Gửi role của 1 player cho 1 client
void SendRolePacket(RakServerInterface* pRak, int toPlayer, int targetPlayer,
                    bool isAdmin, bool isVIP, const char* iconUrl)
{
    uint8_t buf[260];
    int pos = 0;
    buf[pos++] = 220;
    *(uint16_t*)&buf[pos] = (uint16_t)targetPlayer; pos += 2;
    buf[pos++] = (isAdmin ? 1 : 0) | (isVIP ? 2 : 0);
    uint8_t urlLen = (uint8_t)min(strlen(iconUrl), 255u);
    buf[pos++] = urlLen;
    memcpy(&buf[pos], iconUrl, urlLen); pos += urlLen;

    PlayerID pid = pRak->GetPlayerIDFromIndex(toPlayer);
    pRak->Send((char*)buf, pos, HIGH_PRIORITY, RELIABLE_ORDERED, 0, pid, false);
}
```

### Ví dụ Pawn (gamemode)

```pawn
// Gửi role cho tất cả player khi 1 player mới connect
public OnPlayerConnect(playerid) {
    for (new i = 0; i < MAX_PLAYERS; i++) {
        if (IsPlayerConnected(i)) {
            // Gọi native của server plugin C++
            SendPlayerRoleData(playerid, i,
                IsAdmin(i), IsVIP(i), GetIconUrl(i));
        }
    }
}
```

---

## Build

**Yêu cầu:**
- Visual Studio 2019+ (toolset v142 hoặc v143)
- DirectX SDK (June 2010) — cung cấp `d3d9.h`, `d3dx9.h`, `d3dx9.lib`
- Windows SDK 10.0

**Cấu hình:**
| Configuration | Output | Ghi chú |
|---|---|---|
| Debug\|Win32 | `.asi` | Thư mục build mặc định |
| Release\|Win32 | `D:\RCRP Game\rcgame.asi` | Auto-deploy vào GTA SA folder |

**Build:**
```
Mở HUB-Core.sln → chọn Release|Win32 → Ctrl+Shift+B
```

---

## Kỹ thuật sử dụng

| Kỹ thuật | Mô tả |
|---|---|
| VMT Hook (EndScene) | Patch `IDirect3DDevice9` vtable[42] để render mỗi frame |
| VMT Hook (Receive) | Patch `RakClientInterface` vtable[5] để intercept packet |
| D3D9 StateBlock | Capture/restore toàn bộ render state — không phá SAMP render |
| `GetBonePosition(8)` | Lấy tọa độ đầu player (bone HEAD) trong world space |
| `CalcScreenCoors` | GTA SA internal W2S tại `gta_sa.exe+0x71DA00` |
| Async texture load | Download URL trên bg thread, tạo texture trên render thread |

---

## Dependencies

| Lib | Dùng cho |
|---|---|
| `sampapi.lib` | Wrapper SA-MP API (RefNetGame, RefPlayerTags, ...) |
| `d3d9.lib` | Direct3D 9 |
| `d3dx9.lib` | D3DXFont, D3DXSprite, D3DXCreateTexture |
| `urlmon.lib` | URLDownloadToCacheFileA |
| `Ws2_32.lib` | Winsock (RakNet dependency) |

