/**
 * @file hubcore_role.inc
 * @brief Pawn Include File cho open:mp Server Role Component
 * @note Lưu trữ Icon tại "HUB-Core/icons/", hỗ trợ 10 Multi-Slots (Auto Line Wrap), Timed Roles, Rainbow Effects & Custom Nametag Color.
 */

#if defined _hubcore_role_included
    #endinput
#endif
#define _hubcore_role_included

#define MAX_ROLE_SLOTS (10) // 0: Slot chính, 1-9: Slot phụ (Tối đa 10 Role hiển thị cùng lúc, tự phân dòng)

// Enum Preset Role
enum HUB_PresetRole {
    ROLE_NONE      = 0,
    ROLE_ADMIN     = 1,
    ROLE_VIP       = 2,
    ROLE_MODERATOR = 3,
    ROLE_HELPER    = 4,
    ROLE_DEVELOPER = 5
};

/* ========================================================================== */
/* 1. GLOBAL CONFIG & ASYNC RESOURCE (Gọi tại OnGameModeInit)                */
/* ========================================================================== */

/**
 * @brief Đăng ký Resource Icon (URL) toàn Server.
 * Client tự chạy Thread ngầm tải về và lưu trực tiếp vào "HUB-Core/icons/".
 * @param resourceKey - Tên file lưu trữ (vd: "admin.png", "vip.png")
 * @param url         - Đường dẫn tải ảnh PNG
 */
native bool:AddRoleResource(const resourceKey[], const url[]);

/**
 * @brief Cấu hình quy tắc hiển thị chung cho toàn Server.
 * @param distance         - Khoảng cách nhìn thấy Badge (Mặc định: 20.0m)
 * @param enableLOS        - Kiểm tra vật cản / Xuyên tường (Line-of-Sight)
 * @param autoHideInVeh    - Tự động ẩn Badge khi ngồi trong xe
 * @param iconWidth/Height - Kích thước Icon ảnh mặc định (Width x Height)
 */
native bool:SetRoleGlobalConfig(Float:distance = 20.0, bool:enableLOS = true, bool:autoHideInVeh = true, Float:iconWidth = 32.0, Float:iconHeight = 32.0);


/* ========================================================================== */
/* 2. CORE SET ROLE NATIVES (10 Slot Role - Tự Phân Dòng Tự Động)             */
/* ========================================================================== */

/**
 * @brief 1. Gán Preset Role có sẵn
 * @param durationSeconds - 0: Vĩnh viễn | >0: Tự động gỡ Role sau N giây
 */
native bool:SetPlayerPresetRole(toPlayerid, targetPlayerid, HUB_PresetRole:presetRole, slotID = 0, durationSeconds = 0);

/**
 * @brief 2. Gán Custom Style Role (Text + Màu + Nền + Viền)
 */
native bool:SetPlayerCustomRole(toPlayerid, targetPlayerid, const tagText[], color, bgColor = 0, bool:stroke = true, slotID = 0, durationSeconds = 0);

/**
 * @brief 3. Gán Image Role (Lấy icon từ "HUB-Core/icons/<resourceKey>")
 */
native bool:SetPlayerImageRole(toPlayerid, targetPlayerid, const resourceKey[], const tagText[] = "", color = -1, slotID = 0, durationSeconds = 0);


/* ========================================================================== */
/* 3. RAINBOW EFFECTS (Hiệu ứng chớp màu / đổi màu cầu vồng)                 */
/* ========================================================================== */

/**
 * @brief Bật/Tắt hiệu ứng Rainbow cho Nametag Text/Color của Player.
 * @param targetPlayerid - ID người chơi
 * @param toggle         - true: Bật hiệu ứng, false: Tắt
 * @param speed_ms       - Tốc độ chuyển màu (miligiây, mặc định 500ms per step)
 * @param slotID         - -1: Đổi màu Nametag chính, 0..9: Đổi màu Role Tag tại Slot chỉ định
 */
native bool:SetPlayerRainbowRole(targetPlayerid, bool:toggle, speed_ms = 500, slotID = -1);

/**
 * @brief Kiểm tra xem Player/Slot có đang bật hiệu ứng Rainbow hay không.
 */
native bool:IsPlayerRainbowActive(playerid, slotID = -1);


/* ========================================================================== */
/* 4. NAMETAG COLOR & UTILITIES                                               */
/* ========================================================================== */

/**
 * @brief Đặt màu sắc riêng cho Nametag Component (Không trùng với SetPlayerColor của SA-MP).
 * @param targetPlayerid - ID người chơi được đổi màu Nametag
 * @param color          - Màu sắc dạng ARGB (vd: 0xFF00FF00)
 * @param toPlayerid     - ID người nhìn thấy màu này (-1 = Tất cả)
 */
native bool:SetPlayerNametagColor(toPlayerid, targetPlayerid, color);

/**
 * @brief Lấy màu sắc Nametag hiện tại của người chơi.
 */
native bool:GetPlayerNametagColor(targetPlayerid, &color);

// Xóa Role của người chơi (slotID = -1 để xóa sạch tất cả Slot 0..9)
native bool:ClearPlayerRole(toPlayerid, targetPlayerid, slotID = -1);

// Bật/Tắt hiển thị Badge của targetPlayerid (Admin Duty / Undercover)
native bool:SetPlayerRoleVisible(targetPlayerid, bool:toggle, toPlayerid = -1);


/* ========================================================================== */
/* 5. GETTERS & CHECKERS (Hàm truy vấn trạng thái)                             */
/* ========================================================================== */

/**
 * @brief Kiểm tra xem Slot (0-9) của người chơi có đang có Role hay không.
 */
native bool:HasPlayerRole(playerid, slotID = 0);

/**
 * @brief Kiểm tra xem người chơi có đang bật hiển thị Role không.
 */
native bool:IsPlayerRoleVisible(playerid);

/**
 * @brief Kiểm tra xem một Resource Key đã tải xong về HUB-Core/icons/ chưa.
 */
native bool:IsRoleResourceLoaded(const resourceKey[]);


/* ========================================================================== */
/* 6. CALLBACKS                                                               */
/* ========================================================================== */

// Gọi khi Client tải xong 1 Resource từ Thread ngầm và lưu vào HUB-Core/icons/
forward OnRoleResourceLoaded(playerid, const resourceKey[], bool:success);

// Gọi khi Role ở slotID hết thời hạn
forward OnPlayerRoleExpired(playerid, slotID);


/* ========================================================================== */
/* 7. HELPER MACROS                                                           */
/* ========================================================================== */

#define SetPlayerAdminRole(%0)      SetPlayerPresetRole(-1, (%0), ROLE_ADMIN)
#define SetPlayerVIPRole(%0)        SetPlayerPresetRole(-1, (%0), ROLE_VIP)
#define ClearPlayerRoleAll(%0)      ClearPlayerRole(-1, (%0), -1)
#define HidePlayerRole(%0)          SetPlayerRoleVisible((%0), false, -1)
#define ShowPlayerRole(%0)          SetPlayerRoleVisible((%0), true, -1)

// Quick Rainbow Macros
#define SetNametagRainbow(%0,%1)    SetPlayerRainbowRole((%0), (%1), 500, -1)
#define SetRoleRainbow(%0,%1,%2)    SetPlayerRainbowRole((%0), (%1), 500, (%2))
