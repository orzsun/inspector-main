
void Gw2::Game::Net::Cli::GcAuthCmd(undefined8 param_1,longlong param_2)

{
  undefined2 local_res10;
  undefined4 local_res12;
  undefined2 local_res16;
  
  local_res10 = *(undefined2 *)(param_2 + 2);
  local_res12 = 0x2000b;
  local_res16 = 0x282;
  FUN_140239380((undefined8 *)&local_res10);
                    /* WARNING: Subroutine does not return */
  Arena::Core::Platform::Common::Log
            (0,0x1418fcc40,"D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Game\\Net\\Cli\\GcAuthCmd.cpp",
             0x283);
}



void Gw2::Game::Net::Cli::GcAuthCmd(undefined8 *param_1,longlong param_2,short *param_3)

{
  undefined1 auStack_e8 [40];
  ushort uStack_c0;
  WCHAR *pWStack_be;
  WCHAR *pWStack_b6;
  WCHAR aWStack_a8 [32];
  WCHAR aWStack_68 [32];
  ulonglong local_28;
  
  local_28 = DAT_142551900 ^ (ulonglong)auStack_e8;
  if (*param_3 != 0) {
    param_1[3] = 0x138;
    *param_1 = "AuthSrvEncryptCallback";
    param_1[1] = "Gc::AuthSrvEncryptCallback";
    param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Game\\Net\\Cli\\GcAuthCmd.cpp";
    param_1[4] = 0;
    FUN_14023f218();
    return;
  }
  FUN_1409be250(0x20,aWStack_a8);
  FUN_1409be110(0x20,aWStack_68);
  uStack_c0 = 2;
  pWStack_be = aWStack_a8;
  pWStack_b6 = aWStack_68;
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(param_2 + 0x20),0x12,&uStack_c0);
}


undefined8 * Gw2::Game::Net::Cli::GcAuthCmd(undefined8 *param_1,undefined4 *param_2)

{
  undefined8 local_res18 [2];
  
  if (DAT_1426284c8 != (undefined4 *)0x0) {
    if (param_2 != (undefined4 *)0x0) {
      *param_2 = *DAT_1426284c8;
    }
    *(undefined4 *)param_1 = 0xb0000;
    *(undefined4 *)((longlong)param_1 + 4) = 0x38d0002;
    return param_1;
  }
  if (param_2 != (undefined4 *)0x0) {
    *param_2 = 0;
  }
  if (DAT_142628554 != 0) {
    *(undefined4 *)param_1 = 0xbffff;
    *(undefined4 *)((longlong)param_1 + 4) = 0x3960002;
    return param_1;
  }
  FUN_140241940(local_res18);
  if ((short)local_res18[0] == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  *param_1 = local_res18[0];
  return param_1;
}


void Gw2::Game::Net::Cli::GcAuthCmd(undefined1 param_1)

{
  ushort local_res8;
  undefined1 local_resa;
  
  local_res8 = 0xf;
  local_resa = param_1;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),3,&local_res8);
}


void Gw2::Game::Net::Cli::GcAuthCmd
               (undefined4 param_1,undefined8 *param_2,undefined8 *param_3,undefined8 param_4,
               undefined8 param_5)

{
  undefined1 auStack_68 [32];
  ushort local_48;
  undefined4 local_46;
  undefined8 local_42;
  undefined8 uStack_3a;
  undefined8 local_32;
  undefined8 uStack_2a;
  undefined8 local_22;
  undefined8 local_1a;
  ulonglong local_10;
  
  local_10 = DAT_142551900 ^ (ulonglong)auStack_68;
  local_42 = *param_2;
  uStack_3a = param_2[1];
  local_32 = *param_3;
  uStack_2a = param_3[1];
  local_48 = 10;
  local_1a = param_5;
  local_46 = param_1;
  local_22 = param_4;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),0x36,&local_48);
}


void Gw2::Game::Net::Cli::GcAuthCmd(undefined4 param_1)

