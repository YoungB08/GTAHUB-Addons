# GTAHUB-Addons — HUB-Core (v1.0.4)

Client-side ASI plugin cho **SA-MP 0.3.DL** / **Open.mp** viết bằng C++/Win32 & Direct3D 9.  
Build ra `HUB-Core.asi`, tự động load qua ASI Loader khi GTA San Andreas khởi động.

---

## Tính Năng Chính

| Module | Mô tả |
|---|---|
| **Custom Nametag Engine** | Nametag 4 hàng: Role badges (Admin/VIP/Custom/Icon), Tên viền đen 8 hướng, HP/Armour Progress bar, Capsule ID & Ping |
| **Vehicle Limit Patch** | Patch bộ nhớ SAMP nâng giới hạn xe từ 611 lên 8000 xe |
| **RakNet Custom Protocol (v4)** | Nhận truyền tải Role Data (Packet 220, 221, 222, 223, 224 - Set Role By Name JSON) |
| **PNG Image Badge Engine** | Nạp và render tệp ảnh **`.png`** sắc nét, hỗ trợ 100% Alpha Channel và bo tròn góc mượt mà qua Direct3D 9 |
| **D3D9 Hooks & ENB Protection** | Hook VMT EndScene/Present tương thích 100% với ENB Series, Fastman92 7.6 và Open.mp |

---

## Cấu Trúc Dự Án

```
HUB-Core/
├── dllmain.cpp        Entry point, MainThread, PatchVehicleLimit, CChat spawn notify
├── framework.h        Version definitions, GetRefNetGame(), GetRefChat(), Logging
│
├── D3DHelper.h        Vẽ primitives bo tròn góc (DrawRoundedFilledRect, DrawProgressBar...)
├── W2S.h              World-to-Screen (GTA SA CalcScreenCoors 0x71DA00)
├── TextureCache.h     Async PNG/JPG/BMP texture loader cho Direct3D 9
├── PlayerData.h       Struct PlayerNametag, RoleTag + global g_Players[1040]
│
├── Nametag.h          Declarations nametag rendering
├── Nametag.cpp        Render nametag 4 hàng cho tất cả player & actors mỗi frame
│
├── D3DHook.h          Declarations D3D9 hook
├── D3DHook.cpp        VMT Multi-device map hook IDirect3DDevice9::EndScene & Present
│
├── Network.h          Giao thức mạng v4 (IDs 220, 221, 222, 223, 224)
├── Network.cpp        Hook RakClientInterface::Receive, parse packet 220, 222, 223, 224
├── RoleConfig.h       Quản lý tệp HUB-Core/HUB-Roles.json & tra cứu local path
│
└── docs/
    └── RakNet_Protocol.md   Tài liệu giao thức RakNet Server-Client chi tiết
```

---

## Vị Trí Lưu Tệp Trong Game GTA SA

```text
📁 GTA SAN ANDREAS/                     <-- Thư mục gốc Game
 ├── 📄 gta_sa.exe
 ├── 📄 HUB-Core.asi
 └── 📁 HUB-Core/                        <-- Thư mục HUB-Core
      ├── 📄 HUB-Roles.json              <-- File JSON cấu hình Role
      └── 📁 icons/                      <-- Thư mục chứa tệp .png
           ├── 🖼️ admin.png
           ├── 🖼️ vip.png
           ├── 🖼️ mod.png
           ├── 🖼️ helper.png
           └── 🖼️ dev.png
```

---

## Giao Thức Mạng RakNet (Server-Client Protocol v4)

Chi tiết đầy đủ xem tại [docs/RakNet_Protocol.md](file:///d:/GTAHUB-Addons/docs/RakNet_Protocol.md).

### 1. Danh sách Packet IDs

| Packet ID | Tên Packet | Hướng | Mục đích |
|:---:|---|:---:|---|
| **224** | `PACKET_SET_ROLE_BY_NAME` | Server → Client | **Khuyên dùng**: Server chỉ gửi Tên Role (vd: "ADMIN", "VIP") + Cờ PNG (Client tự nạp JSON) |
| **220** | `PACKET_NAMETAG_DATA` | Server → Client | Gửi Custom Role (Text, màu, stroke) hoặc Image-Only Role (Chỉ Icon ảnh) |
| **221** | `PACKET_REQUEST_DATA` | Client → Server | Client gửi yêu cầu Server resync toàn bộ Role Data khi kết nối |
| **222** | `PACKET_SET_PRESET_ROLE` | Server → Client | Gán nhanh Role tiêu chuẩn (`ADMIN`, `VIP`, `MOD`, `HELPER`, `DEV`) |
| **223** | `PACKET_CLEAR_ROLE` | Server → Client | Xóa toàn bộ Role Badge và Icon của người chơi |

---

### 2. Ví Dụ Pawn Gamemode (Packet 224)

```pawn
public OnPlayerSpawn(playerid)
{
    // 1. Set Role ADMIN kèm Icon PNG (useImage = 1)
    if (IsPlayerAdmin(playerid)) {
        SetPlayerRoleByName(-1, playerid, "ADMIN", 1); 
    }
    // 2. Set Role VIP chỉ hiển thị Text Badge, không hiện PNG (useImage = 0)
    else if (GetPlayerVIPLevel(playerid) > 0) {
        SetPlayerRoleByName(-1, playerid, "VIP", 0);
    }
    return 1;
}
```

---

## Hướng Dẫn Build Dự Án

### Yêu cầu môi trường:
- **Visual Studio 2019+** (Toolset v142 / v143)
- **DirectX SDK (June 2010)** — Thư viện `d3d9.h`, `d3dx9.h`, `d3dx9.lib`
- **Windows SDK 10.0**

### Các bước biên dịch:
1. Mở solution `HUB-Core.sln` bằng Visual Studio.
2. Chọn cấu hình **Release | Win32**.
3. Nhấn `Ctrl + Shift + B` (hoặc lệnh CLI MSBuild):
   ```cmd
   MSBuild.exe HUB-Core.vcxproj /p:Configuration=Release /p:Platform=Win32
   ```
4. Output file `HUB-Core.asi` sẽ được tự động copy vào thư mục `GTA SAN ANDREAS`.
