/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */
// Purpose: A central handler for "Game Commands" (GcGameCmd).
// It dispatches based on a command ID (param_1) to various code paths,
// managing the lifecycle of the GcGameCmd system.
//
// Key actions:
// - Initializes global data based on command ID.
// - Can involve complex setup routines for game command system.
// - Handles cleanup/shutdown of the GcGameCmd system.
// - Calls MsgConn::ProcessIncomingRawData (FUN_14023d1c0) for specific commands.
// - Asserts with "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Game\\Net\\Cli\\GcGameCmd.cpp".
void GcGameCmd::Handler(int param_1,undefined8 param_2,uint param_3,byte *param_4)
{
  undefined *puVar1;
  undefined1 auStackY_1f8 [32];
  undefined4 local_1b8 [2];
  longlong local_1b0 [7];
  longlong *local_178;
  longlong local_170 [7];
  undefined8 local_138;
  undefined8 local_128;
  short local_120 [128];
  undefined4 local_20;
  ulonglong local_18;
  
  local_18 = DAT_142551900 ^ (ulonglong)auStackY_1f8;
  if (param_1 == 0) {
    if (DAT_1426283a0 == 0) {
      DAT_1426283a0 = 5;
      _DAT_1426283a2 = 0xb;
      _DAT_1426283a4 = 0xa40003;
    }
  }
  else {
    if (param_1 == 1) {
      if (DAT_1426283a0 == 0) {
        puVar1 = FUN_140fd6730(local_1b8);
        _DAT_1426283f0 = *(undefined4 *)(param_4 + 0x30);
        DAT_142628290 =
             (byte *)FUN_140fd2290(param_2,0,Gw2::Game::Net::Cli::GcGameCmd,local_1b8[0],puVar1,0x14
                                   ,(undefined4 *)(param_4 + 0x34));
        Gw2::Services::Msg::MsgProp(DAT_142628290);
        if (DAT_142628400 == 0) {
          if (DAT_142628404 != 0) {
            local_178 = (longlong *)0x0;
            if (DAT_1426283e8 != (undefined8 *)0x0) {
              local_178 = (longlong *)(**(code **)*DAT_1426283e8)(DAT_1426283e8,local_1b0);
            }
            if (local_178 == (longlong *)0x0) {
                    /* WARNING: Subroutine does not return */
              FUN_1409cd550();
            }
            DAT_142628404 = 1;
            FUN_14023d030((longlong *)&DAT_1426283b0,(longlong)local_1b0);
            if (DAT_142628290 != (byte *)0x0) {
              local_138 = 0;
              if (local_178 != (longlong *)0x0) {
                local_138 = (**(code **)*local_178)(local_178,local_170);
              }
              Gw2::Services::Msg::MsgConn((longlong)DAT_142628290,1,local_170);
            }
            if (local_178 != (longlong *)0x0) {
              (**(code **)(*local_178 + 0x20))(local_178,local_178 != local_1b0);
            }
          }
        }
        else {
          DAT_142628400 = 1;
          if (DAT_142628290 != (byte *)0x0) {
            Gw2::Services::Msg::MsgConn((longlong)DAT_142628290,1);
          }
        }
      }
      goto LAB_14023d6bc;
    }
    if (param_1 == 2) {
      if (DAT_1426283a0 == 0) {
        DAT_1426283a0 = 7;
        _DAT_1426283a2 = 0xb;
        _DAT_1426283a4 = 0xca0003;
      }
      if (DAT_1426283f8 != 0) {
        local_128 = CONCAT44(_DAT_1426283a4,CONCAT22(_DAT_1426283a2,DAT_1426283a0));
        local_20 = DAT_142628298;
                    /* WARNING: Subroutine does not return */
        FUN_1409aab30(local_120,(short *)&DAT_1426282a0,0x80);
      }
      goto LAB_14023d6bc;
    }
    if (param_1 != 3) {
      if (param_1 != 4) {
                    /* WARNING: Subroutine does not return */
        FUN_1409cd700("D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Game\\Net\\Cli\\GcGameCmd.cpp",0x176
                      ,0x1418fca50,param_4);
      }
      if (DAT_142628400 == 0) {
        FUN_14023d1c0(param_3,param_4);
      }
      goto LAB_14023d6bc;
    }
    if (DAT_142628290 != (byte *)0x0) {
      Gw2::Services::Msg::MsgProp(0);
      FUN_140fd2570(DAT_142628290,0,0);
      DAT_142628290 = (byte *)0x0;
    }
    _DAT_1426283f0 = 0;
    DAT_1426283f8 = 0;
    DAT_1426283fc = 0;
  }
  DAT_1426283f4 = 0;
LAB_14023d6bc:
                    /* WARNING: Subroutine does not return */
  FUN_140e37790(local_18 ^ (ulonglong)auStackY_1f8);
}

