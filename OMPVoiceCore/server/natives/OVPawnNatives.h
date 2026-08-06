#pragma once

struct IPawnScript;
namespace ov::server { class OVChannelManager; }

namespace ov::server
{
void SetNativeContext(OVChannelManager* channels);
void RegisterNatives(IPawnScript& script);
}
