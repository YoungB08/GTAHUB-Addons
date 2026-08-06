#if defined _ompvoice_included
    #endinput
#endif
#define _ompvoice_included

native OV_EnableVoice(playerid, bool:enable);
native bool:OV_IsTalking(playerid);
native OV_SetVolume(playerid, Float:volume);
native OV_SetMuted(listenerid, targetid, bool:mute);
native OV_CreateChannel(Float:distance);
native OV_DestroyChannel(channelid);
native OV_SetTalkKey(playerid, keyid);
native OV_StartPhoneCall(playerid, targetid);
native OV_EndPhoneCall(playerid);
native OV_ShowHudIcon(playerid, bool:show);
native OV_CreateRadioChannel();
native OV_JoinRadioChannel(playerid, channelid);
native OV_LeaveRadioChannel(playerid, channelid);
