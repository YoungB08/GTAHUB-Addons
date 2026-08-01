# Hướng Dẫn Biên Dịch, Tích Hợp open:mp Server Role Component & Mã Nguồn Mẫu Pawn Gamemode

Tài liệu này cung cấp hướng dẫn đầy đủ từ A-Z về cách:
1. **Biên dịch** open:mp C++ Server Component (`HUB-ServerComponent`).
2. **Đăng ký trước Resource ảnh (AddRoleResource)** tải qua **Thread riêng Async** tránh lag game.
3. **Tích hợp** component vào file `config.json` của open:mp server.
4. **Mã nguồn mẫu Pawn Gamemode** hoàn chỉnh minh họa toàn bộ các chức năng Set Role & Resource.
5. **Hệ thống Ghi Log chuyên dụng** lưu tại thư mục `Hub-Plugin/HUB-ServerRole.log`.

---

## 1. Cơ Chế Đăng Ký Trước Resource Ảnh (Pre-load Async Thread)

Để đảm bảo hiệu năng và **tuyệt đối không gây giật lag game** cho người chơi khi set Role:
1. Component cung cấp hàm `AddRoleResource(toPlayerid, resourceName, url, localPath)` để **đăng ký trước ảnh**.
2. Nếu tệp ảnh chưa có trên đĩa cứng và có URL internet, plugin sẽ **khởi chạy 1 Worker Thread riêng độc lập (`std::thread`)** để tải tệp ảnh ngầm dưới nền.
3. Khi gọi `SetPlayerRoleByName` hoặc `SetPlayerNametagData`, component chỉ cần gán và hiển thị thông tin ảnh đã có sẵn mà không tốn thời gian tải synchronous trong frame game.

---

## 2. Các Hàm Native Pawn Hỗ Trợ trong `hubcore_role.inc`

| Hàm Native | Mô Tả | Tham Số |
|---|---|---|
| `AddRoleResource` | Đăng ký trước Resource ảnh (Icon/Badge) tải qua Thread riêng không lag game | `(toPlayerid, const resourceName[], const url[], const localPath[])` |
| `HasRoleResource` | Kiểm tra xem Resource ảnh đã sẵn sàng hiển thị chưa | `(const resourceName[])` |
| `SetPlayerRoleByName` | Gán Role theo Tên JSON (vd: "ADMIN", "VIP", "MOD", "HELPER", "DEV") | `(toPlayerid, targetPlayerid, const roleName[], bool:useImage)` |
| `SetPresetRole` | Gán Preset Role tiêu chuẩn (1=ADMIN, 2=VIP, 3=MOD, 4=HELPER, 5=DEV) | `(toPlayerid, targetPlayerid, HUB_PresetRole:presetRole, bool:useImage)` |
| `SetPlayerNametagData` | Gán Nametag tùy chỉnh (chữ, màu ARGB, stroke, icon URL) | `(toPlayerid, targetPlayerid, const iconUrl[], const tagText[], color, bool:stroke)` |
| `ClearPlayerRole` | Xóa Role Badge của người chơi | `(toPlayerid, targetPlayerid)` |
| `GetPlayerRole` | Lấy tên Role hiện tại của người chơi | `(playerid, roleName[], max_len)` |
| `HasPlayerRole` | Kiểm tra người chơi có Role cụ thể hay không | `(playerid, const roleName[])` |

---

## 3. Ví Dụ Mẫu Pawn Gamemode Hoàn Chỉnh (`gamemodes/bare.pwn`)

