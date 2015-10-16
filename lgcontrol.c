/*
 * lgcontrol.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include <termios.h>
#include <getopt.h>
#include <sys/time.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <cstdlib>

#include <vdr/plugin.h>
#include <vdr/status.h>
#include <vdr/shutdown.h>

#include "lgcontrol.h"

// --- LG -----------------------------------------------------------

#ifndef TESTWITHOUT_HARDWARE
LG::LG()
{
  fd = -1;
  baudrate = 9600;
  set_id = 0;
}

LG::~LG()
{
}

void LG::InitSerial(const char *device)
{
  ssize_t r = 0;
  if (fd != -1) return;

  dsyslog("lgcontrol: open: %s\n", device);

  fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) {
     dsyslog("lgcontrol: Cannot open: %s\n", device);
     return;
     }
  tcflush(fd, TCIOFLUSH);

  struct termios tios;
  memset(&tios, 0, sizeof(tios));
  cfsetispeed(&tios, baudrate);
  cfsetospeed(&tios, baudrate);
  tios.c_cflag = CS8 | CLOCAL | CREAD;
  tios.c_iflag = IGNPAR | IGNBRK | IXANY;
  tios.c_lflag = ISIG;
  tios.c_cc[VMIN] = 0;
  tios.c_cc[VTIME] = 0;
  tcsetattr(fd, TCSANOW, &tios);

  tcflush(fd, TCIOFLUSH);
  r=write(fd, "\r", 1);
  r=r;
}

void LG::CloseSerial(void)
{
  if (fd != -1) close(fd);
  fd = -1;
}

int LG::SendCommand(char cmd1, char cmd2, int value, int value2, int value3, int value4)
{
  if (fd < 0) return -256;
  ssize_t r = 0;
  char cmd[30];
  int len;
  int extvalues=value2+value3+value4;
  if ( extvalues > 0 )
     len = sprintf(cmd, "%c%c %02x %02x %02x %02x %02x\r", cmd1, cmd2, set_id, value, value2, value3, value4);
  else {
     if (value >= 0x100)
        len = sprintf(cmd, "%c%c %02x %02x %02x\r", cmd1, cmd2, set_id, value>>8, value&255);
     else
        len = sprintf(cmd, "%c%c %02x %02x\r", cmd1, cmd2, set_id, value);
     }

  dsyslog("lgcontrol: Send %s", cmd);

  r=write(fd, cmd, len);
  r=r;
  tcdrain(fd);
  memset(cmd, 0, sizeof(cmd));
  char *p = cmd;

  struct timeval tv1; gettimeofday(&tv1, NULL);
  double t1 = tv1.tv_sec + (tv1.tv_usec / 1000000.0);

  while (true) {
    struct timeval tv2; gettimeofday(&tv2, NULL);
    double t2 = tv2.tv_sec + (tv2.tv_usec / 1000000.0);
    if (t2 > t1 + TIMEOUT) {
       dsyslog("lgcontrol: Response timeout: %s\n", cmd);
       return -256;
       }
    int n = read(fd, p, 1);
    if (n < 0)
       usleep(1000);
    else if (n == 0)
       usleep(1000);
    else if (p - cmd < (int)sizeof(cmd)) {
       if (p == cmd && cmd[0] == ' ') { cmd[1] = cmd[0]; cmd[0] = cmd2; p++; }
       if (*p == 'x') {
          char cmd2r;
          int id;
          char ok_ng[2];
          int data;
          if (sscanf(cmd, "%c %02x %2c%x", &cmd2r, &id, ok_ng, &data) >= 4) {
             if (cmd2r == cmd2) {
                if (ok_ng[0] == 'O' && ok_ng[1] == 'K') {
                   if ((value == READ_STATUS) || (value == data)) {
                      return data;
                      }
                   }
                else if (ok_ng[0] == 'N' && ok_ng[1] == 'G') {
                   return -data;
                   }
                }
                dsyslog("lgcontrol: Unknown response: %s\n", cmd);
                return -256;
             }
          }
          p++;
       }
  }
}
#else
LG::LG() { time_t t; time(&t); srand((unsigned int)t); }
LG::~LG() {}

void LG::InitSerial(const char *device)     { dsyslog("lgcontrol: fake init Serial: %s", device); }
void LG::CloseSerial(void)                  { dsyslog("lgcontrol: fake close Serial"); }
int LG::SendCommand(char cmd1,  char cmd2,
                    int value,  int value2,
                    int value3, int value4)
                                            { dsyslog("lgcontrol: fake send %c%c %i %i %i %i", cmd1, cmd2, value, value2, value3, value4); return rand() % 64; }
#endif

// --- cSendThread -----------------------------------------------------------

cSendThread::cSendThread(LG **connection):cThread("lgcontrol: rs232 communication thread")
{
  sendinprogress=false;
  extsendinprogress=false;
  lg=*connection;
  CommandQueue=new CommandList();
  Start();
}

cSendThread::~cSendThread()
{
  Cancel(3);
  delete CommandQueue;
}

const char *cSendThread::Working()
{
  if (CommandQueue->First()!=NULL)
    return "lgcontrol: Command queue not empty";

  return NULL;
}

void cSendThread::Action()
{
  CommandListItem *commanditem=NULL;
  while(Running()) {
    if (!extsendinprogress) {
       commanditem=CommandQueue->First();
       if ( commanditem != NULL ) {
          sendinprogress=true;
          lg->SendCommand(commanditem->Cmd1(), commanditem->Cmd2(), commanditem->Value1(), commanditem->Value2(), commanditem->Value3(), commanditem->Value4());
          sendinprogress=false;
          CommandQueue->Del(commanditem);
          }
       }
    usleep(QUEUESENDINTERVALL);
  }
}

void cSendThread::AddToCommandQueue(char cmd1, char cmd2, int value, int value2, int value3, int value4)
{
  CommandQueue->Add(new CommandListItem(cmd1, cmd2, value, value2, value3, value4));
}

// --- cLGCommands ----------------------------------------------------------

cLGCommands::cLGCommands(const char *device)
{
  lg = new LG();
  lg->InitSerial(device);
  SendThread = new cSendThread(&lg);
}

cLGCommands::~cLGCommands()
{
  delete SendThread;
  lg->CloseSerial();
  delete lg;
}

void cLGCommands::SendCommandToCmdQueue(char Cmd1, char Cmd2, int Value1, int Value2, int Value3, int Value4)
{
  SendThread->AddToCommandQueue(Cmd1, Cmd2, Value1, Value2, Value3, Value4);
}

int cLGCommands::SendCommandDirectToTV(char Cmd1, char Cmd2, int Value1, int Value2, int Value3, int Value4)
{
  int rc;
  int timeout=0;
  while (timeout*(QUEUESENDINTERVALL/3) < TIMEOUT*1000 ) {
     if (!SendThread->GetSendInProgress()) {
        SendThread->SetExtSendInProgress(true);
        rc=lg->SendCommand(Cmd1, Cmd2, Value1, Value2, Value3, Value4);
        SendThread->SetExtSendInProgress(false);
        return rc;
        }
     timeout++;
     usleep(QUEUESENDINTERVALL/3);
    }
  return -1;
}

// --- cCheckTvPowerStatus ---------------------------------------------------

cCheckTvPowerStatus::cCheckTvPowerStatus(cLGCommands **commandqueue):cThread("lgcontrol: Tv PowerStatusCheck thread")
{
  LGCmd = *commandqueue;
  autocheck=true;
}

cCheckTvPowerStatus::~cCheckTvPowerStatus()
{
  Cancel(3);
}

void cCheckTvPowerStatus::Action()
{
  while (autocheck) {
    usleep(POWERCHECKINTERVALL*1000000);
    if (LGCmd->GetCurrentPowerStatus() == 0) ShutdownHandler.SetUserInactive();
    }
}

// --- cMenuSetupLGControl -------------------------------------------------------

cMenuSetupLGControl::cMenuSetupLGControl(cCheckTvPowerStatus **CheckTvPowerStatus)
{
  newAutoPowerOff    = AutoPowerOff;
  newAutoPowerOn     = AutoPowerOn;
  newAutoPowerCheck  = AutoPowerCheck;
  newLGVolumeControl = LGVolumeControl;
  newVDRAVPort       = VDRAVPort;
  newLGBaseVolume    = LGBaseVolume;
  newIncreaseLevel   = IncreaseLevel;
  newHideMainMenuEntry = HideMainMenuEntry;

  checkTvPowerStatus = *CheckTvPowerStatus;

  Add(new cMenuEditBoolItem(tr("Hide Mainmenuentry"),   &newHideMainMenuEntry));
  Add(new cMenuEditBoolItem(tr("Auto TV Power Off"),   &newAutoPowerOff));
  Add(new cMenuEditBoolItem(tr("Auto TV Power On"),    &newAutoPowerOn));
  Add(new cMenuEditBoolItem(tr("Set User inactive if TV is Off"),    &newAutoPowerCheck));
  Add(new cMenuEditBoolItem(tr("LG Volume Control"),   &newLGVolumeControl));
  Add(new cMenuEditIntItem(tr("  LG Basevolume"),      &newLGBaseVolume));
  Add(new cMenuEditIntItem(tr("  VDR Increaselevel"),  &newIncreaseLevel));
}

void cMenuSetupLGControl::Store(void)
{
  if (AutoPowerCheck != newAutoPowerCheck) {
     if (newAutoPowerCheck) {
        checkTvPowerStatus->SetAutoCheck(true);
        checkTvPowerStatus->Start();
        }
     else
        checkTvPowerStatus->SetAutoCheck(false);
  }
  SetupStore("HideMainMenuEntry",HideMainMenuEntry  = newHideMainMenuEntry);
  SetupStore("AutoPowerOff",     AutoPowerOff    = newAutoPowerOff);
  SetupStore("AutoPowerOn",      AutoPowerOn     = newAutoPowerOn);
  SetupStore("AutoPowerCheck",   AutoPowerCheck  = newAutoPowerCheck);
  SetupStore("LGVolumeControl",  LGVolumeControl = newLGVolumeControl);
  SetupStore("VDRAVPort",        VDRAVPort       = newVDRAVPort);
  SetupStore("LGBaseVolume",     LGBaseVolume    = newLGBaseVolume);
  SetupStore("IncreaseLevel",    IncreaseLevel   = newIncreaseLevel);
}

// --- cMyStatusMonitor ---------------------------------------------------

cMyStatusMonitor::cMyStatusMonitor(cLGCommands **commandqueue)
{
  LGCmd = *commandqueue;
}

void cMyStatusMonitor::SetVolume(int Volume, bool Absolute)
{
  if (LGVolumeControl) {
     int lgvol=LGBaseVolume;
     float factor;
     int vdrvol=cDevice::PrimaryDevice()->CurrentVolume();
     float fvdrvol=(float)IncreaseLevel;

     if (vdrvol == 0)
        lgvol=0;
     else if (vdrvol > IncreaseLevel) {
        factor=(float)(MAXVOLUME - IncreaseLevel) / (float)LGMAXVOL;
        while (fvdrvol < (float)vdrvol) {
          fvdrvol=fvdrvol+factor;
          lgvol++;
          }
        }
     LGCmd->SetVolume(lgvol);
     }
}

// --- cPluginLgcontrol ------------------------------------------------------

cPluginLgcontrol::cPluginLgcontrol(void)
{
  LGDevice        = LGDEFAULTDEVICE;
  HideMainMenuEntry = 1;
  VDRAVPort       = 0;
  AutoPowerOn     = 1;
  AutoPowerOff    = 1;
  AutoPowerCheck  = 1;
  LGVolumeControl = 1;
  LGBaseVolume    = 0;
  IncreaseLevel   = 0;
  LGCmd           = NULL;
  statusMonitor   = NULL;
  checkTvPowerStatus = NULL;
  IsUserInactive  = true;
}

cPluginLgcontrol::~cPluginLgcontrol()
{
  delete checkTvPowerStatus;
  delete statusMonitor;
  delete LGCmd;
}

const char *cPluginLgcontrol::CommandLineHelp(void)
{
  return "  -d DEV,  --device=DEV   serial device where LG TV is connected";
}

bool cPluginLgcontrol::ProcessArgs(int argc, char *argv[])
{
  static struct option long_options[] = {
       { "device",   required_argument, NULL, 'd' },
       { NULL,       no_argument,       NULL,  0  }
    };

  int c;
  while ((c = getopt_long(argc, argv, "d:", long_options, NULL)) != -1) {
        switch (c) {
          case 'd': LGDevice = optarg;
                    break;
          default:  return false;
          }
        }
  return true;
}

bool cPluginLgcontrol::Initialize(void)
{
  return true;
}

bool cPluginLgcontrol::Start(void)
{
  LGCmd = new cLGCommands(LGDevice);
  statusMonitor = new cMyStatusMonitor(&LGCmd);
  checkTvPowerStatus = new cCheckTvPowerStatus(&LGCmd);
  if (AutoPowerCheck)
     checkTvPowerStatus->Start();
  return true;
}

void cPluginLgcontrol::Stop(void)
{
  if (ShutdownHandler.IsUserInactive() && AutoPowerOff)
     LGCmd->PowerOff();
}

void cPluginLgcontrol::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

void cPluginLgcontrol::MainThreadHook(void)
{
  if ( IsUserInactive != ShutdownHandler.IsUserInactive() ) {
     IsUserInactive=ShutdownHandler.IsUserInactive();
     if (IsUserInactive) {
        if (AutoPowerOff)
           LGCmd->PowerOff();
        }
     else {
        if (AutoPowerOn)
           LGCmd->PowerOn();
        }
     }
}

cString cPluginLgcontrol::Active(void)
{
  // Return a message string if shutdown should be postponed
  return NULL;
}

time_t cPluginLgcontrol::WakeupTime(void)
{
  // Return custom wakeup time for shutdown script
  return 0;
}

cOsdObject *cPluginLgcontrol::MainMenuAction(void)
{
  // Perform the action when selected from the main VDR menu.
  return NULL;
}

cMenuSetupPage *cPluginLgcontrol::SetupMenu(void)
{
  return new cMenuSetupLGControl(&checkTvPowerStatus);
}

bool cPluginLgcontrol::SetupParse(const char *Name, const char *Value)
{
  if      (!strcasecmp(Name, "AutoPowerOff"))      AutoPowerOff    = atoi(Value);
  else if (!strcasecmp(Name, "AutoPowerOn"))       AutoPowerOn     = atoi(Value);
  else if (!strcasecmp(Name, "AutoPowerCheck"))    AutoPowerCheck  = atoi(Value);
  else if (!strcasecmp(Name, "LGVolumeControl"))   LGVolumeControl = atoi(Value);
  else if (!strcasecmp(Name, "LGBaseVolume"))      LGBaseVolume    = atoi(Value);
  else if (!strcasecmp(Name, "VDRAVPort"))         VDRAVPort       = atoi(Value);
  else if (!strcasecmp(Name, "IncreaseLevel"))     IncreaseLevel   = atoi(Value);
  else
     return false;
  return true;
}

bool cPluginLgcontrol::Service(const char *Id, void *Data)
{
  if (strcmp(Id, LGREMOTE_SERVICE) == 0) {
     LGRemote_Service_v1_0_t *r =
           (LGRemote_Service_v1_0_t *) Data;
     switch (r->Command) {
       case LG_Disable3D:
            LGCmd->Disable3D();
            break;
       case LG_SideBySide3D:
            LGCmd->Enable3DSbS();
            break;
       case LG_SideBySide3DLR:
            LGCmd->Enable3DSbSLR();
            break;
       case LG_TopBottom3D:
            LGCmd->Enable3DTB();
            break;
       case LG_CheckBoard3D:
            LGCmd->Enable3DCB();
            break;
       case LG_FrameSequential3D:
            LGCmd->Enable3DFS();
            break;
       case LG_Converted2D:
            LGCmd->Enable3DConverted2D();
            break;
       case LG_SetVolume:
            LGCmd->SetVolume(r->Option);
            break;
       case LG_GetVolume:
            r->rc=LGCmd->GetCurrentVolume();
            break;
       case LG_PowerOff:
            LGCmd->PowerOff();
            break;
       case LG_PowerOn:
            LGCmd->PowerOn();
            break;
       case LG_TogglePower:
            LGCmd->TogglePower();
            break;
       case LG_GetCurrentPowerStatus:
            r->rc=LGCmd->GetCurrentPowerStatus();
            break;
       case LG_ToggleMute:
            LGCmd->ToggleMute();
            break;
       case LG_IsMute:
            r->rc=LGCmd->IsMute();
            break;
       case LG_ToggleRemoteLocked:
            LGCmd->ToggleRemoteLocked();
            break;
       case LG_IsRemoteLocked:
            r->rc=LGCmd->IsRemoteLocked();
            break;
       case LG_SwitchAV:
            LGCmd->SwitchAV(r->Option);
            break;
       default:
            return false;
       }
     return true;
     }

  return false;
}

const char **cPluginLgcontrol::SVDRPHelpPages(void)
{
  static const char *HelpPages[] = {
    "LGTV [ ON | OFF ]\n"
    "    Switch TV\n"
    "    If no option is given, the current status will be returned.",
    "LGAV [ DTV{1-4} | Analog{1-4} | AV{1-4} | Component{1-4} | RGB{1-4} | HDMI {1-4} ]\n"
    "    Switch AV\n"
    "    If no option is given, the current status will be returned.",
    "LG3D [ OFF | SBS | SBSRL | SBSLR | TB | FS | CB | 2D ]\n"
    "    Switch TV 3D mode\n"
    "      SBS | SBSRL = Side By Side (Right Left)\n"
    "      SBSLR       = Side By Side (Left Rigth)\n"
    "      TB          = Top Bottom\n",
    "LVOL [ <number> | + | - | mute ]\n"
    "    Set the audio volume to the given number (which is limited to the range\n"
    "    0...255). If the special options '+' or '-' are given, the volume will\n"
    "    be turned up or down, respectively. The option 'mute' will toggle the\n"
    "    audio muting. If no option is given, the current audio volume level will\n"
    "    be returned.",
    NULL
    };
  return HelpPages;
}

cString cPluginLgcontrol::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode)
{
  char buffer[64];
  if (strcasecmp(Command, "LGTV") == 0) {
     if (Option != NULL) {
        if      (strcasecmp(Option, "ON")  == 0) { LGCmd->PowerOn();  return "TV switched on"; }
        else if (strcasecmp(Option, "OFF") == 0) { LGCmd->PowerOff(); return "TV switched off"; }
        else return "Unknown option";
        }
     if (LGCmd->GetCurrentPowerStatus()) return "TV is on";
     else return "TV is off";
     }
  else if (strcasecmp(Command, "LGAV") == 0) {
     if (Option != NULL) {
        if      (strcasecmp(Option, "DTV1") == 0) {       LGCmd->SwitchAV(LG_AV_DTV1);            return "switch AV to DTV1";    }
        else if (strcasecmp(Option, "DTV2") == 0) {       LGCmd->SwitchAV(LG_AV_DTV2);            return "switch AV to DTV2";    }
        else if (strcasecmp(Option, "DTV3") == 0) {       LGCmd->SwitchAV(LG_AV_DTV3);            return "switch AV to DTV3";    }
        else if (strcasecmp(Option, "DTV4") == 0) {       LGCmd->SwitchAV(LG_AV_DTV4);            return "switch AV to DTV4";    }
        else if (strcasecmp(Option, "Analog1") == 0) {    LGCmd->SwitchAV(LG_AV_Analog1);         return "switch AV to Analog1"; }
        else if (strcasecmp(Option, "Analog2") == 0) {    LGCmd->SwitchAV(LG_AV_Analog2);         return "switch AV to Analog2"; }
        else if (strcasecmp(Option, "Analog3") == 0) {    LGCmd->SwitchAV(LG_AV_Analog3);         return "switch AV to Analog3"; }
        else if (strcasecmp(Option, "Analog4") == 0) {    LGCmd->SwitchAV(LG_AV_Analog4);         return "switch AV to Analog4"; }
        else if (strcasecmp(Option, "AV1") == 0) {        LGCmd->SwitchAV(LG_AV_AV1);             return "switch AV to AV1";     }
        else if (strcasecmp(Option, "AV2") == 0) {        LGCmd->SwitchAV(LG_AV_AV2);             return "switch AV to AV2";     }
        else if (strcasecmp(Option, "AV3") == 0) {        LGCmd->SwitchAV(LG_AV_AV3);             return "switch AV to AV3";     }
        else if (strcasecmp(Option, "AV4") == 0) {        LGCmd->SwitchAV(LG_AV_AV4);             return "switch AV to AV4";     }
        else if (strcasecmp(Option, "Component1") == 0) { LGCmd->SwitchAV(LG_AV_Component1);      return "switch AV to Component1"; }
        else if (strcasecmp(Option, "Component2") == 0) { LGCmd->SwitchAV(LG_AV_Component2);      return "switch AV to Component2"; }
        else if (strcasecmp(Option, "Component3") == 0) { LGCmd->SwitchAV(LG_AV_Component3);      return "switch AV to Component3"; }
        else if (strcasecmp(Option, "Component4") == 0) { LGCmd->SwitchAV(LG_AV_Component4);      return "switch AV to Component4"; }
        else if (strcasecmp(Option, "RGB1") == 0) {       LGCmd->SwitchAV(LG_AV_RGB1);            return "switch AV to RGB1"; }
        else if (strcasecmp(Option, "RGB2") == 0) {       LGCmd->SwitchAV(LG_AV_RGB2);            return "switch AV to RGB2"; }
        else if (strcasecmp(Option, "RGB3") == 0) {       LGCmd->SwitchAV(LG_AV_RGB3);            return "switch AV to RGB3"; }
        else if (strcasecmp(Option, "RGB4") == 0) {       LGCmd->SwitchAV(LG_AV_RGB4);            return "switch AV to RGB4"; }
        else if (strcasecmp(Option, "HDMI1") == 0) {      LGCmd->SwitchAV(LG_AV_HDMI1);           return "switch AV to HDMI1"; }
        else if (strcasecmp(Option, "HDMI2") == 0) {      LGCmd->SwitchAV(LG_AV_HDMI2);           return "switch AV to HDMI2"; }
        else if (strcasecmp(Option, "HDMI3") == 0) {      LGCmd->SwitchAV(LG_AV_HDMI3);           return "switch AV to HDMI3"; }
        else if (strcasecmp(Option, "HDMI4") == 0) {      LGCmd->SwitchAV(LG_AV_HDMI4);           return "switch AV to HDMI4"; }
        else return "Unknown option";
        }
     if (LGCmd->GetCurrentAV()) return "AV true"; 
     return "AV false"; 
  }
  else if (strcasecmp(Command, "LG3D") == 0) {
     if (Option != NULL) {
        if      (strcasecmp(Option, "OFF")   == 0) { LGCmd->Disable3D();      return "3D mode off"; }
        else if (strcasecmp(Option, "SBSRL") == 0) { LGCmd->Enable3DSbS();    return "3D mode - Side by Side RL"; }
        else if (strcasecmp(Option, "SBSLR") == 0) { LGCmd->Enable3DSbSLR();  return "3D mode - Side by Side LR"; }
        else if (strcasecmp(Option, "SBS")   == 0) { LGCmd->Enable3DSbS();    return "3D mode - Side by Side"; }
        else if (strcasecmp(Option, "TB")    == 0) { LGCmd->Enable3DTB();     return "3D mode - Top Bottom"; }
        else if (strcasecmp(Option, "CB")    == 0) { LGCmd->Enable3DCB();     return "3D mode - Check Board"; }
        else if (strcasecmp(Option, "FS")    == 0) { LGCmd->Enable3DFS();     return "3D mode - Frame Sequential"; }
        else if (strcasecmp(Option, "2D")    == 0) { LGCmd->Enable3DConverted2D(); return "3D mode - convert 2D->3D "; }
        else return "Unknown option";
        }
     }
  else if (strcasecmp(Command, "LVOL") == 0) {
    if (Option != NULL) {
       if      (isnumber(Option))                LGCmd->SetVolume(strtol(Option, NULL, 10));
       else if (strcmp(Option, "+") == 0)        LGCmd->SetVolume(LGCmd->GetCurrentVolume()+LGVOLSTEP);
       else if (strcmp(Option, "-") == 0)        LGCmd->SetVolume(LGCmd->GetCurrentVolume()-LGVOLSTEP);
       else if (strcasecmp(Option, "MUTE") == 0) LGCmd->ToggleMute();
       else return "Unknown option";
       }
    if (LGCmd->IsMute())
       sprintf(buffer, "Audio is mute");
    else
       sprintf(buffer, "Audio volume is %i", LGCmd->GetCurrentVolume());
    return buffer;
    }

  return NULL;
}

VDRPLUGINCREATOR(cPluginLgcontrol);
