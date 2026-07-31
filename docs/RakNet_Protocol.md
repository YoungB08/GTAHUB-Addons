# Giao Thức Mạng RakNet Server-Client (HUB-Core Protocol v4)

Tài liệu này mô tả chi tiết giao thức truyền nhận dữ liệu custom giữa **SA-MP Server** (Plugin C++ / Pawn Gamemode) và **Client** (Plugin `HUB-Core.asi`) thông qua RakNet custom packets (IDs `220`, `221`, `222`, `223`, `224`).

---

## 1. Danh Sách Packet IDs

| Packet ID | Tên Packet | Hướng | Mô Tả |
|:---:|---|:---:|---|
| **224** | `PACKET_SET_ROLE_BY_NAME` | Server → Client | **Khuyên dùng**: Server chỉ gửi Tên Role (vd: "ADMIN") + Cờ SVG (Client tự nạp JSON) |
| **220** | `PACKET_NAMETAG_DATA` | Server → Client | Gửi Custom Role (Text tùy chỉnh, màu tùy chỉnh, stroke) hoặc Image-Only Role (Chỉ Icon) |
| **221** | `PACKET_REQUEST_DATA` | Client → Server | Client gửi khi vừa đăng nhập/reconnect để yêu cầu Server gửi lại Role Data |
| **222** | `PACKET_SET_PRESET_ROLE` | Server → Client | Gán nhanh các Role cài sẵn (Admin, VIP, Moderator, Helper, Developer) |
| **223** | `PACKET_CLEAR_ROLE` | Server → Client | Xóa toàn bộ Role Badge và Icon của 1 người chơi |

---

## 2. Chi Tiết Cấu Trúc Packet

### 🔹 Packet 224: `PACKET_SET_ROLE_BY_NAME` (Server → Client) — KHUYÊN DÙNG
Server chỉ cần truyền **Tên Role** (vd: `"ADMIN"`, `"VIP"`, `"MOD"`) và **cờ useSvg** (`0`=Chỉ dùng Text Badge, `1`=Hiển thị Icon SVG từ JSON). Client tự động đối chiếu `HUB-Core/HUB-Roles.json` để lấy màu sắc, text, stroke và icon SVG.

| Offset | Kích thước | Kiểu dữ liệu | Mô tả |
|:---:|:---:|:---:|---|
| `0` | 1 byte | `BYTE` | Packet ID = `224` |
| `1` | 2 bytes | `WORD` | `targetPlayerID` (0 - 1003) |
| `3` | 1 byte | `BYTE` | `roleNameLen` (Độ dài chuỗi tên Role) |
| `4` | N bytes | `char[]` | `roleName` (Chuỗi tên Role, vd: "ADMIN", "VIP") |
| `4+N` | 1 byte | `BYTE` | `useSvg` (`0` = Text Badge Only, `1` = Nạp SVG Icon từ JSON) |

---

### 🔹 Packet 220: `PACKET_NAMETAG_DATA` (Server → Client)
Dùng cho **Custom Role** (nội dung chữ + màu nền + stroke) hoặc **Image-Only Role** (chỉ hiển thị Icon ảnh).

| Offset | Kích thước | Kiểu dữ liệu | Mô tả |
|:---:|:---:|:---:|---|
| `0` | 1 byte | `BYTE` | Packet ID = `220` |
| `1` | 2 bytes | `WORD` | `targetPlayerID` (0 - 1003) |
| `3` | 1 byte | `BYTE` | `iconUrlLen` (Độ dài chuỗi URL ảnh, `0` = không dùng icon) |
| `4` | N bytes | `char[]` | `iconUrl` (URL hình ảnh icon) |
| `4+N` | 1 byte | `BYTE` | `tagCount` (Số lượng role badge, `0` đến `2`) |
| *mỗi tag* | 1 byte | `BYTE` | `textLen` (Độ dài chuỗi badge) |
| *mỗi tag* | M bytes | `char[]` | `text` (Chuỗi chữ hiển thị, vd: "ADMIN", "VIP") |
| *mỗi tag* | 4 bytes | `DWORD` | `colorARGB` (Màu Direct3D `0xAARRGGBB`) |
| *mỗi tag* | 1 byte | `BYTE` | `stroke` (`0` = tắt viền đen, `1` = bật viền đen 8 hướng) |

---

### 🔹 Packet 222: `PACKET_SET_PRESET_ROLE` (Server → Client)
Gán nhanh Role tiêu chuẩn hệ thống.

