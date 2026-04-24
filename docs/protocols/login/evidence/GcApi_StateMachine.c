
void Gw2::Game::Net::Cli::GcApi(undefined2 *param_1)

{
  if (param_1 == (undefined2 *)0x0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  if (DAT_142511520 == -1) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  if (param_1 != (undefined2 *)&DAT_1426280a0) {
    *(undefined4 *)(param_1 + 10) = 0;
    Arena::Core::Collections::Array(param_1,DAT_1426280b4);
                    /* WARNING: Subroutine does not return */
    thunk_FUN_1418c47c0(*(undefined8 **)(param_1 + 4),DAT_1426280a8,(ulonglong)DAT_1426280b4);
  }
  return;
}



int Gw2::Game::Net::Cli::GcApi(void)

{
  ulonglong uVar1;
  
  if (DAT_142628028 == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  if (DAT_14262802c == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  uVar1 = Arena::Core::Platform::Windows::Common::WinTime();
  return ((int)uVar1 - DAT_14262802c) + DAT_142628028;
}


undefined8
Gw2::Game::Net::Cli::GcApi
          (longlong param_1,undefined8 param_2,undefined8 param_3,undefined8 param_4)

{
  if (*(int *)(param_1 + 0x1c) != 3) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd700("D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Game\\Net\\Cli\\GcApi.cpp",0x1e9,
                  0x1418fc638,param_4);
  }
  FUN_1402412e0(10);
  return 1;
}


undefined8 Gw2::Game::Net::Cli::GcApi(longlong param_1)

{
  bool bVar1;
  uint uVar2;
  ulonglong uVar3;
  longlong lVar4;
  bool bVar5;
  
  uVar2 = *(uint *)(param_1 + 0x3c);
  if (uVar2 == 0) {
    uVar2 = FUN_140241880();
    *(uint *)(param_1 + 0x3c) = uVar2;
  }
  if (1 < uVar2 - 1) {
    *(undefined4 *)(param_1 + 0x3c) = 0;
    uVar2 = 0;
  }
  bVar5 = uVar2 == 0;
  if ((*(int *)(param_1 + 0x38) == 0) &&
     ((*(ulonglong *)(param_1 + 0x40) >> ((ulonglong)uVar2 & 0x3f) & 1) != 0)) {
    bVar5 = true;
  }
  uVar3 = *(ulonglong *)(param_1 + 0x48) -
          (*(ulonglong *)(param_1 + 0x48) >> 1 & 0x5555555555555555);
  uVar3 = (uVar3 >> 2 & 0x3333333333333333) + (uVar3 & 0x3333333333333333);
  uVar3 = (uVar3 >> 4) + uVar3 & 0xf0f0f0f0f0f0f0f;
  uVar3 = uVar3 + (uVar3 >> 8);
  lVar4 = uVar3 + (uVar3 >> 0x10);
  bVar1 = false;
  if ((byte)((char)((ulonglong)lVar4 >> 0x20) + (char)lVar4) < 2) {
    bVar1 = bVar5;
  }
  if (bVar1) {
    uVar3 = *(ulonglong *)(param_1 + 0x40) -
            (*(ulonglong *)(param_1 + 0x40) >> 1 & 0x5555555555555555);
    uVar3 = (uVar3 >> 2 & 0x3333333333333333) + (uVar3 & 0x3333333333333333);
    uVar3 = (uVar3 >> 4) + uVar3 & 0xf0f0f0f0f0f0f0f;
    uVar3 = uVar3 + (uVar3 >> 8);
    lVar4 = uVar3 + (uVar3 >> 0x10);
    if (1 < (byte)((char)((ulonglong)lVar4 >> 0x20) + (char)lVar4)) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    do {
      do {
        uVar2 = FUN_1402418b0();
        *(uint *)(param_1 + 0x3c) = uVar2;
        uVar3 = 1L << ((byte)uVar2 & 0x3f);
      } while ((*(ulonglong *)(param_1 + 0x48) & uVar3) != 0);
    } while ((*(ulonglong *)(param_1 + 0x40) & uVar3) != 0);
  }
  *(ulonglong *)(param_1 + 0x40) =
       *(ulonglong *)(param_1 + 0x40) | 1L << ((longlong)(int)uVar2 & 0x3fU);
  FUN_14023f240(uVar2);
  return 1;
}


void Gw2::Game::Net::Cli::GcApi
               (longlong *param_1,undefined8 param_2,undefined8 param_3,undefined8 param_4)

{
  short sVar1;
  int iVar2;
  int iVar3;
  int local_res8 [2];
  undefined4 local_res10;
  undefined4 uStackX_14;
  undefined4 local_res18;
  undefined4 uStackX_1c;
  undefined8 local_res20;
  undefined8 local_58 [3];
  
  sVar1 = *(short *)((longlong)param_1 + 0x24);
  while (sVar1 == 0) {
    iVar2 = (int)param_1[4];
    if (iVar2 == 1) goto LAB_14023cb81;
    if (iVar2 == 0) {
      iVar3 = (**(code **)(*param_1 + 8))(param_1);
      if (iVar3 == 0) {
        return;
      }
      *(undefined4 *)(param_1 + 4) = 2;
    }
    else {
      iVar3 = *(int *)((longlong)param_1 + 0x1c);
      if (iVar2 == 2) {
        if (iVar3 == 0) {
          *(undefined4 *)((longlong)param_1 + 0x2c) = 0;
          FUN_14023d6f0(local_58);
        }
        else {
          if (iVar3 != 3) {
                    /* WARNING: Subroutine does not return */
            FUN_1409cd700("D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Game\\Net\\Cli\\GcApi.cpp",0x205
                          ,0x1418fc638,param_4);
          }
          GcAuthCmd(local_58,(undefined4 *)((longlong)param_1 + 0x2c));
        }
        if ((short)local_58[0] == 0) {
          (**(code **)(*param_1 + 0x10))(param_1);
          *(undefined4 *)(param_1 + 4) = 3;
        }
        else {
          if ((short)local_58[0] == -1) {
            return;
          }
          *(undefined8 *)((longlong)param_1 + 0x24) = local_58[0];
        }
      }
      else {
        if (iVar3 == 0) {
          local_res8[0] = 0;
          FUN_14023d6f0(&local_res20);
        }
        else {
          if (iVar3 != 3) {
                    /* WARNING: Subroutine does not return */
            FUN_1409cd700("D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Game\\Net\\Cli\\GcApi.cpp",0x205
                          ,0x1418fc638,param_4);
          }
          GcAuthCmd(&local_res20,local_res8);
        }
        if ((short)local_res20 == 0) {
          if (local_res8[0] == *(int *)((longlong)param_1 + 0x2c)) {
            return;
          }
          local_res10 = 0xb0007;
          uStackX_14 = 0x22e0001;
          local_res20 = 0x22e0001000b0007;
        }
        if ((short)local_res20 == -1) {
          local_res18 = 0xb0007;
          uStackX_1c = 0x2320001;
          local_res20 = 0x2320001000b0007;
        }
        *(undefined8 *)((longlong)param_1 + 0x24) = local_res20;
      }
    }
    if ((*(short *)((longlong)param_1 + 0x24) == 0) && ((int)param_1[4] == iVar2)) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    sVar1 = *(short *)((longlong)param_1 + 0x24);
  }
  if (sVar1 == 0xe) {
    (**(code **)(*param_1 + 0x18))(param_1);
  }
  else {
LAB_14023cb81:
    (**(code **)(*param_1 + 0x18))(param_1);
    if ((*(uint *)(param_1 + 3) & 0x10000000) == 0) goto LAB_14023cbd5;
  }
  CMSG_Build_0x0060_StateToggle_U32();
LAB_14023cbd5:
  (**(code **)*param_1)(param_1,1);
  return;
}

