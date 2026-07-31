/**
 * @file RoleConfig.h
 * @brief Đọc và quản lý tệp cấu hình HUB-Roles.json.
 *
 * Cho phép Server set role chỉ bằng tên định danh (vd: "ADMIN", "VIP", "MOD").
 * Client tự đọc text, color, stroke và tệp .png / .svg tương ứng từ HUB-Roles.json.
 */
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d9.h>
#include <string>
#include <fstream>
#include <sstream>

namespace RoleConfig {

inline const char* kConfigFile = "HUB-Core\\HUB-Roles.json";
inline const char* kIconsDir   = "HUB-Core\\icons";

struct RolePresetConfig {
    std::string text;
    D3DCOLOR    color     = 0;
    bool        stroke    = false;
    std::string imagePath; // Tệp ảnh PNG / JPG / SVG
    bool        hasConfig = false;
};

/// Kiểm tra tệp có tồn tại trên đĩa cứng không
inline bool FileExists(const std::string& path) {
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

/// Khởi tạo tệp JSON mặc định và thư mục lưu trữ nếu chưa có
inline void InitDefaults() {
    CreateDirectoryA("HUB-Core", NULL);
    CreateDirectoryA(kIconsDir, NULL);

    if (!FileExists(kConfigFile)) {
        std::ofstream json(kConfigFile);
        if (json.is_open()) {
            json << "{\n";
            json << "  \"comment\": \"HUB-Core Role & Icon Configuration File\",\n";
            json << "  \"draw_distance\": 40.0,\n";
            json << "  \"preset_roles\": {\n";
            json << "    \"ADMIN\": { \"text\": \"ADMIN\", \"color\": \"0xFFB30000\", \"stroke\": true, \"png\": \"HUB-Core/icons/admin.png\" },\n";
            json << "    \"VIP\": { \"text\": \"VIP\", \"color\": \"0xFFCC9900\", \"stroke\": false, \"png\": \"HUB-Core/icons/vip.png\" },\n";
            json << "    \"MOD\": { \"text\": \"MOD\", \"color\": \"0xFF0088FF\", \"stroke\": true, \"png\": \"HUB-Core/icons/mod.png\" },\n";
            json << "    \"HELPER\": { \"text\": \"HELPER\", \"color\": \"0xFF22AA22\", \"stroke\": false, \"png\": \"HUB-Core/icons/helper.png\" },\n";
            json << "    \"DEV\": { \"text\": \"DEV\", \"color\": \"0xFFAA00FF\", \"stroke\": true, \"png\": \"HUB-Core/icons/dev.png\" }\n";
            json << "  },\n";
            json << "  \"icon_mappings\": {\n";
            json << "    \"https://cdn.example.com/icons/admin.png\": \"HUB-Core/icons/admin.png\"\n";
            json << "  }\n";
            json << "}\n";
            json.close();
        }
    }
}

/**
 * @brief Đọc khoảng cách hiển thị Nametag tối đa từ HUB-Roles.json (Mặc định: 40m).
 */
inline float GetDrawDistance() {
    InitDefaults();
    std::ifstream file(kConfigFile);
    if (!file.is_open()) return 40.0f;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    size_t pos = content.find("\"draw_distance\"");
    if (pos != std::string::npos) {
        size_t colon = content.find(':', pos);
        if (colon != std::string::npos) {
            float dist = static_cast<float>(atof(content.c_str() + colon + 1));
            if (dist > 1.0f) return dist;
        }
    }
    return 40.0f;
}

/**
 * @brief Tra cứu cấu hình Role theo tên định danh từ HUB-Roles.json.
 */
inline RolePresetConfig GetPresetRoleConfig(const std::string& roleName) {
    RolePresetConfig cfg;
    if (roleName.empty()) return cfg;

    InitDefaults();

    std::ifstream file(kConfigFile);
    if (!file.is_open()) return cfg;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Tìm key "roleName" trong JSON
    std::string keyPattern = "\"" + roleName + "\"";
    size_t pos = content.find(keyPattern);
    if (pos == std::string::npos) return cfg;

    size_t blockStart = content.find('{', pos);
    size_t blockEnd   = content.find('}', pos);
    if (blockStart == std::string::npos || blockEnd == std::string::npos || blockEnd <= blockStart) return cfg;

    std::string block = content.substr(blockStart, blockEnd - blockStart + 1);

    // 1. Text
    size_t tPos = block.find("\"text\"");
    if (tPos != std::string::npos) {
        size_t v1 = block.find('"', block.find(':', tPos));
        size_t v2 = block.find('"', v1 + 1);
        if (v1 != std::string::npos && v2 != std::string::npos) {
            cfg.text = block.substr(v1 + 1, v2 - v1 - 1);
        }
    }
    if (cfg.text.empty()) cfg.text = roleName;

    // 2. Color
    size_t cPos = block.find("\"color\"");
    if (cPos != std::string::npos) {
        size_t v1 = block.find('"', block.find(':', cPos));
        size_t v2 = block.find('"', v1 + 1);
        if (v1 != std::string::npos && v2 != std::string::npos) {
            std::string cStr = block.substr(v1 + 1, v2 - v1 - 1);
            uint32_t val = (uint32_t)strtoul(cStr.c_str(), NULL, 16);
            cfg.color = static_cast<D3DCOLOR>(val);
        }
    }

    // 3. Stroke
    size_t sPos = block.find("\"stroke\"");
    if (sPos != std::string::npos) {
        if (block.find("true", sPos) < block.find(',', sPos) && block.find("true", sPos) < block.find('}', sPos)) {
            cfg.stroke = true;
        }
    }

    // 4. Image Path (hỗ trợ "png", "image", hoặc "svg")
    size_t imgPos = block.find("\"png\"");
    if (imgPos == std::string::npos) imgPos = block.find("\"image\"");
    if (imgPos == std::string::npos) imgPos = block.find("\"svg\"");

    if (imgPos != std::string::npos) {
        size_t v1 = block.find('"', block.find(':', imgPos));
        size_t v2 = block.find('"', v1 + 1);
        if (v1 != std::string::npos && v2 != std::string::npos) {
            cfg.imagePath = block.substr(v1 + 1, v2 - v1 - 1);
        }
    }

    if (cfg.imagePath.empty()) {
        std::string pngCandidate = "HUB-Core/icons/" + roleName + ".png";
        std::string svgCandidate = "HUB-Core/icons/" + roleName + ".svg";
        if (FileExists(pngCandidate)) {
            cfg.imagePath = pngCandidate;
        } else if (FileExists(svgCandidate)) {
            cfg.imagePath = svgCandidate;
        } else {
            cfg.imagePath = pngCandidate;
        }
    }

    cfg.hasConfig = true;
    return cfg;
}

/**
 * @brief Tìm đường dẫn tệp cục bộ tương ứng với URL/Key từ HUB-Roles.json.
 */
inline std::string ResolveLocalPath(const std::string& urlOrKey) {
    if (urlOrKey.empty()) return "";

    InitDefaults();

    if (FileExists(urlOrKey)) {
        return urlOrKey;
    }

    std::ifstream file(kConfigFile);
    if (!file.is_open()) return "";

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    std::string searchKey = "\"" + urlOrKey + "\"";
    size_t keyPos = content.find(searchKey);
    if (keyPos != std::string::npos) {
        size_t colonPos = content.find(':', keyPos + searchKey.length());
        if (colonPos != std::string::npos) {
            size_t valStart = content.find('"', colonPos + 1);
            if (valStart != std::string::npos) {
                size_t valEnd = content.find('"', valStart + 1);
                if (valEnd != std::string::npos) {
                    std::string localPath = content.substr(valStart + 1, valEnd - valStart - 1);
                    if (FileExists(localPath)) {
                        return localPath;
                    }
                }
            }
        }
    }

    return "";
}

} // namespace RoleConfig
