// [OLD_NAME]: Gs2c_PostParseDispatcher
void Portal::DispatchMessage(longlong param_1,longlong param_2,undefined4 *param_3)

{
  byte bVar1;
  short sVar2;
  uint uVar3;
  char *pcVar4;
  code *pcVar5;
  undefined4 uVar6;
  longlong *plVar7;
  longlong *plVar8;
  int *piVar9;
  int *piVar10;
  longlong lVar11;
  longlong lVar12;
  longlong *plVar13;
  undefined8 uVar14;
  longlong lVar15;
  longlong lVar16;
  int iVar17;
  ulonglong uVar18;
  uint uVar19;
  int iVar20;
  undefined8 *puVar21;
  ushort *puVar22;
  ulonglong *puVar23;
  undefined *puVar24;
  undefined8 *puVar25;
  uint *puVar26;
  ushort *puVar27;
  ulonglong uVar28;
  longlong **pplVar29;
  undefined4 *puVar30;
  undefined1 auStackY_b8 [32];
  longlong *local_68;
  longlong *local_60;
  ulonglong uStack_58;
  undefined4 local_50;
  undefined4 local_4c;
  ulonglong local_48;
  
  local_48 = DAT_142551900 ^ (ulonglong)auStackY_b8;
  switch(*param_3) {
  case 0x2b:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142512470,param_3[2],
                        (ulonglong)(param_3 + 4));
    lVar15 = FUN_1409a4490();
    local_68 = (longlong *)CONCAT44(local_68._4_4_,(uint)*(byte *)(lVar16 + 2));
                    /* WARNING: Subroutine does not return */
    FUN_1402510e0((undefined4 *)(*(longlong *)(lVar15 + 0x1f0) + 0x88),&LAB_1402536b8,
                  (undefined4 *)(lVar16 + 4),(undefined4 *)&local_68);
  case 0x2c:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142512560,param_3[2],
                        (ulonglong)(param_3 + 4));
    FUN_140260250(param_1,lVar16);
    break;
  case 0x2d:
    puVar30 = param_3 + 4;
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_1425126d0,param_3[2],
                        (ulonglong)puVar30);
    puVar27 = *(ushort **)(lVar16 + 0x12);
    puVar21 = (undefined8 *)(lVar16 + 2);
    plVar13 = (longlong *)Gw2::Game::Portal::PlManager(param_1,puVar21,puVar27);
    if (plVar13 == (longlong *)0x0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    uVar18 = Gw2::Game::Portal::PlUtil((uint)*(byte *)(lVar16 + 0x23),puVar21,puVar27,puVar30);
    puVar22 = *(ushort **)(lVar16 + 0x1a);
    uVar28 = uVar18 & 0xffffffff;
    plVar8 = (longlong *)FUN_1402579b0(param_1,puVar22);
    if (plVar8 == (longlong *)0x0) {
      if ((int)uVar18 == 0) break;
      Gw2::Game::Portal::PlUtil((uint)*(byte *)(lVar16 + 0x22),puVar22,puVar27,puVar30);
      plVar8 = (longlong *)Gw2::Game::Portal::PlChannel(param_1,*(ushort **)(lVar16 + 0x1a));
      if (plVar8 == (longlong *)0x0) break;
    }
    uVar14 = 0;
    plVar7 = plVar13;
    Arena::Core::Collections::Array((longlong)(plVar8 + 1),plVar13,uVar28,0);
    if (*(longlong *)(lVar16 + 0x24) == 0) {
      uVar6 = 0;
      plVar7 = (longlong *)&DAT_141b61e10;
    }
    else {
      uVar14 = Gw2::Game::Portal::PlUtil
                         ((uint)*(byte *)(*(longlong *)(lVar16 + 0x24) + 0x10),plVar7,uVar28,uVar14)
      ;
      uVar6 = (undefined4)uVar14;
      plVar7 = *(longlong **)(lVar16 + 0x24);
    }
    local_60 = (longlong *)*plVar7;
    uStack_58 = plVar7[1];
    FUN_140262d10((longlong)(plVar8 + 1),(ulonglong)plVar13,(ulonglong *)&local_60,uVar6);
    iVar17 = (**(code **)(*plVar8 + 0x170))(plVar8,0);
    if (iVar17 != 0) {
      FUN_140256dc0(plVar8 + 1);
    }
    break;
  case 0x2e:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142512810,param_3[2],
                        (ulonglong)(param_3 + 4));
    lVar15 = Gw2::Game::Portal::PlManager
                       (param_1,(undefined8 *)(lVar16 + 0x1a),*(ushort **)(lVar16 + 0x2a));
    if (lVar15 == 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    lVar15 = FUN_1409a4490();
    FUN_14025f6e0(*(longlong *)(lVar15 + 0x1f0),lVar16);
    break;
  case 0x2f:
    uVar18 = (ulonglong)(uint)param_3[2];
    puVar30 = param_3 + 4;
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_1425129a0,param_3[2],
                        (ulonglong)puVar30);
    Gw2::Game::Portal::PlManager(param_1,lVar16,uVar18,puVar30);
    break;
  case 0x30:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142512ae0,param_3[2],
                        (ulonglong)(param_3 + 4));
    FUN_140260430(param_1,lVar16);
    break;
  case 0x31:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_14256c6f0,param_3[0x44],
                        (ulonglong)(param_3 + 0x45));
    lVar15 = FUN_1402579b0(param_1,(ushort *)(param_3 + 4));
    if (lVar15 != 0) {
      FUN_140ffc0a0(lVar15 + 0xac0,lVar16);
    }
    break;
  case 0x32:
    puVar30 = param_3 + 0x45;
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142511a70,param_3[0x44],
                        (ulonglong)puVar30);
    FUN_14025f130(param_1,(ushort *)(param_3 + 4),lVar16,puVar30);
    break;
  case 0x33:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142512e70,param_3[0x44],
                        (ulonglong)(param_3 + 0x45));
    plVar13 = (longlong *)FUN_1402579b0(param_1,(ushort *)(param_3 + 4));
    if (plVar13 != (longlong *)0x0) {
      Arena::Core::Collections::Array((longlong)(plVar13 + 1));
      if (*(int *)(lVar16 + 2) != 0) {
        lVar15 = FUN_1409a4490();
        local_68 = (longlong *)CONCAT44(local_68._4_4_,*(undefined4 *)(lVar16 + 2));
        local_60 = plVar13;
        MsgQueue_Enqueue((undefined4 *)(*(longlong *)(lVar15 + 0x1f0) + 0x88),FUN_1402536b0,
                         (undefined4 *)&local_68,&local_60);
      }
      iVar17 = (**(code **)(*plVar13 + 0x170))(plVar13,0);
      if (iVar17 != 0) {
        FUN_140256dc0(plVar13 + 1);
      }
    }
    break;
  case 0x34:
    lVar15 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142512210,param_3[0x44],
                        (ulonglong)(param_3 + 0x45));
    lVar16 = *(longlong *)(lVar15 + 0x12);
    lVar11 = FUN_1402579b0(param_1,(ushort *)(param_3 + 4));
    if (lVar11 != 0) {
      local_60 = *(longlong **)(lVar15 + 2);
      uStack_58 = *(ulonglong *)(lVar15 + 10);
      plVar13 = (longlong *)Arena::Core::Collections::Array(param_1 + 400,(int *)&local_60);
      if ((plVar13 != (longlong *)0x0) && (lVar16 != 0)) {
        piVar9 = (int *)(**(code **)(*plVar13 + 0x130))(plVar13);
        plVar8 = *(longlong **)(lVar11 + 0xaa8);
        plVar13 = plVar8 + *(uint *)(lVar11 + 0xab4);
        for (; plVar8 != plVar13; plVar8 = plVar8 + 1) {
          piVar10 = (int *)(**(code **)(**(longlong **)(*plVar8 + 0x78) + 0x130))();
          if (((*piVar10 == *piVar9) && (piVar10[1] == piVar9[1])) &&
             ((piVar10[2] == piVar9[2] && (piVar10[3] == piVar9[3])))) goto LAB_14025deec;
        }
      }
    }
    break;
  case 0x35:
    lVar15 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142512ef0,param_3[2],
                        (ulonglong)(param_3 + 4));
    lVar16 = FUN_1409a4490();
    lVar16 = *(longlong *)(lVar16 + 0x1f0);
    plVar13 = (longlong *)FUN_1402579b0(lVar16 + 0x1d0,*(ushort **)(lVar15 + 2));
    local_68 = plVar13;
    if (plVar13 != (longlong *)0x0) {
      local_60 = *(longlong **)(lVar15 + 10);
      uStack_58 = *(ulonglong *)(lVar15 + 0x12);
      plVar8 = (longlong *)Arena::Core::Collections::Array(lVar16 + 0x360,(int *)&local_60);
      local_60 = plVar8;
      if ((plVar8 == (longlong *)0x0) ||
         (((*(byte *)(plVar8 + 0x2e) & 8) == 0 &&
          ((bVar1 = *(byte *)(lVar15 + 0x32), uVar19 = (**(code **)(*plVar8 + 0x238))(plVar8),
           (bVar1 & 1) != 0 ||
           (((iVar17 = (**(code **)(*plVar13 + 0x138))(plVar13), iVar17 != 6 &&
             (iVar17 = (**(code **)(*plVar13 + 0x138))(plVar13), iVar17 != 7)) || (0x1d < uVar19))))
          )))) {
        if (plVar8 != (longlong *)0x0) {
          MsgQueue_Enqueue_C((undefined4 *)(lVar16 + 0xe8),Vtbl_Invoke_B8_Slot,&local_68,&local_60,
                             (undefined8 *)(lVar15 + 0x2a));
        }
        MsgQueue_Enqueue_B((undefined4 *)(lVar16 + 0xe8),&LAB_140253684,&local_68,lVar15 + 10,
                           (undefined8 *)(lVar15 + 0x1a),(undefined8 *)(lVar15 + 0x22),
                           (undefined8 *)(lVar15 + 0x2a),(undefined4 *)(lVar15 + 0x33),
                           (undefined1 *)(lVar15 + 0x37));
      }
    }
    break;
  case 0x36:
    lVar15 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142513080,param_3[2],
                        (ulonglong)(param_3 + 4));
    lVar16 = FUN_1409a4490();
    lVar16 = *(longlong *)(lVar16 + 0x1f0);
    local_60 = (longlong *)FUN_1402579b0(lVar16 + 0x1d0,*(ushort **)(lVar15 + 2));
    if (local_60 != (longlong *)0x0) {
                    /* WARNING: Subroutine does not return */
      MsgQueue_Enqueue_A((undefined4 *)(lVar16 + 0xe8),&LAB_14025368c,&local_60,lVar15 + 10);
    }
    break;
  case 0x37:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_14256c6f0,param_3[8],
                        (ulonglong)(param_3 + 9));
    if (lVar16 == 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    lVar15 = Arena::Core::Collections::Array(param_1 + 0x90,(ulonglong *)(param_3 + 4));
    if (lVar15 != 0) {
      FUN_140ffc0a0(lVar15 + 0x20,lVar16);
    }
    break;
  case 0x38:
    iVar17 = param_3[8];
    if ((param_3 + 9 == (undefined4 *)0x0) || (iVar17 == 0)) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    lVar16 = Arena::Core::Collections::Array(param_1 + 0x90,(ulonglong *)(param_3 + 4));
    if (lVar16 != 0) {
      if (lVar16 == -8) {
        lVar16 = 0;
      }
      FUN_140275e60(lVar16,param_3 + 9,iVar17);
    }
    break;
  case 0x39:
    iVar17 = param_3[8];
    if ((param_3 + 9 == (undefined4 *)0x0) || (iVar17 == 0)) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    lVar16 = Arena::Core::Collections::Array(param_1 + 0x90,(ulonglong *)(param_3 + 4));
    if (lVar16 != 0) {
      if (lVar16 == -8) {
        lVar16 = 0;
      }
      FUN_140275ef0(lVar16,param_3 + 9,iVar17);
    }
    break;
  case 0x3a:
    uVar18 = (ulonglong)(uint)param_3[8];
    puVar30 = param_3 + 9;
    puVar24 = &DAT_142513fa0;
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142513fa0,param_3[8],
                        (ulonglong)puVar30);
    Gw2::Game::Portal::PlUtil((uint)*(byte *)(lVar16 + 10),puVar24,uVar18,puVar30);
    lVar15 = Arena::Core::Collections::Array(param_1 + 0x90,(ulonglong *)(param_3 + 4));
    if (lVar15 == 0) {
      lVar15 = FUN_140274c50();
    }
    *(uint *)(lVar15 + 0x198) = *(uint *)(lVar15 + 0x198) | 1;
    if (*(byte *)(lVar16 + 0x15) != 0) {
      puVar26 = Flags_DecodeOptions((uint *)&local_68,(uint)*(byte *)(lVar16 + 0x15));
      uVar19 = *puVar26;
      *(uint *)(lVar15 + 0x198) = *(uint *)(lVar15 + 0x198) | 2;
      if ((uVar19 & 1) != 0) {
        FUN_140ffb580(lVar15 + 0x20);
      }
      puVar25 = *(undefined8 **)(lVar15 + 0x178);
      puVar21 = puVar25 + *(uint *)(lVar15 + 0x184);
      for (; puVar25 != puVar21; puVar25 = puVar25 + 1) {
        (**(code **)(*(longlong *)*puVar25 + 0x10))((longlong *)*puVar25,uVar19);
      }
      lVar11 = lVar15 + 8;
      if (lVar15 + 8 != 0) {
        lVar11 = lVar15;
      }
      PreApply_Cleanup(lVar11,uVar19);
    }
    if (*(longlong *)(lVar16 + 0xd) != 0) {
      FUN_140260970();
    }
    puVar30 = *(undefined4 **)(lVar16 + 0x16);
    if (puVar30 != (undefined4 *)0x0) {
      *(undefined4 *)(lVar15 + 0x23c) = puVar30[1];
      *(undefined4 *)(lVar15 + 0x240) = puVar30[3];
      *(undefined4 *)(lVar15 + 0x244) = *puVar30;
      *(undefined4 *)(lVar15 + 0x248) = puVar30[2];
    }
    break;
  case 0x3b:
    pplVar29 = (longlong **)(param_3 + 9);
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142514160,param_3[8],
                        (ulonglong)pplVar29);
    FUN_14025f8d0(param_1,(ulonglong *)(param_3 + 4),lVar16,pplVar29);
    break;
  case 0x3c:
    lVar15 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142512210,param_3[8],
                        (ulonglong)(param_3 + 9));
    lVar16 = *(longlong *)(lVar15 + 0x12);
    lVar11 = Arena::Core::Collections::Array(param_1 + 0x90,(ulonglong *)(param_3 + 4));
    if (lVar11 != 0) {
      local_60 = *(longlong **)(lVar15 + 2);
      uStack_58 = *(ulonglong *)(lVar15 + 10);
      plVar13 = (longlong *)Arena::Core::Collections::Array(param_1 + 400,(int *)&local_60);
      if ((plVar13 != (longlong *)0x0) && (lVar16 != 0)) {
        piVar9 = (int *)(**(code **)(*plVar13 + 0x130))(plVar13);
        plVar8 = *(longlong **)(lVar11 + 0x178);
        plVar13 = plVar8 + *(uint *)(lVar11 + 0x184);
        for (; plVar8 != plVar13; plVar8 = plVar8 + 1) {
          piVar10 = (int *)(**(code **)(**(longlong **)(*plVar8 + 0x78) + 0x130))();
          if ((((*piVar10 == *piVar9) && (piVar10[1] == piVar9[1])) && (piVar10[2] == piVar9[2])) &&
             (piVar10[3] == piVar9[3])) goto LAB_14025deec;
        }
      }
    }
    break;
  case 0x3d:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_1425140e0,param_3[8],
                        (ulonglong)(param_3 + 9));
    lVar15 = Arena::Core::Collections::Array(param_1 + 0x90,(ulonglong *)(param_3 + 4));
    if (lVar15 != 0) {
      puVar25 = *(undefined8 **)(lVar16 + 3);
      puVar21 = (undefined8 *)((longlong)puVar25 + (ulonglong)*(byte *)(lVar16 + 2) * 0x24);
      if (puVar25 != puVar21) {
        do {
          plVar13 = (longlong *)Gw2::Game::Portal::PlManager(param_1,puVar25,(ushort *)puVar25[2]);
          if (plVar13 == (longlong *)0x0) {
                    /* WARNING: Subroutine does not return */
            FUN_1409cd550();
          }
          local_60 = (longlong *)puVar25[3];
          iVar17 = 0;
          if (local_60 != (longlong *)0x0) {
            sVar2 = (short)*local_60;
            uVar18 = 0;
            uVar19 = 0;
            while (sVar2 != 0) {
              uVar19 = (int)uVar18 + 1;
              uVar18 = (ulonglong)uVar19;
              sVar2 = *(short *)((longlong)local_60 + uVar18 * 2);
            }
            iVar17 = uVar19 + 1;
          }
          uStack_58 = CONCAT44(uStack_58._4_4_,iVar17);
          Table_InstallOrRefresh(lVar15 + 8,plVar13,&local_60,*(uint *)(puVar25 + 4));
          puVar25 = (undefined8 *)((longlong)puVar25 + 0x24);
        } while (puVar25 != puVar21);
      }
    }
    break;
  case 0x3e:
    iVar17 = param_3[0x10];
    plVar13 = (longlong *)(param_3 + 8);
    if ((param_3 + 0x11 == (undefined4 *)0x0) || (iVar17 == 0)) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    if (plVar13 == (longlong *)0x0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    lVar16 = Arena::Core::Collections::Array(param_1 + 0x90,(ulonglong *)(param_3 + 4));
    if (lVar16 != 0) {
      uVar19 = 0;
      iVar20 = 0;
      if (plVar13 != (longlong *)0x0) {
        sVar2 = (short)*plVar13;
        uVar18 = 0;
        while (sVar2 != 0) {
          uVar19 = (int)uVar18 + 1;
          uVar18 = (ulonglong)uVar19;
          sVar2 = *(short *)((longlong)plVar13 + uVar18 * 2);
        }
        iVar20 = uVar19 + 1;
      }
      uStack_58 = CONCAT44(uStack_58._4_4_,iVar20);
      local_60 = plVar13;
      uVar18 = FUN_140257fa0(lVar16 + 0x1a0,(longlong)&local_60);
      if (uVar18 == 0) {
                    /* WARNING: Subroutine does not return */
        FUN_1409cd550();
      }
      if (uVar18 == 0xfffffffffffffff8) {
        uVar18 = 0;
      }
      FUN_14026c020(uVar18,param_3 + 0x11,iVar17);
    }
    break;
  case 0x3f:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142511600,param_3[2],
                        (ulonglong)(param_3 + 4));
    local_60 = (longlong *)0x0;
    uStack_58 = uStack_58 & 0xffffffff00000000;
    pcVar4 = *(char **)(lVar16 + 2);
    local_50 = CONCAT22(local_50._2_2_,1);
    uVar6 = FUN_1409a8e80();
    local_50 = CONCAT22((short)uVar6,(undefined2)local_50);
    local_4c = 0;
    ManagedString_Build_FromAnsiLen1((longlong *)&local_60,pcVar4,(char *)0xffffffff,0);
    iVar17 = 1;
    goto LAB_14025e618;
  case 0x40:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142511860,param_3[2],
                        (ulonglong)(param_3 + 4));
    local_60 = (longlong *)0x0;
    uStack_58 = uStack_58 & 0xffffffff00000000;
    pcVar4 = *(char **)(lVar16 + 2);
    local_50 = CONCAT22(local_50._2_2_,1);
    uVar6 = FUN_1409a8e80();
    local_50 = CONCAT22((short)uVar6,(undefined2)local_50);
    local_4c = 0;
    ManagedString_Build_FromAnsiLen1((longlong *)&local_60,pcVar4,(char *)0xffffffff,0);
    iVar17 = 2;
