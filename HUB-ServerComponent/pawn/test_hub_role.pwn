/**
 * @file test_hub_role.pwn
 * @brief Gamemode test mẫu hoàn chỉnh kiểm thử 100% tính năng open:mp Server Role Component & hubcore_role.inc.
 */

#include <openmp>
#include <hubcore_role>

main()
{
    print("\n==================================================");
    print("   GTAHUB - open:mp Role System Test Gamemode");
    print("   Version: v4.0.0 (Multi-Slot, Timed, Rainbow)");
    print("==================================================\n");
}

public OnGameModeInit()
{
    SetGameModeText("GTAHUB Role Test v4.0.0");
    AddPlayerClass(0, 1958.3783, 1343.1572, 15.3746, 269.1425, 0, 0, 0, 0, 0, 0);

    // 1. Cấu hình khoảng cách nhìn thấy & tầm nhìn (Line-of-Sight)
    SetRoleGlobalConfig(25.0, true, true, 32.0, 32.0);

    // 2. Pre-register / Pre-load toàn bộ Role Icons qua Async Worker Thread từ lúc khởi tạo Server
    AddRoleResource("admin.png",  "https://cdn.example.com/icons/admin.png");
    AddRoleResource("vip.png",    "https://cdn.example.com/icons/vip.png");
    AddRoleResource("mod.png",    "https://cdn.example.com/icons/mod.png");
    AddRoleResource("helper.png", "https://cdn.example.com/icons/helper.png");
    AddRoleResource("dev.png",    "https://cdn.example.com/icons/dev.png");

    print("[GAMEMODE] Registered all role icon resources for async background downloading.");
    return 1;
}

public OnPlayerConnect(playerid)
{
    new msg[128], name[MAX_PLAYER_NAME];
    GetPlayerName(playerid, name, sizeof(name));
    format(msg, sizeof(msg), "[SERVER] Chao mung %s (ID: %d) da tham gia test Server Role!", name, playerid);
    SendClientMessageToAll(0x00FF00FF, msg);

    SendClientMessage(playerid, 0xFFFF00FF, "=== LENTH TEST ROLE COMMANDS ===");
    SendClientMessage(playerid, 0xFFFFFFFF, "/testpreset - Gán 3 Slot Preset Role");
    SendClientMessage(playerid, 0xFFFFFFFF, "/testcustom - Gán Custom Role Tag kèm màu sắc");
    SendClientMessage(playerid, 0xFFFFFFFF, "/testimage  - Gán Image Role Icon");
    SendClientMessage(playerid, 0xFFFFFFFF, "/testrainbow - Bật/Tắt hiệu ứng Rainbow cầu vồng");
    SendClientMessage(playerid, 0xFFFFFFFF, "/testcolor  - Đổi màu Nametag chính");
    SendClientMessage(playerid, 0xFFFFFFFF, "/testtimed  - Gán Role thời hạn 10 giây (Tự hết hạn)");
    SendClientMessage(playerid, 0xFFFFFFFF, "/testundercover - Bật/Tắt chế độ ẩn Badge (Undercover)");
    SendClientMessage(playerid, 0xFFFFFFFF, "/clearall   - Xóa toàn bộ Role slots");
    return 1;
}

public OnPlayerDisconnect(playerid, reason)
{
    ClearPlayerRoleAll(playerid);
    return 1;
}

public OnPlayerSpawn(playerid)
{
    // Mặc định gán Slot 0: Preset VIP khi Spawn
    SetPlayerVIPRole(playerid);
    SendClientMessage(playerid, 0x00FF00FF, "[ROLE] Ban da duoc thiet lap Slot 0 Preset VIP khi Spawn.");
    return 1;
}

