/**
 * @file W2S.h
 * @brief World-to-Screen conversion dùng GTA SA CalcScreenCoors (header-only).
 *
 * CalcScreenCoors là hàm nội bộ của gta_sa.exe tại offset 0x71DA00.
 * Nó project tọa độ 3D world sang pixel màn hình 2D.
 *
 * Lưu ý:
 *  - Hàm trả false nếu điểm nằm sau camera (w hoặc h < 0).
 *  - Tọa độ trả ra chưa kiểm tra giới hạn viewport — caller tự kiểm.
 */
#pragma once
#include "sampapi/CVector.h"

namespace W2S {

/// Signature của CalcScreenCoors trong gta_sa.exe (0x71DA00: 6 tham số)
using FnCalcScreenCoors = bool(__cdecl*)(
    const sampapi::CVector* pWorld,
    sampapi::CVector*       pScreen,
    float*                  pW,
    float*                  pH,
    bool                    bCheckClip,
    bool                    bArg5);

/// Địa chỉ cố định trong gta_sa.exe (không đổi theo version SAMP)
constexpr DWORD kCalcScreenCoorsAddr = 0x71DA00;

/**
 * @brief Chuyển tọa độ 3D world sang 2D screen.
 * @param worldPos  Vị trí trong thế giới game
 * @param outX      [out] Tọa độ X trên màn hình
 * @param outY      [out] Tọa độ Y trên màn hình
 * @return true nếu điểm hiển thị trước camera, false nếu khuất sau lưng.
 */
inline bool ToScreen(sampapi::CVector worldPos, float& outX, float& outY) {
    static auto fn = reinterpret_cast<FnCalcScreenCoors>(kCalcScreenCoorsAddr);
    sampapi::CVector screen;
    float w = 0.f, h = 0.f;
    if (!fn(&worldPos, &screen, &w, &h, false, false)) return false;
    if (w <= 0.0f) return false; // Điểm nằm đằng sau lưng camera
    outX = screen.x;
    outY = screen.y;
    return true;
}

} // namespace W2S
