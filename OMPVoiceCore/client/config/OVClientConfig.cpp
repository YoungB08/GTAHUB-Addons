#include "OVClientConfig.h"

#include "shared/OVLogger.h"

namespace ov::client
{
OVClientConfig::OVClientConfig(std::filesystem::path path) : store_(std::move(path)) {}
bool OVClientConfig::Load()
{
    const bool result = store_.Load(values_, lastError_);
    if (!result) OV_LOG_WARN("Config", "Config load used defaults: %s", lastError_.c_str());
    return result;
}
bool OVClientConfig::Save()
{
    const bool result = store_.Save(values_, lastError_);
    if (!result) OV_LOG_ERROR("Config", "Config save failed: %s", lastError_.c_str());
    return result;
}
}
