#pragma once

#include <string_view>
#include <vector>

struct IPawnScript;
namespace ov::server { class OVChannelManager; }

namespace ov::server
{
void SetNativeContext(OVChannelManager* channels);
void RegisterNatives(IPawnScript& script);
[[nodiscard]] const std::vector<std::string_view>& RegisteredNativeNames();
[[nodiscard]] bool HasRegisteredNative(std::string_view name);
}