{
  ushort local_res10;
  undefined4 local_res12;
  short local_res18 [8];
  
  if (DAT_1426284c8 != 0) {
    local_res10 = 0xb;
    local_res12 = param_1;
    if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
                    /* WARNING: Subroutine does not return */
    CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),6,&local_res10);
  }
  if (DAT_142628554 == 0) {
    FUN_140241940((undefined8 *)local_res18);
    if (local_res18[0] == 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
  }
  return;
}


void Gw2::Game::Net::Cli::GcAuthCmd(undefined4 param_1)

{
  ushort local_res10;
  undefined4 local_res12;
  
  local_res10 = 0x22;
  local_res12 = param_1;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),6,&local_res10);
}


void Gw2::Game::Net::Cli::GcAuthCmd(undefined4 param_1,uint param_2,undefined8 param_3)

{
  ushort local_18;
  undefined4 local_16;
  undefined2 local_12;
  undefined8 local_10;
  
  local_18 = 0xc;
  local_16 = param_1;
  if (0xffff < param_2) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  local_12 = (undefined2)param_2;
  local_10 = param_3;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),0x10,&local_18);
}


void Gw2::Game::Net::Cli::GcAuthCmd(undefined4 param_1,undefined4 param_2,undefined1 param_3)

{
  undefined1 auStack_48 [32];
  ushort local_28;
  undefined4 local_26;
  undefined4 local_22;
  undefined1 local_1e;
  ulonglong local_18;
  
  local_18 = DAT_142551900 ^ (ulonglong)auStack_48;
  local_28 = 0xe;
  local_26 = param_1;
  local_22 = param_2;
  local_1e = param_3;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),0xb,&local_28);
}


void Gw2::Game::Net::Cli::GcAuthCmd(undefined4 param_1,undefined4 param_2,undefined4 param_3)

{
  undefined1 auStack_48 [32];
  ushort local_28;
  undefined4 local_26;
  undefined4 local_22;
  undefined4 local_1e;
  ulonglong local_18;
  
  local_18 = DAT_142551900 ^ (ulonglong)auStack_48;
  local_28 = 0x10;
  local_26 = param_1;
  local_22 = param_2;
  local_1e = param_3;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),0xe,&local_28);
}


void Gw2::Game::Net::Cli::GcAuthCmd(undefined4 param_1,undefined8 param_2)

{
  ushort local_18;
  undefined4 local_16;
  undefined8 local_12;
  
  local_18 = 0x14;
  local_16 = param_1;
  local_12 = param_2;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),0xe,&local_18);
}


void Gw2::Game::Net::Cli::GcAuthCmd(undefined4 param_1,undefined8 param_2,undefined8 param_3)

{
  ushort local_28;
  undefined4 local_26;
  undefined8 local_22;
  undefined8 local_1a;
  
  local_28 = 0x18;
  local_26 = param_1;
  local_22 = param_2;
  local_1a = param_3;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),0x16,&local_28);
}


void Gw2::Game::Net::Cli::GcAuthCmd(undefined4 param_1,undefined8 param_2)

{
  ushort local_18;
  undefined4 local_16;
  undefined8 local_12;
  
  local_18 = 0x16;
  local_16 = param_1;
  local_12 = param_2;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),0xe,&local_18);
}


void Gw2::Game::Net::Cli::GcAuthCmd(void)

{
  ushort local_res8 [16];
  
  local_res8[0] = 0x1e;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),2,local_res8);
}


void Gw2::Game::Net::Cli::GcAuthCmd(undefined4 param_1)

{
  ushort local_res10;
  undefined4 local_res12;
  
  local_res10 = 0x19;
  local_res12 = param_1;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),6,&local_res10);
}


void Gw2::Game::Net::Cli::GcAuthCmd
               (undefined4 param_1,undefined8 param_2,undefined8 param_3,undefined8 param_4,
               undefined8 param_5)