| Offset | Kích thước | Kiểu dữ liệu | Mô tả |
|:---:|:---:|:---:|---|
| `0` | 1 byte | `BYTE` | Packet ID = `222` |
| `1` | 2 bytes | `WORD` | `targetPlayerID` (0 - 1003) |
| `3` | 1 byte | `BYTE` | `presetRoleType` (1=ADMIN, 2=VIP, 3=MOD, 4=HELPER, 5=DEV) |
| `4` | 1 byte | `BYTE` | `useSvg` (`0` = Text Badge Only, `1` = Nạp SVG Icon từ JSON) |

---

### 🔹 Packet 223: `PACKET_CLEAR_ROLE` (Server → Client)
Xóa dữ liệu nametag role của người chơi.

| Offset | Kích thước | Kiểu dữ liệu | Mô tả |
|:---:|:---:|:---:|---|
| `0` | 1 byte | `BYTE` | Packet ID = `223` |
| `1` | 2 bytes | `WORD` | `targetPlayerID` (0 - 1003) |

---

### 🔹 Packet 221: `PACKET_REQUEST_DATA` (Client → Server)
Gửi từ Client lên Server khi vừa vào game để yêu cầu Server đồng bộ lại role của tất cả người chơi đang online.

| Offset | Kích thước | Kiểu dữ liệu | Mô tả |
|:---:|:---:|:---:|---|
| `0` | 1 byte | `BYTE` | Packet ID = `221` |

---

## 3. Mã Nguồn Mẫu Phía Server (C++ Server Plugin)

```cpp
// Gửi Packet 224: Set Role bằng Tên (Ví dụ "ADMIN", useSvg = 1)
void Server_SetRoleByName(RakServerInterface* pRak, int toPlayer, int targetPlayer, const char* roleName, bool useSvg) {
    uint8_t buf[64];
    int pos = 0;

    buf[pos++] = 224; // Packet ID = 224
    *(uint16_t*)&buf[pos] = (uint16_t)targetPlayer; pos += 2;
    
    uint8_t len = (uint8_t)strlen(roleName);
    buf[pos++] = len;
    memcpy(&buf[pos], roleName, len); pos += len;
    
    buf[pos++] = useSvg ? 1 : 0;

    PlayerID pid = pRak->GetPlayerIDFromIndex(toPlayer);
    pRak->Send((char*)buf, pos, HIGH_PRIORITY, RELIABLE_ORDERED, 0, pid, false);
}
```

---

## 4. Mã Nguồn Mẫu Phía Pawn Gamemode (`hubcore.inc`)

```pawn
#if defined _hubcore_included
    #endinput
#endif
#define _hubcore_included

native SetPlayerRoleByName(toPlayerid, targetPlayerid, const roleName[], useSvg);

public OnPlayerSpawn(playerid)
{
    // Set Role ADMIN kèm SVG Icon
    if (IsPlayerAdmin(playerid)) {
        SetPlayerRoleByName(-1, playerid, "ADMIN", 1);
    }
    // Set Role VIP chỉ hiện Text Badge
    else if (GetPlayerVIPLevel(playerid) > 0) {
        SetPlayerRoleByName(-1, playerid, "VIP", 0);
    }
    return 1;
}
```

---

## 5. Cơ Chế Nạp Icon Cục Bộ (`HUB-Core/HUB-Roles.json`)

Nhằm tối ưu hiệu năng và tiết kiệm băng thông, Client `HUB-Core.asi` sẽ **ưu tiên kiểm tra tệp Icon trên đĩa cứng trước khi tải từ Internet**:

1. Khi khởi động, plugin tự động tạo tệp `HUB-Core/HUB-Roles.json` và thư mục `HUB-Core/icons/`.
2. Khi Server gửi Tên Role hoặc URL, Client sẽ tra cứu trong `HUB-Core/HUB-Roles.json` và nạp tệp `.svg` tương ứng.

### Cấu trúc tệp `HUB-Core/HUB-Roles.json`:

```json
{
  "comment": "HUB-Core Role & Icon Configuration File",
  "preset_roles": {
    "ADMIN": { "text": "ADMIN", "color": "0xFFB30000", "stroke": true, "svg": "HUB-Core/icons/admin.svg" },
    "VIP": { "text": "VIP", "color": "0xFFCC9900", "stroke": false, "svg": "HUB-Core/icons/vip.svg" },
    "MOD": { "text": "MOD", "color": "0xFF0088FF", "stroke": true, "svg": "HUB-Core/icons/mod.svg" },
    "HELPER": { "text": "HELPER", "color": "0xFF22AA22", "stroke": false, "svg": "HUB-Core/icons/helper.svg" },
    "DEV": { "text": "DEV", "color": "0xFFAA00FF", "stroke": true, "svg": "HUB-Core/icons/dev.svg" }
  },
  "icon_mappings": {
    "https://cdn.example.com/icons/admin.svg": "HUB-Core/icons/admin.svg"
  }
}
```