```pawn
#include <openmp>
#include <hubcore_role>

main()
{
    print("\n==================================================");
    print("   GTAHUB - open:mp Role Gamemode Loaded");
    print("==================================================\n");
}

public OnGameModeInit()
{
    SetGameModeText("GTAHUB Role Server");
    AddPlayerClass(0, 1958.3783, 1343.1572, 15.3746, 269.1425, 0, 0, 0, 0, 0, 0);

    // 1. Pre-register / Pre-load toan bo Role Icons qua Thread rieng truoc khi player vao game (Khong gay lag game)
    AddRoleResource(-1, "ADMIN",  "https://cdn.example.com/icons/admin.png",  "HUB-Core/icons/admin.png");
    AddRoleResource(-1, "VIP",    "https://cdn.example.com/icons/vip.png",    "HUB-Core/icons/vip.png");
    AddRoleResource(-1, "MOD",    "https://cdn.example.com/icons/mod.png",    "HUB-Core/icons/mod.png");
    AddRoleResource(-1, "HELPER", "https://cdn.example.com/icons/helper.png", "HUB-Core/icons/helper.png");
    AddRoleResource(-1, "DEV",    "https://cdn.example.com/icons/dev.png",    "HUB-Core/icons/dev.png");
    return 1;
}

public OnPlayerConnect(playerid)
{
    new msg[128], name[MAX_PLAYER_NAME];
    GetPlayerName(playerid, name, sizeof(name));
    format(msg, sizeof(msg), "[SERVER] Chao mung %s (ID: %d) da tham gia server!", name, playerid);
    SendClientMessageToAll(0x00FF00FF, msg);
    return 1;
}

public OnPlayerDisconnect(playerid, reason)
{
    // Tu dong xoa Role data tren server khi nguoi choi thoat
    ClearPlayerRole(-1, playerid);
    return 1;
}

public OnPlayerSpawn(playerid)
{
    // Khi Set Role, chi can hien thi Icon da Pre-load (Khong can cho tai ngam gay giat)
    if (IsPlayerAdmin(playerid))
    {
        SetPlayerRoleByName(-1, playerid, "ADMIN", true);
        SendClientMessage(playerid, 0xFF0000FF, "[ROLE] Ban da duoc thiet lap Nametag Badge: [ADMIN]");
    }
    else
    {
        SetPresetRole(-1, playerid, ROLE_VIP, false);
        SendClientMessage(playerid, 0xFFFF00FF, "[ROLE] Ban da duoc thiet lap Nametag Badge: [VIP]");
    }
    return 1;
}

public OnPlayerText(playerid, text[])
{
    if (strcmp(text, "/setadmin", true) == 0)
    {
        SetPlayerAdminRole(playerid, true);
        SendClientMessage(playerid, 0x00FFFFAFA, "[ROLE] Set role ADMIN thanh cong!");
        return 0;
    }
    else if (strcmp(text, "/setvip", true) == 0)
    {
        SetPlayerVIPRole(playerid, false);
        SendClientMessage(playerid, 0x00FFFFAFA, "[ROLE] Set role VIP thanh cong!");
        return 0;
    }
    else if (strcmp(text, "/customrole", true) == 0)
    {
        SetPlayerNametagData(-1, playerid, "HUB-Core/icons/dev.png", "LEAD DEV", 0xFFAA00FF, true);
        SendClientMessage(playerid, 0x00FFFFAFA, "[ROLE] Set custom nametag [LEAD DEV] thanh cong!");
        return 0;
    }
    else if (strcmp(text, "/clearrole", true) == 0)
    {
        ClearPlayerRoleAll(playerid);
        SendClientMessage(playerid, 0xFF0000FF, "[ROLE] Da xoa role badge!");
        return 0;
    }
    return 1;
}
```

---

## 4. Hệ Thống Log Tự Động (`Hub-Plugin/HUB-ServerRole.log`)

Log ví dụ khi thực thi `AddRoleResource` qua Async Thread:
```text
[2026-08-01 16:58:10] [INFO] AddRoleResource: name='ADMIN', url='https://cdn.example.com/icons/admin.png', localPath='HUB-Core/icons/admin.png', isReady=0, toPlayer=-1
[2026-08-01 16:58:10] [INFO] [Async Thread] Started downloading resource 'ADMIN' from 'https://cdn.example.com/icons/admin.png'...
[2026-08-01 16:58:12] [INFO] [Async Thread] Successfully downloaded resource 'ADMIN' -> 'HUB-Core/icons/admin.png'.
[2026-08-01 16:58:45] [INFO] SetPlayerRoleByName: targetPlayer=0, roleName='ADMIN', imagePath='HUB-Core/icons/admin.png', useImage=1, toPlayer=-1
```
