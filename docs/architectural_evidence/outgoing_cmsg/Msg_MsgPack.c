// [OLD_NAME]: MsgBuilder_ProcessSchema
/* WARNING: Type propagation algorithm not settling */

void Msg::MsgPack(undefined8 *param_1,longlong *param_2,uint *param_3,longlong *param_4,
                 ulonglong param_5,int *param_6,uint *param_7)

{
  uint *puVar1;
  ulonglong uVar2;
  longlong lVar3;
  short *psVar4;
  undefined1 *puVar5;
  byte *pbVar6;
  undefined2 *puVar7;
  undefined8 *puVar8;
  undefined1 *puVar9;
  byte *pbVar10;
  undefined2 *puVar11;
  uint uVar12;
  short *psVar13;
  char *pcVar14;
  undefined8 *puVar15;
  uint uVar16;
  uint uVar17;
  uint uVar18;
  undefined1 auStackY_138 [32];
  byte local_f8 [2];
  bool local_f6 [2];
  ushort local_f4 [2];
  undefined8 local_f0;
  longlong *local_e8;
  longlong local_d8;
  longlong lStack_d0;
  longlong *local_c8;
  longlong local_c0;
  longlong local_b8;
  longlong local_b0;
  longlong local_a8;
  uint *local_a0;
  undefined8 local_98 [5];
  undefined8 local_70;
  undefined8 uStack_68;
  undefined8 local_60;
  undefined4 local_58;
  ulonglong local_50;
  
  local_50 = DAT_142551900 ^ (ulonglong)auStackY_138;
  local_c8 = param_2;
  local_a0 = param_7;
  local_e8 = param_4;
  do {
    puVar1 = local_a0;
    uVar12 = 0;
    if ((param_5 != 0) &&
       (param_5 < (ulonglong)*(uint *)(&DAT_1420de510 + (ulonglong)*param_3 * 8) + *param_4)) {
      param_1[3] = 0x36f;
      param_1[1] = "Msg::MsgPack";
      *param_1 = "Bad size";
      param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
      param_1[4] = 0;
      goto LAB_140fd3ec6;
    }
    switch(*param_3) {
    case 1:
    case 3:
      lVar3 = *param_2;
      puVar15 = (undefined8 *)*param_4;
      if (lVar3 == 0) {
        psVar13 = (short *)param_2[1];
        FUN_140240a60(psVar13,*(int *)(psVar13 + 10) + 2);
                    /* WARNING: Subroutine does not return */
        thunk_FUN_1418c47c0((undefined8 *)
                            ((ulonglong)*(uint *)(psVar13 + 10) + *(longlong *)(psVar13 + 4)),
                            puVar15,2);
      }
      uVar12 = 1;
      while( true ) {
        uVar17 = ((int)lVar3 - *(int *)(lVar3 + 0xd0)) + 0x94cU >> 1;
        uVar18 = uVar12;
        if (uVar17 <= uVar12) {
          uVar18 = uVar17;
        }
        puVar8 = (undefined8 *)((ulonglong)(uVar18 * 2) + (longlong)puVar15);
        for (; puVar15 < puVar8; puVar15 = (undefined8 *)((longlong)puVar15 + 2)) {
          **(undefined2 **)(lVar3 + 0xd0) = *(undefined2 *)puVar15;
          *(longlong *)(lVar3 + 0xd0) = *(longlong *)(lVar3 + 0xd0) + 2;
        }
        if (uVar18 < uVar17) break;
        uVar12 = uVar12 - uVar18;
        MsgConn_FlushPacketBuffer();
      }
      *local_e8 = *local_e8 + 2;
      *param_6 = *param_6 + 2;
      param_4 = local_e8;
      break;
    case 2:
      puVar15 = (undefined8 *)*param_4;
      if (*param_2 == 0) {
        psVar13 = (short *)param_2[1];
        FUN_140240a60(psVar13,*(int *)(psVar13 + 10) + 1);
                    /* WARNING: Subroutine does not return */
        thunk_FUN_1418c47c0((undefined8 *)
                            ((ulonglong)*(uint *)(psVar13 + 10) + *(longlong *)(psVar13 + 4)),
                            puVar15,1);
      }
      MsgConn_WriteToPacketBuffer(*param_2,1,puVar15,1);
      *param_4 = *param_4 + 1;
      *param_6 = *param_6 + 1;
      break;
    case 4:
      local_d8 = *param_2;
      lStack_d0 = param_2[1];
      MsgConn_WriteCompressedInt(&local_d8,param_4,param_6,0);
      break;
    case 5:
      lVar3 = *param_2;
      puVar15 = (undefined8 *)*param_4;
      if (lVar3 == 0) {
LAB_140fd2eb0:
        psVar13 = (short *)param_2[1];
        FUN_140240a60(psVar13,*(int *)(psVar13 + 10) + 8);
                    /* WARNING: Subroutine does not return */
        thunk_FUN_1418c47c0((undefined8 *)
                            ((ulonglong)*(uint *)(psVar13 + 10) + *(longlong *)(psVar13 + 4)),
                            puVar15,8);
      }
      uVar12 = 1;
      while( true ) {
        uVar17 = ((int)lVar3 - *(int *)(lVar3 + 0xd0)) + 0x94cU >> 3;
        uVar18 = uVar12;
        if (uVar17 <= uVar12) {
          uVar18 = uVar17;
        }
        puVar8 = (undefined8 *)((ulonglong)(uVar18 * 8) + (longlong)puVar15);
        for (; puVar15 < puVar8; puVar15 = puVar15 + 1) {
          **(undefined8 **)(lVar3 + 0xd0) = *puVar15;
          *(longlong *)(lVar3 + 0xd0) = *(longlong *)(lVar3 + 0xd0) + 8;
        }
        if (uVar18 < uVar17) break;
        uVar12 = uVar12 - uVar18;
        MsgConn_FlushPacketBuffer();
      }
      goto LAB_140fd2ee9;
    case 6:
      lVar3 = *param_2;
      puVar15 = (undefined8 *)*param_4;
      if (lVar3 == 0) {
LAB_140fd2f6e:
        psVar13 = (short *)param_2[1];
        FUN_140240a60(psVar13,*(int *)(psVar13 + 10) + 4);
                    /* WARNING: Subroutine does not return */
        thunk_FUN_1418c47c0((undefined8 *)
                            ((ulonglong)*(uint *)(psVar13 + 10) + *(longlong *)(psVar13 + 4)),
                            puVar15,4);
      }
      uVar12 = 1;
      while( true ) {
        uVar17 = ((int)lVar3 - *(int *)(lVar3 + 0xd0)) + 0x94cU >> 2;
        uVar18 = uVar12;
        if (uVar17 <= uVar12) {
          uVar18 = uVar17;
        }
        puVar8 = (undefined8 *)((ulonglong)(uVar18 * 4) + (longlong)puVar15);
        for (; puVar15 < puVar8; puVar15 = (undefined8 *)((longlong)puVar15 + 4)) {
          **(undefined4 **)(lVar3 + 0xd0) = *(undefined4 *)puVar15;
          *(longlong *)(lVar3 + 0xd0) = *(longlong *)(lVar3 + 0xd0) + 4;
        }
        if (uVar18 < uVar17) break;
        uVar12 = uVar12 - uVar18;
        MsgConn_FlushPacketBuffer();
      }
      goto LAB_140fd2fa7;
    case 7:
      lVar3 = *param_2;
      puVar15 = (undefined8 *)*param_4;
      if (lVar3 == 0) goto LAB_140fd2eb0;
      uVar12 = 2;
      while( true ) {
        uVar17 = ((int)lVar3 - *(int *)(lVar3 + 0xd0)) + 0x94cU >> 2;
        uVar18 = uVar12;
        if (uVar17 <= uVar12) {
          uVar18 = uVar17;
        }
        puVar8 = (undefined8 *)((ulonglong)(uVar18 * 4) + (longlong)puVar15);
        for (; puVar15 < puVar8; puVar15 = (undefined8 *)((longlong)puVar15 + 4)) {
          **(undefined4 **)(lVar3 + 0xd0) = *(undefined4 *)puVar15;
          *(longlong *)(lVar3 + 0xd0) = *(longlong *)(lVar3 + 0xd0) + 4;
        }
        if (uVar18 < uVar17) break;
        uVar12 = uVar12 - uVar18;
        MsgConn_FlushPacketBuffer();
      }
      goto LAB_140fd2ee9;
    case 8:
      lVar3 = *param_2;
      puVar15 = (undefined8 *)*param_4;
      if (lVar3 == 0) {
        psVar13 = (short *)param_2[1];
        FUN_140240a60(psVar13,*(int *)(psVar13 + 10) + 0xc);
                    /* WARNING: Subroutine does not return */
        thunk_FUN_1418c47c0((undefined8 *)
                            ((ulonglong)*(uint *)(psVar13 + 10) + *(longlong *)(psVar13 + 4)),
                            puVar15,0xc);
      }
      uVar12 = 3;
      while( true ) {
        uVar17 = ((int)lVar3 - *(int *)(lVar3 + 0xd0)) + 0x94cU >> 2;
        uVar18 = uVar12;
        if (uVar17 <= uVar12) {
          uVar18 = uVar17;
        }
        puVar8 = (undefined8 *)((ulonglong)(uVar18 * 4) + (longlong)puVar15);
        for (; puVar15 < puVar8; puVar15 = (undefined8 *)((longlong)puVar15 + 4)) {
          **(undefined4 **)(lVar3 + 0xd0) = *(undefined4 *)puVar15;
          *(longlong *)(lVar3 + 0xd0) = *(longlong *)(lVar3 + 0xd0) + 4;
        }
        if (uVar18 < uVar17) break;
        uVar12 = uVar12 - uVar18;
        MsgConn_FlushPacketBuffer();
      }
      *local_e8 = *local_e8 + 0xc;
      *param_6 = *param_6 + 0xc;
      param_4 = local_e8;
      break;
    case 9:
      lVar3 = *param_2;
      puVar15 = (undefined8 *)*param_4;
      if (lVar3 == 0) {
LAB_140fd315e:
        psVar13 = (short *)param_2[1];
        FUN_140240a60(psVar13,*(int *)(psVar13 + 10) + 0x10);
                    /* WARNING: Subroutine does not return */
        thunk_FUN_1418c47c0((undefined8 *)
                            ((ulonglong)*(uint *)(psVar13 + 10) + *(longlong *)(psVar13 + 4)),
                            puVar15,0x10);
      }
      uVar12 = 4;
      while( true ) {
        uVar17 = ((int)lVar3 - *(int *)(lVar3 + 0xd0)) + 0x94cU >> 2;
        uVar18 = uVar12;
        if (uVar17 <= uVar12) {
          uVar18 = uVar17;
        }
        puVar8 = (undefined8 *)((ulonglong)(uVar18 * 4) + (longlong)puVar15);
        for (; puVar15 < puVar8; puVar15 = (undefined8 *)((longlong)puVar15 + 4)) {
          **(undefined4 **)(lVar3 + 0xd0) = *(undefined4 *)puVar15;
          *(longlong *)(lVar3 + 0xd0) = *(longlong *)(lVar3 + 0xd0) + 4;
        }
        if (uVar18 < uVar17) break;
        uVar12 = uVar12 - uVar18;
        MsgConn_FlushPacketBuffer();
      }
      *local_e8 = *local_e8 + 0x10;
      *param_6 = *param_6 + 0x10;
      param_4 = local_e8;
      break;
    case 10:
      lVar3 = *param_2;
      puVar15 = (undefined8 *)*param_4;
      if (lVar3 == 0) {
        psVar13 = (short *)param_2[1];
        FUN_140240a60(psVar13,*(int *)(psVar13 + 10) + 0xc);
                    /* WARNING: Subroutine does not return */
        thunk_FUN_1418c47c0((undefined8 *)
                            ((ulonglong)*(uint *)(psVar13 + 10) + *(longlong *)(psVar13 + 4)),
                            puVar15,0xc);
      }
      uVar12 = 3;
      while( true ) {
        param_4 = local_e8;
        uVar17 = ((int)lVar3 - *(int *)(lVar3 + 0xd0)) + 0x94cU >> 2;
        uVar18 = uVar12;
        if (uVar17 <= uVar12) {
          uVar18 = uVar17;
        }
        puVar8 = (undefined8 *)((ulonglong)(uVar18 * 4) + (longlong)puVar15);
        for (; puVar15 < puVar8; puVar15 = (undefined8 *)((longlong)puVar15 + 4)) {
          **(undefined4 **)(lVar3 + 0xd0) = *(undefined4 *)puVar15;
          *(longlong *)(lVar3 + 0xd0) = *(longlong *)(lVar3 + 0xd0) + 4;
        }
        if (uVar18 < uVar17) break;
        uVar12 = uVar12 - uVar18;
        MsgConn_FlushPacketBuffer();
      }
      local_d8 = *param_2;
      lStack_d0 = param_2[1];
      *local_e8 = *local_e8 + 0xc;
      *param_6 = *param_6 + 0xc;
      MsgConn_WriteCompressedInt(&local_d8,local_e8,param_6,1);
      break;
    case 0xb:
      puVar15 = (undefined8 *)*param_4;
      if (*param_2 == 0) goto LAB_140fd315e;
      MsgConn_WriteToPacketBuffer(*param_2,0x10,puVar15,1);
      *param_4 = *param_4 + 0x10;
      *param_6 = *param_6 + 0x10;
      break;
    case 0xc:
      puVar15 = (undefined8 *)*param_4;
      lVar3 = *param_2;
      local_70 = *puVar15;
      uStack_68 = puVar15[1];
      local_60 = puVar15[2];
      local_58 = *(undefined4 *)(puVar15 + 3);
      if (lVar3 == 0) {
        psVar13 = (short *)param_2[1];
        FUN_140240a60(psVar13,*(int *)(psVar13 + 10) + 0x1c);
                    /* WARNING: Subroutine does not return */
        thunk_FUN_1418c47c0((undefined8 *)
                            ((ulonglong)*(uint *)(psVar13 + 10) + *(longlong *)(psVar13 + 4)),
                            &local_70,0x1c);
      }
      uVar12 = 0x1c;
      puVar15 = &local_70;
      while( true ) {
        uVar17 = ((int)lVar3 - *(int *)(lVar3 + 0xd0)) + 0x94c;
        uVar18 = uVar12;
        if (uVar17 <= uVar12) {
          uVar18 = uVar17;
        }
        puVar8 = (undefined8 *)((ulonglong)uVar18 + (longlong)puVar15);
        for (; puVar15 < puVar8; puVar15 = (undefined8 *)((longlong)puVar15 + 1)) {
          **(undefined1 **)(lVar3 + 0xd0) = *(undefined1 *)puVar15;
          *(longlong *)(lVar3 + 0xd0) = *(longlong *)(lVar3 + 0xd0) + 1;
        }
        if (uVar18 < uVar17) break;
        uVar12 = uVar12 - uVar18;
        MsgConn_FlushPacketBuffer();
      }
      *local_e8 = *local_e8 + 0x1c;
      *param_6 = *param_6 + 0x1c;
      param_4 = local_e8;
      break;
    case 0xd:
      psVar13 = &DAT_1418fd248;
      if (*(short **)*param_4 != (short *)0x0) {
        psVar13 = *(short **)*param_4;
      }
      uVar2 = Arena::Core::Basics::Str(psVar13);
      uVar12 = (int)uVar2 + 1;
      if (param_3[4] < uVar12) {
        param_1[3] = 0x3c2;
        param_1[1] = "Msg::MsgPack";
        *param_1 = "MP_STRING too long";
        param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
        param_1[4] = 0;
        goto LAB_140fd3ec6;
      }
      lVar3 = *local_c8;
      uVar18 = uVar12;
      if (lVar3 == 0) {
        psVar4 = (short *)local_c8[1];
        FUN_140240a60(psVar4,*(int *)(psVar4 + 10) + uVar12 * 2);
                    /* WARNING: Subroutine does not return */
        thunk_FUN_1418c47c0((undefined8 *)
                            ((ulonglong)*(uint *)(psVar4 + 10) + *(longlong *)(psVar4 + 4)),
                            (undefined8 *)psVar13,(ulonglong)(uVar12 * 2));
      }
      while( true ) {
        uVar16 = ((int)lVar3 - *(int *)(lVar3 + 0xd0)) + 0x94cU >> 1;
        uVar17 = uVar18;
        if (uVar16 <= uVar18) {
          uVar17 = uVar16;
        }
        psVar4 = (short *)((ulonglong)(uVar17 * 2) + (longlong)psVar13);
        for (; psVar13 < psVar4; psVar13 = psVar13 + 1) {
          **(short **)(lVar3 + 0xd0) = *psVar13;
          *(longlong *)(lVar3 + 0xd0) = *(longlong *)(lVar3 + 0xd0) + 2;
        }
        if (uVar17 < uVar16) break;
        MsgConn_FlushPacketBuffer();
        uVar18 = uVar18 - uVar17;
      }
      *local_e8 = *local_e8 + 8;
      *param_6 = *param_6 + uVar12 * 2;
      param_4 = local_e8;
      param_2 = local_c8;
      break;
    case 0xe:
      pcVar14 = "";
      if (*(char **)*param_4 != (char *)0x0) {
        pcVar14 = *(char **)*param_4;
      }
      uVar2 = Arena::Core::Basics::Str(pcVar14);
      uVar12 = (int)uVar2 + 1;
      if (param_3[4] < uVar12) {
        param_1[3] = 0x3cf;
        *param_1 = "MP_CSTRING too long";
        param_1[1] = "Msg::MsgPack";
        param_1[4] = 0;
        param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
        goto LAB_140fd3ec6;
      }
      if (*param_2 == 0) {
        psVar13 = (short *)param_2[1];
        FUN_140240a60(psVar13,*(int *)(psVar13 + 10) + uVar12);
                    /* WARNING: Subroutine does not return */
        thunk_FUN_1418c47c0((undefined8 *)
                            ((ulonglong)*(uint *)(psVar13 + 10) + *(longlong *)(psVar13 + 4)),
                            (undefined8 *)pcVar14,(ulonglong)uVar12);
      }
      MsgConn_WriteToPacketBuffer(*param_2,uVar12,(undefined8 *)pcVar14,1);
      *local_e8 = *local_e8 + 8;
      *param_6 = *param_6 + uVar12;
      param_4 = local_e8;
      break;
    case 0xf:
      lVar3 = *param_2;
      local_c0 = *(longlong *)*param_4;
      *param_4 = (longlong)((longlong *)*param_4 + 1);
      local_f6[0] = local_c0 != 0;
      if (lVar3 == 0) {
        psVar13 = (short *)param_2[1];
        FUN_140240a60(psVar13,*(int *)(psVar13 + 10) + 1);
                    /* WARNING: Subroutine does not return */
        thunk_FUN_1418c47c0((undefined8 *)
                            ((ulonglong)*(uint *)(psVar13 + 10) + *(longlong *)(psVar13 + 4)),
                            (undefined8 *)local_f6,1);
      }
      uVar12 = 1;
      puVar9 = local_f6;
      while( true ) {
        param_4 = local_e8;
        uVar17 = ((int)lVar3 - *(int *)(lVar3 + 0xd0)) + 0x94c;
        uVar18 = uVar12;
        if (uVar17 <= uVar12) {
          uVar18 = uVar17;
        }
        puVar5 = puVar9 + uVar18;
        for (; puVar9 < puVar5; puVar9 = puVar9 + 1) {
          **(undefined1 **)(lVar3 + 0xd0) = *puVar9;
          *(longlong *)(lVar3 + 0xd0) = *(longlong *)(lVar3 + 0xd0) + 1;
        }
        if (uVar18 < uVar17) break;
        uVar12 = uVar12 - uVar18;
        MsgConn_FlushPacketBuffer();
      }
      *param_6 = *param_6 + 1;
      if (local_c0 != 0) {
        local_d8 = *param_2;
        lStack_d0 = param_2[1];
        lVar3 = MsgPack(local_98,&local_d8,*(uint **)(param_3 + 6),&local_c0,0,param_6,(uint *)0x0);
        if (*(int *)(lVar3 + 0x24) == 0) {
          param_1[3] = 0x3e6;
          param_1[1] = "Msg::MsgPack";
          *param_1 = "MP_OPTIONAL";
          param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
          param_1[4] = 0;
          goto LAB_140fd3ec6;
        }
      }
      break;
    case 0x10:
      local_b8 = *(longlong *)*param_4;
      *param_4 = (longlong)((longlong *)*param_4 + 1);
      if (param_3[4] != 0) {
        do {
          local_d8 = *param_2;
          lStack_d0 = param_2[1];
          lVar3 = MsgPack(local_98,&local_d8,*(uint **)(param_3 + 6),&local_b8,0,param_6,(uint *)0x0
                         );
          if (*(int *)(lVar3 + 0x24) == 0) {
            param_1[3] = 0x3f0;
            param_1[1] = "Msg::MsgPack";
            *param_1 = "MP_ARRAY_FIXED";
            param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
            param_1[4] = 0;
            goto LAB_140fd3ec6;
          }
          uVar12 = uVar12 + 1;
        } while (uVar12 < param_3[4]);
      }
      break;
    case 0x11:
      local_f8[0] = *(byte *)*param_4;
      if (param_3[4] < (uint)local_f8[0]) {
        param_1[3] = 0x3f8;
        param_1[1] = "Msg::MsgPack";
        *param_1 = "MP_ARRAY_VAR_SMALL";
        param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
        param_1[4] = 0;
        goto LAB_140fd3ec6;
      }
      lVar3 = *param_2;
      *param_4 = (longlong)((byte *)*param_4 + 1);
      if (lVar3 == 0) {
        FUN_140d7ac40((short *)param_2[1],(undefined8 *)local_f8,1);
      }
      else {
        uVar12 = 1;
        pbVar10 = local_f8;
        while( true ) {
          uVar17 = ((int)lVar3 - *(int *)(lVar3 + 0xd0)) + 0x94c;
          uVar18 = uVar12;
          if (uVar17 <= uVar12) {
            uVar18 = uVar17;
          }
          pbVar6 = pbVar10 + uVar18;
          for (; pbVar10 < pbVar6; pbVar10 = pbVar10 + 1) {
            **(byte **)(lVar3 + 0xd0) = *pbVar10;
            *(longlong *)(lVar3 + 0xd0) = *(longlong *)(lVar3 + 0xd0) + 1;
          }
          param_4 = local_e8;
          if (uVar18 < uVar17) break;
          uVar12 = uVar12 - uVar18;
          MsgConn_FlushPacketBuffer();
        }
      }
      uVar12 = 0;
      local_b0 = *(longlong *)*param_4;
      *param_6 = *param_6 + 1;
      *param_4 = *param_4 + 8;
      if (local_f8[0] != 0) {
        do {
          local_d8 = *param_2;
          lStack_d0 = param_2[1];
          lVar3 = MsgPack(local_98,&local_d8,*(uint **)(param_3 + 6),&local_b0,0,param_6,(uint *)0x0
                         );
          if (*(int *)(lVar3 + 0x24) == 0) {
            param_1[3] = 0x400;
            param_1[1] = "Msg::MsgPack";
            *param_1 = "MP_ARRAY_VAR_SMALL";
            param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
            param_1[4] = 0;
            goto LAB_140fd3ec6;
          }
          uVar12 = uVar12 + 1;
        } while (uVar12 < local_f8[0]);
      }
      break;
    case 0x12:
      local_f4[0] = *(ushort *)*param_4;
      if (param_3[4] < (uint)local_f4[0]) {
        param_1[3] = 0x408;
        param_1[1] = "Msg::MsgPack";
        *param_1 = "MP_ARRAY_VAR_LARGE";
        param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
        param_1[4] = 0;
        goto LAB_140fd3ec6;
      }
      lVar3 = *param_2;
      *param_4 = (longlong)((ushort *)*param_4 + 1);
      if (lVar3 == 0) {
        psVar13 = (short *)param_2[1];
        FUN_140240a60(psVar13,*(int *)(psVar13 + 10) + 2);
                    /* WARNING: Subroutine does not return */
        thunk_FUN_1418c47c0((undefined8 *)
                            ((ulonglong)*(uint *)(psVar13 + 10) + *(longlong *)(psVar13 + 4)),
                            (undefined8 *)local_f4,2);
      }
      uVar12 = 1;
      puVar11 = local_f4;
      while( true ) {
        param_4 = local_e8;
        uVar17 = ((int)lVar3 - *(int *)(lVar3 + 0xd0)) + 0x94cU >> 1;
        uVar18 = uVar12;
        if (uVar17 <= uVar12) {
          uVar18 = uVar17;
        }
        puVar7 = (undefined2 *)((ulonglong)(uVar18 * 2) + (longlong)puVar11);
        for (; puVar11 < puVar7; puVar11 = puVar11 + 1) {
          **(undefined2 **)(lVar3 + 0xd0) = *puVar11;
          *(longlong *)(lVar3 + 0xd0) = *(longlong *)(lVar3 + 0xd0) + 2;
        }
        if (uVar18 < uVar17) break;
        uVar12 = uVar12 - uVar18;
        MsgConn_FlushPacketBuffer();
      }
      uVar12 = 0;
      local_a8 = *(longlong *)*local_e8;
      *param_6 = *param_6 + 2;
      *local_e8 = *local_e8 + 8;
      if (local_f4[0] != 0) {
        do {
          local_d8 = *param_2;
          lStack_d0 = param_2[1];
          lVar3 = MsgPack(local_98,&local_d8,*(uint **)(param_3 + 6),&local_a8,0,param_6,(uint *)0x0
                         );
          if (*(int *)(lVar3 + 0x24) == 0) {
            param_1[3] = 0x410;
            param_1[1] = "Msg::MsgPack";
            *param_1 = "MP_ARRAY_VAR_LARGE";
            param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
            param_1[4] = 0;
            goto LAB_140fd3ec6;
          }
          uVar12 = uVar12 + 1;
        } while (uVar12 < local_f4[0]);
      }
      break;
    case 0x13:
      uVar12 = param_3[4];
      puVar15 = *(undefined8 **)*param_4;
      if (*param_2 == 0) {
        psVar13 = (short *)param_2[1];
        FUN_140240a60(psVar13,*(int *)(psVar13 + 10) + uVar12);
                    /* WARNING: Subroutine does not return */
        thunk_FUN_1418c47c0((undefined8 *)
                            ((ulonglong)*(uint *)(psVar13 + 10) + *(longlong *)(psVar13 + 4)),
                            puVar15,(ulonglong)uVar12);
      }
      MsgConn_WriteToPacketBuffer(*param_2,uVar12,puVar15,1);
      *local_e8 = *local_e8 + 8;
      *param_6 = *param_6 + param_3[4];
      param_4 = local_e8;
      break;
    case 0x14:
      local_f8[1] = *(byte *)*param_4;
      if ((ushort)param_3[4] < (ushort)local_f8[1]) {
        param_1[3] = 0x420;
        param_1[1] = "Msg::MsgPack";
        *param_1 = "MP_BUFFER_VAR_SMALL";
        param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
        param_1[4] = 0;
        goto LAB_140fd3ec6;
      }
      lVar3 = *param_2;
      *param_4 = (longlong)((byte *)*param_4 + 1);
      if (lVar3 == 0) {
        FUN_140d7ac40((short *)param_2[1],(undefined8 *)((longlong)local_f8 + 1),1);
      }
      else {
        MsgConn_WriteToPacketBuffer(lVar3,1,(undefined8 *)((longlong)local_f8 + 1),1);
      }
      pbVar10 = *(byte **)*param_4;
      *param_6 = *param_6 + 1;
      uVar12 = param_3[4];
      if (((puVar1 != (uint *)0x0) && (*puVar1 = 0xffffffff, pbVar10 != (byte *)0x0)) &&
         ((uVar12 >> 0x10 & 1) != 0)) {
        *puVar1 = (uint)*pbVar10;
      }
      WriteDataToBuffer(param_2,(uint)local_f8[1],(undefined8 *)pbVar10,1);
      *param_4 = *param_4 + 8;
      *param_6 = *param_6 + (uint)local_f8[1];
      break;
    case 0x15:
      local_f0._0_2_ = *(ushort *)*param_4;
      if ((ushort)param_3[4] < (ushort)local_f0) {
        param_1[3] = 0x42f;
        param_1[1] = "Msg::MsgPack";
        *param_1 = "MP_BUFFER_VAR_LARGE";
        param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
        param_1[4] = 0;
        goto LAB_140fd3ec6;
      }
      lVar3 = *param_2;
      *param_4 = (longlong)((ushort *)*param_4 + 1);
      if (lVar3 == 0) {
        FUN_140d7ac40((short *)param_2[1],&local_f0,2);
      }
      else {
        MsgConn_WriteToPacketBuffer(lVar3,1,&local_f0,2);
      }
      pbVar10 = *(byte **)*param_4;
      *param_6 = *param_6 + 2;
      uVar12 = param_3[4];
      if (((puVar1 != (uint *)0x0) && (*puVar1 = 0xffffffff, pbVar10 != (byte *)0x0)) &&
         ((uVar12 >> 0x10 & 1) != 0)) {
        *puVar1 = (uint)*pbVar10;
      }
      WriteDataToBuffer(param_2,(uint)(ushort)local_f0,(undefined8 *)pbVar10,1);
      *param_4 = *param_4 + 8;
      *param_6 = *param_6 + (uint)(ushort)local_f0;
      break;
    case 0x16:
      param_1[3] = 0x43d;
      param_1[1] = "Msg::MsgPack";
      *param_1 = "MP_SRV_ALIGN";
      param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
      param_1[4] = 0;
      goto LAB_140fd3ec6;
    case 0x17:
      lVar3 = *param_2;
      puVar15 = (undefined8 *)*param_4;
      if (lVar3 == 0) goto LAB_140fd2f6e;
      uVar12 = 1;
      while( true ) {
        uVar17 = ((int)lVar3 - *(int *)(lVar3 + 0xd0)) + 0x94cU >> 2;
        uVar18 = uVar12;
        if (uVar17 <= uVar12) {
          uVar18 = uVar17;
        }
        puVar8 = (undefined8 *)((ulonglong)(uVar18 * 4) + (longlong)puVar15);
        for (; puVar15 < puVar8; puVar15 = (undefined8 *)((longlong)puVar15 + 4)) {
          **(undefined4 **)(lVar3 + 0xd0) = *(undefined4 *)puVar15;
          *(longlong *)(lVar3 + 0xd0) = *(longlong *)(lVar3 + 0xd0) + 4;
        }
        if (uVar18 < uVar17) break;
        uVar12 = uVar12 - uVar18;
        MsgConn_FlushPacketBuffer();
      }
      goto LAB_140fd2fa7;
    case 0x18:
      if (*param_2 != 0) {
        param_1[3] = 0x44a;
        param_1[1] = "Msg::MsgPack";
        *param_1 = "MP_SRV_END";
        param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
        param_1[4] = 0;
        goto LAB_140fd3ec6;
      }
      uVar12 = -*(int *)((short *)param_2[1] + 10) & 3;
      if (uVar12 != 0) {
        FUN_140d7ac40((short *)param_2[1],(undefined8 *)&DAT_1420de69c,uVar12);
        *param_6 = *param_6 + uVar12;
      }
    case 0:
      *param_1 = 0;
      param_1[1] = 0;
      param_1[2] = 0;
      param_1[3] = 0;
      *(undefined4 *)(param_1 + 4) = 0;
      *(undefined4 *)((longlong)param_1 + 0x24) = 1;
LAB_140fd3ec6:
                    /* WARNING: Subroutine does not return */
      FUN_140e37790(local_50 ^ (ulonglong)auStackY_138);
    case 0x19:
      lVar3 = *param_2;
      puVar15 = (undefined8 *)*param_4;
      if (lVar3 == 0) goto LAB_140fd2f6e;
      uVar12 = 1;
      while( true ) {
        uVar17 = ((int)lVar3 - *(int *)(lVar3 + 0xd0)) + 0x94cU >> 2;
        uVar18 = uVar12;
        if (uVar17 <= uVar12) {
          uVar18 = uVar17;
        }
        puVar8 = (undefined8 *)((ulonglong)(uVar18 * 4) + (longlong)puVar15);
        for (; puVar15 < puVar8; puVar15 = (undefined8 *)((longlong)puVar15 + 4)) {
          **(undefined4 **)(lVar3 + 0xd0) = *(undefined4 *)puVar15;
          *(longlong *)(lVar3 + 0xd0) = *(longlong *)(lVar3 + 0xd0) + 4;
        }
        if (uVar18 < uVar17) break;
        uVar12 = uVar12 - uVar18;
        MsgConn_FlushPacketBuffer();
      }
LAB_140fd2fa7:
      *local_e8 = *local_e8 + 4;
      *param_6 = *param_6 + 4;
      param_4 = local_e8;
      break;
    case 0x1a:
      lVar3 = *param_2;
      puVar15 = (undefined8 *)*param_4;
      if (lVar3 == 0) goto LAB_140fd2eb0;
      uVar12 = 1;
      while( true ) {
        uVar17 = ((int)lVar3 - *(int *)(lVar3 + 0xd0)) + 0x94cU >> 3;
        uVar18 = uVar12;
        if (uVar17 <= uVar12) {
          uVar18 = uVar17;
        }
        puVar8 = (undefined8 *)((ulonglong)(uVar18 * 8) + (longlong)puVar15);
        for (; puVar15 < puVar8; puVar15 = puVar15 + 1) {
          **(undefined8 **)(lVar3 + 0xd0) = *puVar15;
          *(longlong *)(lVar3 + 0xd0) = *(longlong *)(lVar3 + 0xd0) + 8;
        }
        if (uVar18 < uVar17) break;
        uVar12 = uVar12 - uVar18;
        MsgConn_FlushPacketBuffer();
      }
LAB_140fd2ee9:
      *local_e8 = *local_e8 + 8;
      *param_6 = *param_6 + 8;
      param_4 = local_e8;
    }
    param_3 = param_3 + 10;
  } while( true );
}