LAB_14025e618:
    MsgConn_DispatchString(param_1,&local_60,iVar17,param_3[2],param_3 + 4);
    if (local_60 != (longlong *)0x0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409bf4b0((longlong)local_60);
    }
    break;
  default:
    **(undefined4 **)(param_2 + 0x18) = 1;
    break;
  case 0x42:
    uVar14 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142513400,param_3[2],
                        (ulonglong)(param_3 + 4));
    lVar16 = FUN_1409a4490();
                    /* WARNING: Subroutine does not return */
    FUN_140250a10((undefined4 *)(*(longlong *)(lVar16 + 0x1f0) + 0x88),&LAB_1402536e8,uVar14);
  case 0x43:
    uVar14 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142513570,param_3[2],
                        (ulonglong)(param_3 + 4));
    lVar16 = FUN_1409a4490();
                    /* WARNING: Subroutine does not return */
    FUN_140250a10((undefined4 *)(*(longlong *)(lVar16 + 0x1f0) + 0x88),&LAB_1402536f0,uVar14);
  case 0x44:
    uVar18 = (ulonglong)(uint)param_3[2];
    puVar30 = param_3 + 4;
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142513c50,param_3[2],
                        (ulonglong)puVar30);
    puVar23 = (ulonglong *)(lVar16 + 2);
    lVar15 = Table_GetOrFindByKey(param_1 + 0x128,puVar23);
    if (lVar15 == 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    if (*(longlong *)(lVar16 + 0x12) == 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    puVar27 = *(ushort **)(*(longlong *)(lVar16 + 0x12) + 2);
    uVar18 = Selector_MapOrValidate
                       ((uint)*(byte *)(*(longlong *)(lVar16 + 0x12) + 0x1e),puVar23,uVar18,puVar30)
    ;
    plVar13 = (longlong *)
              Handle_ConstructFromTuple
                        (param_1,(ulonglong *)(*(longlong *)(lVar16 + 0x12) + 10),
                         uVar18 & 0xffffffff,puVar27);
    lVar11 = Table_InsertEntry(lVar15 + 0x10,plVar13,
                               *(undefined4 *)(*(longlong *)(lVar16 + 0x12) + 0x1a));
    uVar6 = *(undefined4 *)(lVar16 + 0x1a);
    lVar12 = FUN_14024fd60((uint *)(lVar11 + 0xb0),(int *)(lVar16 + 0x27));
    *(undefined4 *)(lVar12 + 0x20) = uVar6;
    Arena::Core::Collections::Array
              (lVar11 + 8,(int *)(lVar16 + 0x27),*(undefined8 **)(lVar16 + 0x1f),
               (ulonglong)*(byte *)(lVar16 + 0x1e));
    if (lVar15 + 0x10 == 0) {
      lVar15 = 0;
    }
    FUN_14025ccc0((longlong)(plVar13 + 1),lVar15,lVar11);
    break;
  case 0x45:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142513d70,param_3[2],
                        (ulonglong)(param_3 + 4));
    lVar15 = Table_GetOrFindByKey(param_1 + 0x128,(ulonglong *)(lVar16 + 0x16));
    if (lVar15 == 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    local_60 = *(longlong **)(lVar16 + 2);
    uStack_58 = *(ulonglong *)(lVar16 + 10);
    local_50 = *(undefined4 *)(lVar16 + 0x12);
    lVar15 = Arena::Core::Collections::Array(lVar15 + 0x50,(uint *)&local_60);
    if (lVar15 == 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    if (*(longlong *)(lVar16 + 0x26) == 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    FUN_140ffc0a0(lVar15 + 0x48,*(longlong *)(lVar16 + 0x26));
    break;
  case 0x46:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142513bb0,param_3[2],
                        (ulonglong)(param_3 + 4));
    lVar15 = Table_GetOrFindByKey(param_1 + 0x128,(ulonglong *)(lVar16 + 2));
    if (lVar15 == 0) {
      lVar15 = Table_CreateIfMissing(param_1 + 0x128,(ulonglong *)(lVar16 + 2));
    }
    FUN_140265260(lVar15 + 0x10);
    puVar26 = (uint *)(lVar15 + 0x98);
    *(undefined4 *)(lVar15 + 0x9c) = 0;
    if (*(undefined1 (**) [32])(lVar15 + 0xa0) != (undefined1 (*) [32])0x0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409a99f0(*(undefined1 (**) [32])(lVar15 + 0xa0),0,(ulonglong)*puVar26 * 0x18);
    }
    piVar10 = *(int **)(lVar16 + 0x13);
    piVar9 = piVar10 + (ulonglong)*(byte *)(lVar16 + 0x12) * 5;
    for (; piVar10 != piVar9; piVar10 = piVar10 + 5) {
      if (piVar10[4] != 0) {
        uVar18 = FUN_140250360(puVar26,piVar10,(uint *)&local_68);
        uVar18 = uVar18 & 0xffffffff;
        if (*(int *)(*(longlong *)(lVar15 + 0xa0) + 0x14 + uVar18 * 0x18) == 0) {
          if (*puVar26 * 2 <= (uint)((*(int *)(lVar15 + 0x9c) + 1) * 3)) {
            FUN_140265970(puVar26,*puVar26 * 2);
            uVar18 = FUN_140250360(puVar26,piVar10,(uint *)&local_68);
            uVar18 = uVar18 & 0xffffffff;
          }
          *(undefined4 *)(*(longlong *)(lVar15 + 0xa0) + 0x14 + uVar18 * 0x18) = local_68._0_4_;
          *(int *)(lVar15 + 0x9c) = *(int *)(lVar15 + 0x9c) + 1;
        }
        uVar14 = *(undefined8 *)(piVar10 + 2);
        puVar21 = (undefined8 *)(*(longlong *)(lVar15 + 0xa0) + uVar18 * 0x18);
        *puVar21 = *(undefined8 *)piVar10;
        puVar21[1] = uVar14;
        *(int *)(*(longlong *)(lVar15 + 0xa0) + 0x10 + uVar18 * 0x18) = piVar10[4];
      }
    }
    break;
  case 0x47:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142513e60,param_3[2],
                        (ulonglong)(param_3 + 4));
    plVar13 = (longlong *)Table_GetOrFindByKey(param_1 + 0x128,(ulonglong *)(lVar16 + 2));
    if (plVar13 == (longlong *)0x0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    if ((plVar13[0xc] & 1U) == 0) {
      for (lVar15 = plVar13[0xc]; lVar15 != 0;
          lVar15 = *(longlong *)((longlong)(int)plVar13[10] + 8 + lVar15)) {
        FUN_140ffc4d0(lVar15 + 0x48);
        if ((*(ulonglong *)((longlong)(int)plVar13[10] + 8 + lVar15) & 1) != 0) break;
      }
    }
    if (*(int *)(lVar16 + 0x12) != 0) {
      lVar15 = FUN_1409a4490();
      local_68 = (longlong *)CONCAT44(local_68._4_4_,*(undefined4 *)(lVar16 + 0x12));
      local_60 = plVar13;
      MsgQueue_Enqueue((undefined4 *)(*(longlong *)(lVar15 + 0x1f0) + 0x88),&LAB_1402536f8,
                       (undefined4 *)&local_68,&local_60);
    }
    if (plVar13 == (longlong *)0xfffffffffffffff0) {
      plVar13 = (longlong *)0x0;
    }
    uVar18 = FUN_140277800((longlong)plVar13,(longlong *)0x0);
    if ((int)uVar18 != 0) {
                    /* WARNING: Subroutine does not return */
      _guard_check_icall(plVar13);
    }
    break;
  case 0x48:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142513a10,param_3[2],
                        (ulonglong)(param_3 + 4));
    FUN_14025fc00(param_1,lVar16);
    break;
  case 0x49:
    uVar18 = (ulonglong)(uint)param_3[2];
    puVar30 = param_3 + 4;
    puVar24 = &DAT_1425136e0;
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_1425136e0,param_3[2],
                        (ulonglong)puVar30);
    puVar27 = *(ushort **)(lVar16 + 2);
    uVar18 = Selector_MapOrValidate((uint)*(byte *)(lVar16 + 0x1e),puVar24,uVar18,puVar30);
    lVar16 = Handle_ConstructFromTuple
                       (param_1,(ulonglong *)(lVar16 + 10),uVar18 & 0xffffffff,puVar27);
    *(undefined4 *)(lVar16 + 100) = 0;
    if (*(undefined1 (**) [32])(lVar16 + 0x68) != (undefined1 (*) [32])0x0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409a99f0(*(undefined1 (**) [32])(lVar16 + 0x68),0,
                    (ulonglong)*(uint *)(lVar16 + 0x60) * 0x18);
    }
    break;
  case 0x4a:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_1425137d0,param_3[2],
                        (ulonglong)(param_3 + 4));
    plVar13 = (longlong *)Arena::Core::Collections::Array(param_1 + 0xe0,(ulonglong *)(lVar16 + 2));
    if (plVar13 == (longlong *)0x0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    iVar17 = *(int *)((longlong)plVar13 + 0x4c);
    uVar14 = FUN_140276fc0(plVar13,(longlong *)0x0);
    if (((int)uVar14 != 0) && (iVar17 == 0)) {
      FUN_140257000(plVar13 + 1);
    }
    break;
  case 0x4b:
    uVar14 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142513360,param_3[2],
                        (ulonglong)(param_3 + 4));
    lVar16 = FUN_1409a4490();
                    /* WARNING: Subroutine does not return */
    FUN_140250a10((undefined4 *)(*(longlong *)(lVar16 + 0x1f0) + 0x88),&LAB_140253684,uVar14);
  case 0x4c:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_142513f00,param_3[2],
                        (ulonglong)(param_3 + 4));
    lVar15 = Arena::Core::Collections::Array(param_1 + 0x1d8,(ulonglong *)(lVar16 + 2));
    if (lVar15 != 0) {
      uVar6 = *(undefined4 *)(lVar16 + 0x12);
      lVar16 = lVar15;
      if (lVar15 == -8) {
        lVar16 = 0;
      }
      FUN_140277ea0(lVar16,uVar6);
      *(undefined4 *)(lVar15 + 0x70) = uVar6;
    }
    break;
  case 0x4e:
    lVar16 = MsgConn::BuildArgsFromSchema
                       ((short *)(param_1 + 0x170),(uint *)&DAT_1425142a0,param_3[2],
                        (ulonglong)(param_3 + 4));
    if (lVar16 == 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    lVar15 = FUN_1409a4490();
    FUN_14025ef90(*(undefined8 *)(lVar15 + 0x1f0),lVar16);
    break;
  case 0x50:
    lVar16 = FUN_1409a4490();
    plVar13 = *(longlong **)(lVar16 + 0x1f0);
    if (*(int *)((longlong)plVar13 + 0x6e4) != 0) {
                    /* WARNING: Subroutine does not return */
      FUN_14024ff70((uint *)(plVar13 + 0xdc),param_3 + 4,(uint *)&local_68);
    }
    piVar10 = (int *)plVar13[0xc1];
    piVar9 = piVar10 + *(uint *)((longlong)plVar13 + 0x614);
    if (piVar10 < piVar9) {
      do {
        if (*piVar10 == param_3[4]) {
          uVar18 = (longlong)piVar10 - plVar13[0xc1] >> 2;
          uVar19 = (uint)uVar18;
          if (uVar19 != 0xffffffff) {
            uVar3 = *(uint *)((longlong)plVar13 + 0x614);
            local_68 = (longlong *)CONCAT44(local_68._4_4_,uVar19);
            if (uVar3 <= uVar19) {
              FUN_140233f00("D:\\Perforce\\Live\\NAEU\\v2\\Code\\Arena\\Core\\Collections\\Array.h",
                            0x531,0x1418fc310,(undefined4 *)&local_68,
                            (undefined4 *)((longlong)plVar13 + 0x614));
              pcVar5 = (code *)swi(3);
              (*pcVar5)();
              return;
            }
            if (uVar19 + 1 < uVar3) {
              *(undefined4 *)(plVar13[0xc1] + (uVar18 & 0xffffffff) * 4) =
                   *(undefined4 *)(plVar13[0xc1] + (ulonglong)(uVar3 - 1) * 4);
            }
            *(int *)((longlong)plVar13 + 0x614) = *(int *)((longlong)plVar13 + 0x614) + -1;
            FUN_14027a1f0((longlong)(plVar13 + 0xc4),param_3[4],(short *)(param_3 + 5));
            goto LAB_14025ee2d;
          }
          break;
        }
        piVar10 = piVar10 + 1;
      } while (piVar10 < piVar9);
    }
    Arena::Core::Collections::Array(plVar13,param_3[4],(short *)(param_3 + 5));
  }
LAB_14025ee2d:
                    /* WARNING: Subroutine does not return */
  FUN_140e37790(local_48 ^ (ulonglong)auStackY_b8);
LAB_14025deec:
  if (*plVar8 != 0) {
    FUN_140ffc0a0(*plVar8 + 8,lVar16);
  }
  goto LAB_14025ee2d;
}