{
  ushort local_38;
  undefined4 local_36;
  undefined8 local_32;
  undefined8 local_2a;
  undefined8 local_22;
  undefined8 local_1a;
  
  local_38 = 0x25;
  local_1a = param_5;
  local_36 = param_1;
  local_32 = param_2;
  local_2a = param_3;
  local_22 = param_4;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),0x26,&local_38);
}


void Gw2::Game::Net::Cli::GcAuthCmd(undefined4 param_1,undefined4 *param_2)

{
  undefined1 auStack_68 [32];
  ushort local_48;
  undefined4 local_46;
  undefined4 local_42;
  undefined4 local_3e;
  undefined4 local_3a;
  undefined4 local_36;
  undefined4 local_32;
  undefined4 local_2e;
  undefined4 local_2a;
  undefined1 local_26;
  undefined1 local_25;
  undefined1 local_24;
  undefined1 local_23;
  undefined1 local_22;
  undefined1 local_21;
  undefined1 local_20;
  undefined1 local_1f;
  undefined1 local_1e;
  undefined1 local_1d;
  undefined1 local_1c;
  undefined1 local_1b;
  undefined1 local_1a;
  undefined1 local_19;
  undefined1 local_18;
  undefined1 local_17;
  ulonglong local_10;
  
  local_10 = DAT_142551900 ^ (ulonglong)auStack_68;
  local_32 = param_2[4];
  local_2e = param_2[5];
  local_48 = 0x27;
  local_42 = *param_2;
  local_3e = param_2[1];
  local_3a = param_2[2];
  local_36 = param_2[3];
  local_26 = *(undefined1 *)(param_2 + 7);
  local_23 = *(undefined1 *)((longlong)param_2 + 0x1d);
  local_22 = *(undefined1 *)((longlong)param_2 + 0x1e);
  local_24 = *(undefined1 *)((longlong)param_2 + 0x1f);
  local_20 = *(undefined1 *)(param_2 + 8);
  local_1b = *(undefined1 *)((longlong)param_2 + 0x21);
  local_25 = *(undefined1 *)((longlong)param_2 + 0x22);
  local_21 = *(undefined1 *)((longlong)param_2 + 0x23);
  local_1f = *(undefined1 *)(param_2 + 9);
  local_1d = *(undefined1 *)((longlong)param_2 + 0x25);
  local_1e = *(undefined1 *)((longlong)param_2 + 0x26);
  local_1c = *(undefined1 *)((longlong)param_2 + 0x27);
  local_1a = *(undefined1 *)(param_2 + 10);
  local_19 = *(undefined1 *)((longlong)param_2 + 0x29);
  local_18 = *(undefined1 *)((longlong)param_2 + 0x2a);
  local_17 = *(undefined1 *)((longlong)param_2 + 0x2b);
  local_2a = param_2[6];
  local_46 = param_1;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),0x32,&local_48);
}


void Gw2::Game::Net::Cli::GcAuthCmd
               (undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,
               undefined4 param_5)

{
  undefined1 auStack_48 [32];
  ushort local_28;
  undefined4 local_26;
  undefined4 local_22;
  undefined4 local_1e;
  undefined4 local_1a;
  undefined4 local_16;
  ulonglong local_10;
  
  local_10 = DAT_142551900 ^ (ulonglong)auStack_48;
  local_28 = 0x28;
  local_16 = param_5;
  local_26 = param_1;
  local_22 = param_2;
  local_1e = param_3;
  local_1a = param_4;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),0x16,&local_28);
}


void Gw2::Game::Net::Cli::GcAuthCmd(undefined4 param_1,undefined4 param_2,undefined4 param_3)

{
  undefined1 auStack_48 [32];
  ushort local_28;
  undefined4 local_26;
  undefined4 local_22;
  undefined4 local_1e;
  ulonglong local_18;
  
  local_18 = DAT_142551900 ^ (ulonglong)auStack_48;
  local_28 = 0x26;
  local_26 = param_1;
  local_22 = param_2;
  local_1e = param_3;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),0xe,&local_28);
}


