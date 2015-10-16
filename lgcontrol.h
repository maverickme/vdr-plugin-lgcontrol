/*
 * lgcontrol.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#ifndef __LGCONTROL_H
#define __LGCONTROL_H

#define TIMEOUT                  1.0
#define POWER_CHECK_TIMEOUT      60.0

#define READ_STATUS              0xFF

#define LGDEFAULTDEVICE          "/dev/ttyUSB0"

#define CONVERTED_2D_3D_DEPTH    0x14

#define LGMINVOL                 0x00
#define LGMAXVOL                 0x64
#define LGVOLSTEP                0x01

#define QUEUESENDINTERVALL       2000  // useconds
#define POWERCHECKINTERVALL      60    // seconds

#include "lgcontrol_service.h"

static const char *VERSION        = "0.0.6";
static const char *DESCRIPTION    = "LG-TV remote controled by VDR via rs232";
static const char *MAINMENUENTRY  = NULL;

int AutoPowerOn;
int AutoPowerOff;
int AutoPowerCheck;
int LGVolumeControl;
int LGBaseVolume;
int VDRAVPort;
int IncreaseLevel;
int HideMainMenuEntry;

// --- LG -----------------------------------------------------------

class LG {
private:
  int fd;
  int baudrate;
  int set_id;
public:
  LG();
  ~LG();
  int SendCommand(char cmd1, char cmd2, int value, int value2=0, int value3=0, int value4=0);
  void InitSerial(const char *device);
  void CloseSerial(void);
  };

// --- CommandListItem ---------------------------------------------------------

class CommandListItem:public cListObject {
private:
  char cmd1, cmd2;
  int value1, value2, value3, value4;
public:
  CommandListItem(char _cmd1, char _cmd2, int _value1, int _value2=0, int _value3=0, int _value4=0) { cmd1=_cmd1; cmd2=_cmd2 ;value1=_value1; value2=_value2; value3=_value3; value4=_value4; }
  char  Cmd1() { return   cmd1; }
  char  Cmd2() { return   cmd2; }
  int Value1() { return value1; }
  int Value2() { return value2; }
  int Value3() { return value3; }
  int Value4() { return value4; }
  };

// --- CommandList -------------------------------------------------------------
class CommandList:public cList<CommandListItem> {
  };

// --- cSendThread -----------------------------------------------------------

class cSendThread:public cThread {
private:
  LG          *lg;
  CommandList *CommandQueue;
  int  SendCommand(char cmd1, char cmd2, int value, int value2=0, int value3=0, int value4=0);
  bool sendinprogress;
  bool extsendinprogress;
protected:
  virtual void Action();
public:
  cSendThread(LG **connection);
  ~cSendThread();
  bool GetSendInProgress() { return sendinprogress; }
  void SetExtSendInProgress(bool progress) { extsendinprogress = progress; }
  const char *Working();
  void AddToCommandQueue(char cmd1, char cmd2, int value, int value2=0, int value3=0, int value4=0);
  bool IsCommandQueueEmpty(){return CommandQueue->First();}
  };


// --- cLGCommands -----------------------------------------------------------

class cLGCommands {
private:
  LG          *lg;
  cSendThread *SendThread;
  int  SendCommandDirectToTV(char Cmd1, char Cmd2, int Value1, int Value2=0, int Value3=0, int Value4=0);
  void SendCommandToCmdQueue(char Cmd1, char Cmd2, int Value1, int Value2=0, int Value3=0, int Value4=0);
public:
  cLGCommands(const char *device);
  ~cLGCommands();

  void Disable3D()             { SendCommandToCmdQueue('x', 't', 0x01 ,0x01, 0x01); }
  void Enable3DSbS()           { SendCommandToCmdQueue('x', 't', 0x00, 0x01, 0x00); }
  void Enable3DSbSLR()         { SendCommandToCmdQueue('x', 't', 0x00, 0x01, 0x01); }
  void Enable3DTB()            { SendCommandToCmdQueue('x', 't', 0x00, 0x00, 0x01); }
  void Enable3DCB()            { SendCommandToCmdQueue('x', 't', 0x00, 0x02, 0x00); }
  void Enable3DFS()            { SendCommandToCmdQueue('x', 't', 0x00, 0x03, 0x00); }
  void Enable3DConverted2D()   { SendCommandToCmdQueue('x', 't', 0x03, 0x00, 0x00, CONVERTED_2D_3D_DEPTH); }

  void PowerOn()               { SendCommandToCmdQueue('k', 'a', 0x01); }
  void PowerOff()              { SendCommandToCmdQueue('k', 'a', 0x00); }

  void TogglePower()           { SendCommandToCmdQueue('m', 'c', 0x08); }
  void ToggleMute()            { SendCommandToCmdQueue('m', 'c', 0x09); }

  void SetVolume(int vol)      { SendCommandToCmdQueue('k', 'f', (vol > LGMAXVOL) ? LGMAXVOL : (vol < LGMINVOL) ? LGMINVOL : vol); }

  bool IsMute()                { if (SendCommandDirectToTV('k', 'e', READ_STATUS)) { return false; } else { return true;  } }
  bool IsRemoteLocked()        { if (SendCommandDirectToTV('k', 'm', READ_STATUS)) { return false; } else { return true;  } }
  int  GetCurrentPowerStatus() { if (SendCommandDirectToTV('k', 'a', READ_STATUS)) { return true;  } else { return false; } }

  int  GetCurrentVolume()      { return SendCommandDirectToTV('k', 'f', READ_STATUS); }
  int  GetCurrentAV()          { return SendCommandDirectToTV('x', 'b', READ_STATUS); }

  void SwitchAV(int av)        { switch (av) {
                                  case LG_AV_DTV1:       SendCommandToCmdQueue('x', 'b', 0x00); break;
                                  case LG_AV_DTV2:       SendCommandToCmdQueue('x', 'b', 0x01); break;
                                  case LG_AV_DTV3:       SendCommandToCmdQueue('x', 'b', 0x02); break;
                                  case LG_AV_DTV4:       SendCommandToCmdQueue('x', 'b', 0x03); break;
                                  case LG_AV_Analog1:    SendCommandToCmdQueue('x', 'b', 0x10); break;
                                  case LG_AV_Analog2:    SendCommandToCmdQueue('x', 'b', 0x11); break;
                                  case LG_AV_Analog3:    SendCommandToCmdQueue('x', 'b', 0x12); break;
                                  case LG_AV_Analog4:    SendCommandToCmdQueue('x', 'b', 0x13); break;
                                  case LG_AV_AV1:        SendCommandToCmdQueue('x', 'b', 0x20); break;
                                  case LG_AV_AV2:        SendCommandToCmdQueue('x', 'b', 0x21); break;
                                  case LG_AV_AV3:        SendCommandToCmdQueue('x', 'b', 0x22); break;
                                  case LG_AV_AV4:        SendCommandToCmdQueue('x', 'b', 0x23); break;
                                  case LG_AV_Component1: SendCommandToCmdQueue('x', 'b', 0x40); break;
                                  case LG_AV_Component2: SendCommandToCmdQueue('x', 'b', 0x41); break;
                                  case LG_AV_Component3: SendCommandToCmdQueue('x', 'b', 0x42); break;
                                  case LG_AV_Component4: SendCommandToCmdQueue('x', 'b', 0x43); break;
                                  case LG_AV_RGB1:       SendCommandToCmdQueue('x', 'b', 0x60); break;
                                  case LG_AV_RGB2:       SendCommandToCmdQueue('x', 'b', 0x61); break;
                                  case LG_AV_RGB3:       SendCommandToCmdQueue('x', 'b', 0x62); break;
                                  case LG_AV_RGB4:       SendCommandToCmdQueue('x', 'b', 0x63); break;
                                  case LG_AV_HDMI1:      SendCommandToCmdQueue('x', 'b', 0x90); break;
                                  case LG_AV_HDMI2:      SendCommandToCmdQueue('x', 'b', 0x91); break;
                                  case LG_AV_HDMI3:      SendCommandToCmdQueue('x', 'b', 0x92); break;
                                  case LG_AV_HDMI4:      SendCommandToCmdQueue('x', 'b', 0x93); break;
			       } }

  void ToggleRemoteLocked()    { IsRemoteLocked() ? SendCommandToCmdQueue('k', 'm', 0x00) : SendCommandToCmdQueue('k', 'm', 0x01); }

  };

// --- cCheckTvPowerStatus -------------------------------------------------

class cCheckTvPowerStatus:public cThread {
private:
  bool autocheck;
  cLGCommands *LGCmd;
protected:
  virtual void Action();
public:
  cCheckTvPowerStatus(cLGCommands **commandqueue);
  ~cCheckTvPowerStatus();
  void SetAutoCheck(bool check) { autocheck = check; }
  };

// --- cMenuSetupLGControl -------------------------------------------------

class cMenuSetupLGControl : public cMenuSetupPage {
private:
  int newHideMainMenuEntry;
  int newAutoPowerOff;
  int newAutoPowerOn;
  int newAutoPowerCheck;
  int newLGVolumeControl;
  int newLGBaseVolume;
  int newVDRAVPort;
  int newIncreaseLevel;
  cCheckTvPowerStatus *checkTvPowerStatus;
protected:
  virtual void Store(void);
public:
  cMenuSetupLGControl(cCheckTvPowerStatus **CheckTvPowerStatus);
  };

// --- cMyStatusMonitor ------------------------------------------------------

class cMyStatusMonitor : public cStatus {
private:
  cLGCommands *LGCmd;
protected:
  virtual void SetVolume(int Volume, bool Absolute);
public:
  cMyStatusMonitor(cLGCommands **commandqueue);
  };

// --- cPluginLgcontrol ------------------------------------------------------

class cPluginLgcontrol : public cPlugin {
private:
  bool              IsUserInactive;
  const char       *LGDevice;
  cLGCommands      *LGCmd;
  cMyStatusMonitor *statusMonitor;
  cCheckTvPowerStatus *checkTvPowerStatus;
public:
  cPluginLgcontrol(void);
  virtual ~cPluginLgcontrol();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return DESCRIPTION; }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Initialize(void);
  virtual bool Start(void);
  virtual void Stop(void);
  virtual void Housekeeping(void);
  virtual void MainThreadHook(void);
  virtual const char *MainMenuEntry(void) { return MAINMENUENTRY; }
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
  virtual cString Active(void);
  virtual time_t WakeupTime(void);
  virtual bool Service(const char *Id, void *Data);
  virtual const char **SVDRPHelpPages(void);
  virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode);
  };

#endif //__LGCONTROL_H