public OnPlayerText(playerid, text[])
{
    if (strcmp(text, "/testpreset", true) == 0)
    {
        // Gán 3 Slot song song
        SetPlayerPresetRole(-1, playerid, ROLE_ADMIN, 0, 0);     // Slot 0: ADMIN
        SetPlayerPresetRole(-1, playerid, ROLE_VIP, 1, 0);       // Slot 1: VIP
        SetPlayerPresetRole(-1, playerid, ROLE_DEVELOPER, 2, 0); // Slot 2: DEV
        SendClientMessage(playerid, 0x00FFFFAFA, "[TEST] Da gan 3 Slot Preset (Slot 0: ADMIN, Slot 1: VIP, Slot 2: DEV).");
        return 0;
    }
    else if (strcmp(text, "/testcustom", true) == 0)
    {
        // Custom Style Role Tag
        SetPlayerCustomRole(-1, playerid, "SUPER ADMIN", 0xFFB30000, 0x88000000, true, 0, 0);
        SetPlayerCustomRole(-1, playerid, "PATRON VIP",  0xFFCC9900, 0x88000000, false, 1, 0);
        SendClientMessage(playerid, 0x00FFFFAFA, "[TEST] Da gan Custom Role Style vao Slot 0 & Slot 1.");
        return 0;
    }
    else if (strcmp(text, "/testimage", true) == 0)
    {
        // Image Role Tag
        SetPlayerImageRole(-1, playerid, "admin.png", "LEAD ADMIN", 0xFFB30000, 0, 0);
        SendClientMessage(playerid, 0x00FFFFAFA, "[TEST] Da gan Image Role Tag (admin.png) vao Slot 0.");
        return 0;
    }
    else if (strcmp(text, "/testrainbow", true) == 0)
    {
        new bool:active = IsPlayerRainbowActive(playerid, -1);
        SetPlayerRainbowRole(playerid, !active, 300, -1); // Quick Rainbow Nametag chính
        SetPlayerRainbowRole(playerid, !active, 300, 0);  // Rainbow Slot 0
        if (!active)
            SendClientMessage(playerid, 0x00FF00FF, "[TEST] Da BAT hieu ung Rainbow Cau Vong chớp màu!");
        else
            SendClientMessage(playerid, 0xFF0000FF, "[TEST] Da TAT hieu ung Rainbow Cau Vong.");
        return 0;
    }
    else if (strcmp(text, "/testcolor", true) == 0)
    {
        SetPlayerNametagColor(-1, playerid, 0xFF00FFCC); // Mau Xanh Ngoc ARGB
        SendClientMessage(playerid, 0x00FFCCFF, "[TEST] Da doi mau Nametag chinh sang mau Xanh Ngoc (0xFF00FFCC)!");
        return 0;
    }
    else if (strcmp(text, "/testtimed", true) == 0)
    {
        SetPlayerCustomRole(-1, playerid, "TIMED ROLE (10s)", 0xFFAA00FF, 0x88000000, true, 1, 10);
        SendClientMessage(playerid, 0xFFAA00FF, "[TEST] Da gan Role thoi han 10 giay vao Slot 1! Role se tu xoa va nhay callback.");
        return 0;
    }
    else if (strcmp(text, "/testundercover", true) == 0)
    {
        new bool:visible = IsPlayerRoleVisible(playerid);
        SetPlayerRoleVisible(playerid, !visible, -1);
        if (visible)
            SendClientMessage(playerid, 0xFFFF00FF, "[TEST] Da AN Badge (Che do Undercover / Admin Off Duty).");
        else
            SendClientMessage(playerid, 0x00FF00FF, "[TEST] Da HIEN Badge (Che do Admin On Duty).");
        return 0;
    }
    else if (strcmp(text, "/clearall", true) == 0)
    {
        ClearPlayerRoleAll(playerid);
        SendClientMessage(playerid, 0xFF0000FF, "[TEST] Da xoa toan bo Role o tat ca cac Slot.");
        return 0;
    }
    return 1;
}

public OnRoleResourceLoaded(playerid, const resourceKey[], bool:success)
{
    new msg[128];
    format(msg, sizeof(msg), "[RESOURCE EVENT] Resource '%s' download status: %s", resourceKey, success ? "SUCCESS" : "FAILED");
    print(msg);
    return 1;
}

public OnPlayerRoleExpired(playerid, slotID)
{
    new msg[128];
    format(msg, sizeof(msg), "[ROLE EXPIRED EVENT] Player ID %d: Role at Slot %d has EXPIRED!", playerid, slotID);
    SendClientMessage(playerid, 0xFF0000FF, msg);
    print(msg);
    return 1;
}
