// [OLD_NAME]: Gs2c_SrvMsgDispatcher
undefined8 * Msg::DispatchStream(undefined8 *param_1,byte *param_2,undefined4 *param_3)

{
  byte *pbVar1;
  longlong *plVar2;
  longlong lVar3;
  code *pcVar4;
  ushort *puVar5;
  int iVar6;
  uint uVar7;
  undefined8 uVar8;
  longlong extraout_RAX;
  longlong extraout_RAX_00;
  undefined8 *puVar9;
  ushort *puVar10;
  ushort *puVar11;
  uint uVar12;
  uint uVar13;
  undefined1 *puVar14;
  ulonglong *puVar15;
  ulonglong uVar16;
  bool bVar17;
  uint local_res10 [2];
  undefined4 *local_res18;
  int local_res20 [2];
  int local_68 [2];
  uint local_60 [2];
  ushort *local_58;
  ulonglong *local_50;
  ushort *local_48;
  ushort *local_40;
  
  pbVar1 = param_2 + 0x80;
  bVar17 = *(int *)(param_2 + 0x94) == *(int *)(param_2 + 0xa0);
  local_res18 = param_3;
  do {
    if (bVar17) {
LAB_140fd1e34:
      *param_1 = 0;
      param_1[1] = 0;
      param_1[2] = 0;
      param_1[3] = 0;
      *(undefined4 *)(param_1 + 4) = 0;
      *(undefined4 *)((longlong)param_1 + 0x24) = 1;
      return param_1;
    }
    if (*(longlong *)(param_2 + 0x48) == 0) {
      local_res10[0] = 0;
      uVar8 = Arena::Core::Collections::Array
                        ((longlong)pbVar1,2,4,(undefined1 (*) [32])local_res10,0);
      if ((int)uVar8 != 0) {
        uVar8 = FUN_140fd5b90(*(longlong *)(param_2 + 0x18),local_res10[0]);
        if ((int)uVar8 != 0) {
          *(int *)(param_2 + 0x50) = *(int *)(param_2 + 0x50) + 1;
        }
        uVar12 = *(uint *)(param_2 + 0x50);
        uVar8 = FUN_140fd5bb0(*(longlong *)(param_2 + 0x18));
        if (uVar12 <= (uint)uVar8) {
                    /* WARNING: Subroutine does not return */
          FUN_1409bdb00();
        }
        FUN_140fd5b70(*(longlong *)(param_2 + 0x18),*param_3,*(undefined4 *)(param_2 + 0x40));
                    /* WARNING: Subroutine does not return */
        Arena::Core::Platform::Common::Log
                  (0,0x1420de8e0,(ulonglong)*(uint *)(param_2 + 0x10),
                   (ulonglong)*(uint *)(param_2 + 0x40));
      }
      goto LAB_140fd1e34;
    }
    if (*(int *)(*(longlong *)(*(longlong *)(param_2 + 0x48) + 8) + 0x20) == 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    puVar10 = (ushort *)(param_2 + 0xa8);
    if (*(longlong *)(param_2 + 0xb8) != *(longlong *)(param_2 + 0xc0)) {
      uVar13 = *(int *)(param_2 + 0x94) - *(int *)(param_2 + 0xa0);
      local_60[0] = uVar13;
      uVar7 = FUN_140fd6f20((int *)puVar10);
      uVar12 = (uint)(*(int *)(param_2 + 0xb0) - *(int *)puVar10) >> 1;
      local_58 = (ushort *)CONCAT44(local_58._4_4_,uVar7);
      local_40 = puVar10;
      if (uVar12 <= uVar7) {
                    /* WARNING: Subroutine does not return */
        FUN_1409cd550();
      }
      uVar12 = uVar12 - uVar7;
      if (uVar13 < uVar12) {
        uVar12 = uVar13;
      }
      uVar16 = (ulonglong)uVar12;
      puVar14 = (undefined1 *)((ulonglong)*(uint *)(param_2 + 0xa0) + *(longlong *)(param_2 + 0x88))
      ;
      FUN_140fd6b30((int *)puVar10,uVar12,puVar14);
      puVar11 = (ushort *)FUN_140fd6fc0((undefined8 *)puVar10,uVar16,puVar14);
      puVar10 = *(ushort **)(param_2 + 0xc0);
      uVar7 = *(uint *)(*(longlong *)(*(longlong *)(param_2 + 0x48) + 8) + 0x20);
      local_48 = puVar11;
      local_50 = (ulonglong *)Arena::Core::Collections::Array(*(uint **)(param_2 + 200),uVar7);
      local_68[0] = 0;
      local_res10[0] = 0xffffffff;
      puVar15 = MsgUnpack::ParseWithSchema
                          (*(uint **)(*(longlong *)(param_2 + 0x48) + 8),(ulonglong *)&local_48,
                           puVar10,local_50,(longlong)local_50 + (ulonglong)uVar7,
                           *(undefined8 *)(param_2 + 200),local_68,local_res10);
      param_3 = local_res18;
      if (local_68[0] == 0) {
        if (extraout_RAX_00 == 0) {
          puVar11 = (ushort *)
                    ((ulonglong)*(uint *)(param_2 + 0xa0) + (ulonglong)uVar12 +
                    *(longlong *)(param_2 + 0x88));
          uVar12 = local_60[0] - uVar12;
          puVar10 = local_40;
          goto LAB_140fd1dc5;
        }
        if (((*param_2 & 1) != 0) && (DAT_142854950 != (code *)0x0)) {
          (*DAT_142854950)(*local_res18,*(undefined4 *)(param_2 + 0x40));
        }
        uVar12 = (int)local_48 - (int)puVar11;
        if (*(longlong *)(param_2 + 0x390) != 0) {
          plVar2 = *(longlong **)(param_2 + 0x390);
          local_60[0] = uVar12;
          local_40 = puVar11;
          if (plVar2 == (longlong *)0x0) {
            FUN_140e36738();
            pcVar4 = (code *)swi(3);
            puVar9 = (undefined8 *)(*pcVar4)();
            return puVar9;
          }
          (**(code **)(*plVar2 + 0x10))(plVar2,&local_40,local_60);
        }
        lVar3 = *(longlong *)(param_2 + 0x48);
        if (*(int *)(lVar3 + 0x10) == 0) {
          iVar6 = (**(code **)(lVar3 + 0x18))(param_3,local_50);
        }
        else {
          if (*(int *)(lVar3 + 0x10) != 1) {
                    /* WARNING: Subroutine does not return */
            FUN_1409cd700("D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp",
                          0xaf2,0x1420de850,puVar15);
          }
          iVar6 = (**(code **)(lVar3 + 0x18))(*param_3,local_50);
        }
        if (iVar6 != 0) {
          thunk_FUN_140fd6360(*(undefined4 *)(param_2 + 0x10),**(undefined4 **)(param_2 + 0x48),
                              *(undefined4 *)
                               (*(longlong *)(*(undefined4 **)(param_2 + 0x48) + 2) + 0x10),
                              local_res10[0],uVar12);
          Gw2::Services::Msg::MsgUtil((longlong)pbVar1,uVar12 - (int)local_58);
          FUN_140fd6ee0((undefined8 *)(param_2 + 0xa8));
          goto LAB_140fd1cd8;
        }
      }
LAB_140fd1dda:
      FUN_1409a35f0();
      param_2[0x20] = 1;
      param_2[0x21] = 0;
      param_2[0x22] = 0;
      param_2[0x23] = 0;
      if ((*(int *)(param_2 + 0x10) != 0) || (*(int *)(param_2 + 0x40) != 1)) {
        *(undefined4 *)(param_1 + 3) = 0xb30;
        *param_1 = "ERR_DISPATCH_FAILED";
        param_1[1] = "Msg::DispatchStream";
        param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
        *(undefined4 *)((longlong)param_1 + 0x1c) = *(undefined4 *)(param_2 + 0x40);
        *(undefined4 *)(param_1 + 4) = *(undefined4 *)(param_2 + 0x44);
        *(undefined4 *)((longlong)param_1 + 0x24) = 0;
        return param_1;
      }
      goto LAB_140fd1e34;
    }
    local_40 = (ushort *)((ulonglong)*(uint *)(param_2 + 0x94) + *(longlong *)(param_2 + 0x88));
    puVar11 = (ushort *)((ulonglong)*(uint *)(param_2 + 0xa0) + *(longlong *)(param_2 + 0x88));
    uVar12 = *(uint *)(*(longlong *)(*(longlong *)(param_2 + 0x48) + 8) + 0x20);
    local_58 = puVar11;
    local_50 = (ulonglong *)Arena::Core::Collections::Array(*(uint **)(param_2 + 200),uVar12);
    puVar5 = local_40;
    local_res20[0] = 0;
    local_res10[0] = 0xffffffff;
    puVar15 = MsgUnpack::ParseWithSchema
                        (*(uint **)(*(longlong *)(param_2 + 0x48) + 8),(ulonglong *)&local_58,
                         local_40,local_50,(longlong)local_50 + (ulonglong)uVar12,
                         *(undefined8 *)(param_2 + 200),local_res20,local_res10);
    if (local_res20[0] != 0) goto LAB_140fd1dda;
    if (extraout_RAX == 0) {
      uVar12 = (int)puVar5 - (int)puVar11;
LAB_140fd1dc5:
      FUN_140fd6b30((int *)puVar10,uVar12,(undefined1 *)puVar11);
      param_2[0x94] = 0;
      param_2[0x95] = 0;
      param_2[0x96] = 0;
      param_2[0x97] = 0;
      param_2[0xa0] = 0;
      param_2[0xa1] = 0;
      param_2[0xa2] = 0;
      param_2[0xa3] = 0;
      goto LAB_140fd1e34;
    }
    if (((*param_2 & 1) != 0) && (DAT_142854950 != (code *)0x0)) {
      (*DAT_142854950)(*param_3,*(undefined4 *)(param_2 + 0x40));
    }
    uVar12 = (int)local_58 - (int)puVar11;
    if (*(longlong *)(param_2 + 0x390) != 0) {
      plVar2 = *(longlong **)(param_2 + 0x390);
      local_60[0] = uVar12;
      local_40 = puVar11;
      if (plVar2 == (longlong *)0x0) {
        FUN_140e36738();
        pcVar4 = (code *)swi(3);
        puVar9 = (undefined8 *)(*pcVar4)();
        return puVar9;
      }
      (**(code **)(*plVar2 + 0x10))(plVar2,&local_40,local_60);
    }
    lVar3 = *(longlong *)(param_2 + 0x48);
    if (*(int *)(lVar3 + 0x10) == 0) {
      iVar6 = (**(code **)(lVar3 + 0x18))(param_3,local_50);
    }
    else {
      if (*(int *)(lVar3 + 0x10) != 1) {
                    /* WARNING: Subroutine does not return */
        FUN_1409cd700("D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp",0xa8e,
                      0x1420de850,puVar15);
      }
      iVar6 = (**(code **)(lVar3 + 0x18))(*param_3,local_50);
    }
    if (iVar6 == 0) goto LAB_140fd1dda;
    Gw2::Services::Msg::MsgUtil((longlong)pbVar1,uVar12);
    thunk_FUN_140fd6360(*(undefined4 *)(param_2 + 0x10),**(undefined4 **)(param_2 + 0x48),
                        *(undefined4 *)(*(longlong *)(*(undefined4 **)(param_2 + 0x48) + 2) + 0x10),
                        local_res10[0],uVar12);
LAB_140fd1cd8:
    param_2[0x48] = 0;
    param_2[0x49] = 0;
    param_2[0x4a] = 0;
    param_2[0x4b] = 0;
    param_2[0x4c] = 0;
    param_2[0x4d] = 0;
    param_2[0x4e] = 0;
    param_2[0x4f] = 0;
    bVar17 = *(int *)(param_2 + 0x94) == *(int *)(param_2 + 0xa0);
  } while( true );
}

