#include "OMPVoiceCore.h"

#include "server/natives/OVPawnNatives.h"
#include "shared/OVConstants.h"
#include "shared/OVCrashSafety.h"
#include "shared/OVLogger.h"

#include <Server/Components/Pawn/pawn.hpp>
#include <Server/Components/Vehicles/vehicles.hpp>
#include <core.hpp>
#include <player.hpp>
#include <sdk.hpp>

#include <filesystem>
#include <string>

namespace
{
std::unique_ptr<OMPVoiceCore> g_component;
}

OMPVoiceCore::OMPVoiceCore()
    : voiceServer_(channels_), diagnostics_(std::filesystem::path("ompvoice") / "debug")
{
}

OMPVoiceCore::~OMPVoiceCore()
{
    voiceServer_.Stop();
    if (core_)
    {
        core_->getEventDispatcher().removeEventHandler(this);
        core_->getPlayers().getPlayerConnectDispatcher().removeEventHandler(this);
        core_->getPlayers().getPlayerTextDispatcher().removeEventHandler(this);
    }
    if (pawn_) pawn_->getEventDispatcher().removeEventHandler(this);
    ov::Logger::Instance().Shutdown();
    ov::CrashSafety::Uninstall();
}

StringView OMPVoiceCore::componentName() const { return "OMPVoiceCore"; }
SemanticVersion OMPVoiceCore::componentVersion() const { return SemanticVersion(1, 0, 0); }

void OMPVoiceCore::onLoad(ICore* core)
{
    core_ = core;
    ov::CrashSafety::Install(std::filesystem::path("ompvoice") / "debug");
    ov::Logger::Instance().Initialize(std::filesystem::path("ompvoice") / "logs", "server.log", "Server");
    OV_LOG_INFO("Component", "OMPVoiceCore loading; voice port is %u", ov::OMPVOICE_PORT);
    core_->getEventDispatcher().addEventHandler(this);
    core_->getPlayers().getPlayerConnectDispatcher().addEventHandler(this);
    core_->getPlayers().getPlayerTextDispatcher().addEventHandler(this);
}

void OMPVoiceCore::onInit(IComponentList* components)
{
    pawn_ = components->queryComponent<IPawnComponent>();
    if (pawn_)
    {
        pawn_->getEventDispatcher().addEventHandler(this);
        ov::server::SetNativeContext(&channels_);
    }
}

void OMPVoiceCore::onReady()
{
    udpBound_ = voiceServer_.Start();
    if (!udpBound_) OV_LOG_ERROR("Network", "Voice UDP could not bind to %u", ov::OMPVOICE_PORT);
}

void OMPVoiceCore::onFree(IComponent* component)
{
    if (component == pawn_) pawn_ = nullptr;
}

void OMPVoiceCore::provideConfiguration(ILogger&, IEarlyConfig& config, bool defaults)
{
    if (defaults) config.setInt("ompvoice.voice_port", ov::OMPVOICE_PORT);
}

void OMPVoiceCore::free()
{
    if (g_component.get() == this) g_component.release();
    delete this;
}
void OMPVoiceCore::reset() { voiceServer_.Stop(); udpBound_ = false; }

void OMPVoiceCore::onTick(Microseconds, TimePoint)
{
    if (!core_) return;
    for (IPlayer* player : core_->getPlayers().entries())
    {
        int vehicleId = -1;
        if (auto* vehicleData = queryExtension<IPlayerVehicleData>(player))
        {
            if (const auto* vehicle = vehicleData->getVehicle()) vehicleId = vehicle->getID();
        }
        voiceServer_.UpdatePlayer(player->getID(), ov::Vector3{player->getPosition().x, player->getPosition().y, player->getPosition().z}, vehicleId);
    }
}

void OMPVoiceCore::onPlayerConnect(IPlayer& player)
{
    const auto position = player.getPosition();
    voiceServer_.UpdatePlayer(player.getID(), ov::Vector3{position.x, position.y, position.z}, -1);
}

void OMPVoiceCore::onPlayerDisconnect(IPlayer& player, PeerDisconnectReason)
{
    voiceServer_.RemovePlayer(player.getID());
}

bool OMPVoiceCore::onPlayerCommandText(IPlayer&, StringView message)
{
    const std::string command(message);
    if (command != "/ovdiag") return true;
    std::string output;
    if (!diagnostics_.Run(udpBound_, output)) OV_LOG_ERROR("Debug", "Unable to write diagnostic report");
    return false;
}

void OMPVoiceCore::onAmxLoad(IPawnScript& script) { ov::server::RegisterNatives(script); }
void OMPVoiceCore::onAmxUnload(IPawnScript&) {}

COMPONENT_ENTRY_POINT()
{
    g_component = std::make_unique<OMPVoiceCore>();
    return g_component.get();
}
