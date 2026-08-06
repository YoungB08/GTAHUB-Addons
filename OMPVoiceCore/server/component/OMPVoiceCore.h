#pragma once

#include "server/channels/OVChannelManager.h"
#include "server/debug/ServerVoiceDebug.h"
#include "server/voice/OVVoiceServer.h"

#include <Server/Components/Pawn/pawn.hpp>
#include <core.hpp>
#include <player.hpp>
#include <sdk.hpp>

#include <memory>

class OMPVoiceCore final : public IComponent, public CoreEventHandler, public PlayerConnectEventHandler, public PlayerTextEventHandler, public PawnEventHandler
{
public:
    OMPVoiceCore();
    ~OMPVoiceCore() override;

    PROVIDE_UID(0xD6FEE4A6B0EA27A3ULL);
    StringView componentName() const override;
    SemanticVersion componentVersion() const override;
    void onLoad(ICore* core) override;
    void onInit(IComponentList* components) override;
    void onReady() override;
    void onFree(IComponent* component) override;
    void provideConfiguration(ILogger& logger, IEarlyConfig& config, bool defaults) override;
    void free() override;
    void reset() override;
    void onTick(Microseconds elapsed, TimePoint now) override;
    void onPlayerConnect(IPlayer& player) override;
    void onPlayerDisconnect(IPlayer& player, PeerDisconnectReason reason) override;
    bool onPlayerCommandText(IPlayer& player, StringView message) override;
    void onAmxLoad(IPawnScript& script) override;
    void onAmxUnload(IPawnScript& script) override;

    [[nodiscard]] bool IsUdpBound() const noexcept { return udpBound_; }

private:
    ICore* core_{};
    IPawnComponent* pawn_{};
    ov::server::OVChannelManager channels_;
    ov::server::OVVoiceServer voiceServer_;
    ov::server::ServerVoiceDebug diagnostics_;
    bool udpBound_{};
};
