#pragma rational Float(32, 8)
#include <ompvoice>

main()
{
    new channel = OV_CreateChannel(25.0);
    OV_EnableVoice(0, true);
    OV_SetVolume(0, 1.0);
    OV_SetMuted(0, 1, false);
    OV_SetTalkKey(0, 0x5A);
    OV_ShowHudIcon(0, true);
    OV_CreateRadioChannel();
    OV_JoinRadioChannel(0, channel);
    OV_LeaveRadioChannel(0, channel);
    OV_StartPhoneCall(0, 1);
    OV_EndPhoneCall(0);
    OV_DestroyChannel(channel);
}
