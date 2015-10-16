#ifndef __LGCONTROL_SERVICE_H
#define __LGCONTROL_SERVICE_H

#define LGREMOTE_SERVICE "LGRemoteService-v1.0"
#include <string>

enum _lg_commands_
{
    LG_Disable3D,
    LG_SideBySide3D,
    LG_SideBySide3DLR,
    LG_TopBottom3D,
    LG_CheckBoard3D,
    LG_FrameSequential3D,
    LG_Converted2D,
    LG_SetVolume,
    LG_GetVolume,
    LG_PowerOff,
    LG_PowerOn,
    LG_TogglePower,
    LG_GetCurrentPowerStatus,
    LG_ToggleMute,
    LG_IsMute,
    LG_ToggleRemoteLocked,
    LG_IsRemoteLocked,
    LG_SwitchAV,
};

enum _lg_av_
{
    LG_AV_DTV1,
    LG_AV_DTV2,
    LG_AV_DTV3,
    LG_AV_DTV4,
    LG_AV_Analog1,
    LG_AV_Analog2,
    LG_AV_Analog3,
    LG_AV_Analog4,
    LG_AV_AV1,
    LG_AV_AV2,
    LG_AV_AV3,
    LG_AV_AV4,
    LG_AV_Component1,
    LG_AV_Component2,
    LG_AV_Component3,
    LG_AV_Component4,
    LG_AV_RGB1,
    LG_AV_RGB2,
    LG_AV_RGB3,
    LG_AV_RGB4,
    LG_AV_HDMI1,
    LG_AV_HDMI2,
    LG_AV_HDMI3,
    LG_AV_HDMI4,
};

std::string lgavnames[] = { "DTV1", "DTV2", "DTV3", "DTV4", "Analog1", "Analog2", "Analog3", "Analog4", "AV1", "AV2", "AV3", "AV4", "Component1", "Component2", "Component3", "Component4", "HDMI1", "HDMI2", "HDMI3", "HDMI4" };

typedef struct
{
    int Command;
    int Option;
    int rc;
} LGRemote_Service_v1_0_t;

#endif