void Gw2::Game::Net::Cli::GcAuthCmd(undefined4 param_1,undefined4 param_2,undefined8 param_3)

{
  ushort local_28;
  undefined4 local_26;
  undefined4 local_22;
  undefined8 local_1e;
  
  local_28 = 0x24;
  local_26 = param_1;
  local_22 = param_2;
  local_1e = param_3;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),0x12,&local_28);
}


void Gw2::Game::Net::Cli::GcAuthCmd
               (undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,
               undefined4 param_5,undefined4 param_6,undefined8 *param_7)

{
  undefined1 auStack_68 [32];
  ushort local_48;
  undefined4 local_46;
  undefined4 local_42;
  undefined4 local_3e;
  undefined4 local_3a;
  undefined4 local_36;
  undefined4 local_32;
  undefined8 local_2e;
  undefined8 uStack_26;
  ulonglong local_18;
  
  local_18 = DAT_142551900 ^ (ulonglong)auStack_68;
  local_48 = 0x1d;
  local_2e = *param_7;
  uStack_26 = param_7[1];
  local_36 = param_5;
  local_32 = param_6;
  local_46 = param_1;
  local_42 = param_2;
  local_3e = param_3;
  local_3a = param_4;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),0x2a,&local_48);
}


void Gw2::Game::Net::Cli::GcAuthCmd(void)

{
  ushort local_res8 [4];
  short local_res10 [12];
  
  if (DAT_1426284c8 != 0) {
    local_res8[0] = 0x23;
    if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
                    /* WARNING: Subroutine does not return */
    CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),2,local_res8);
  }
  if (DAT_142628554 == 0) {
    FUN_140241940((undefined8 *)local_res10);
    if (local_res10[0] == 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
  }
  return;
}


void Gw2::Game::Net::Cli::GcAuthCmd(void)

{
  DWORD DVar1;
  ushort local_res8;
  uint local_resa;
  
  local_res8 = 1;
  DVar1 = timeGetTime();
  local_resa = DVar1 | 1;
  if (DAT_1426284c8 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
                    /* WARNING: Subroutine does not return */
  CMSG::BuildAndSendPacket(*(byte **)(DAT_1426284c8 + 0x20),6,&local_res8);
}





undefined8 *
Gw2::Game::Net::Cli::GcGameCmd(undefined8 *param_1,undefined8 param_2,undefined8 *param_3)

{
  bool bVar1;
  undefined7 extraout_var;
  
  bVar1 = FUN_140239700(DAT_1426283f4,param_3);
  if ((int)CONCAT71(extraout_var,bVar1) == 0) {
    param_1[3] = 0x91;
    *param_1 = "IGcApi_RecvError";
    param_1[1] = "Gc::GameSrvEncryptCallback";
    param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Game\\Net\\Cli\\GcGameCmd.cpp";
    param_1[4] = 0;
    return param_1;
  }
  FUN_140fd2870(DAT_142628290);
  FUN_140238750(DAT_142628290);
  DAT_1426283f8 = 1;
  *param_1 = 0;
  param_1[1] = 0;
  param_1[2] = 0;
  param_1[3] = 0;
  *(undefined4 *)(param_1 + 4) = 0;
  DAT_1426283fc = 1;
  *(undefined4 *)((longlong)param_1 + 0x24) = 1;
  return param_1;
}


/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void Gw2::Game::Net::Cli::GcGameCmd
               (undefined4 param_1,undefined8 *param_2,uint param_3,undefined8 *param_4)

{
  if (DAT_1426283f4 != 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  _DAT_1426282a0 = 0;
  DAT_142628298 = 0;
  _DAT_1426283a0 = 0xb0000;
  _DAT_1426283a4 = 0x1880003;
  DAT_1426283f4 = param_1;
  FUN_140241450(0,param_2,param_4,param_3);
  return;
}
