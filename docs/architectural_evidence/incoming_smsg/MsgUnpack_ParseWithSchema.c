// [OLD_NAME]: Msg_ParseAndDispatch_BuildArgs
ulonglong *
MsgUnpack::ParseWithSchema
          (uint *param_1,ulonglong *param_2,ushort *param_3,ulonglong *param_4,ulonglong param_5,
          uint *param_6,undefined4 *param_7,uint *param_8)

{
  byte *pbVar1;
  char cVar2;
  byte bVar3;
  ushort uVar4;
  uint uVar5;
  ulonglong uVar6;
  ulonglong uVar7;
  undefined8 uVar8;
  ushort *puVar9;
  longlong extraout_RAX;
  longlong extraout_RAX_00;
  longlong extraout_RAX_01;
  ulonglong *puVar10;
  longlong extraout_RAX_02;
  undefined1 *puVar11;
  int iVar12;
  uint uVar13;
  ulonglong uVar14;
  uint uVar15;
  ulonglong *puVar16;
  ulonglong *local_38 [2];
  
  local_38[0] = param_4;
  if (param_5 == 0) {
                    /* WARNING: Subroutine does not return */
    Arena::Core::Platform::Common::Log(2,0x20de790,0,param_4);
  }
  uVar15 = *param_1;
  if ((uVar15 == 0) || (uVar15 == 0x18)) {
    return param_4;
  }
  do {
    puVar16 = (ulonglong *)0x0;
    if ((*(int *)(&DAT_020de514 + (ulonglong)uVar15 * 8) != 0) &&
       (param_3 < (ushort *)((ulonglong)*(uint *)(&DAT_020de510 + (ulonglong)uVar15 * 8) + *param_2)
       )) {
      return (ulonglong *)0x0;
    }
    if (param_5 < (byte *)((ulonglong)*(uint *)(&DAT_020de510 + (ulonglong)uVar15 * 8) +
                          (longlong)param_4)) {
                    /* WARNING: Subroutine does not return */
      Arena::Core::Platform::Common::Log(2,0x20de7e0,param_5,0);
    }
    switch(uVar15) {
    case 1:
    case 3:
                    /* WARNING: Subroutine does not return */
      Reflect_GenericCopyDispatcher(param_4,(undefined8 *)*param_2,2);
    case 2:
      *(byte *)param_4 = *(byte *)*param_2;
      param_4 = (ulonglong *)((longlong)param_4 + 1);
      *param_2 = *param_2 + 1;
      local_38[0] = param_4;
      break;
    case 4:
      puVar16 = (ulonglong *)param_7;
      uVar8 = FUN_00fd4b40((longlong *)param_2,(byte *)param_3,(longlong *)local_38,param_7);
      param_4 = local_38[0];
      if ((int)uVar8 == 0) {
        return puVar16;
      }
      break;
    case 5:
    case 7:
    case 0x1a:
                    /* WARNING: Subroutine does not return */
      Reflect_GenericCopyDispatcher(param_4,(undefined8 *)*param_2,8);
    case 6:
    case 0x19:
                    /* WARNING: Subroutine does not return */
      Reflect_GenericCopyDispatcher(param_4,(undefined8 *)*param_2,4);
    case 8:
                    /* WARNING: Subroutine does not return */
      Reflect_GenericCopyDispatcher(param_4,(undefined8 *)*param_2,0xc);
    case 9:
    case 0xb:
                    /* WARNING: Subroutine does not return */
      Reflect_GenericCopyDispatcher(param_4,(undefined8 *)*param_2,0x10);
    case 10:
                    /* WARNING: Subroutine does not return */
      Reflect_GenericCopyDispatcher(param_4,(undefined8 *)*param_2,0x10);
    case 0xc:
      puVar10 = (ulonglong *)*param_2;
      uVar6 = puVar10[1];
      uVar7 = puVar10[3];
      uVar14 = puVar10[2];
      *param_4 = *puVar10;
      param_4[1] = uVar6;
      param_4[2] = uVar14;
      *(int *)(param_4 + 3) = (int)uVar7;
      param_4 = (ulonglong *)((longlong)param_4 + 0x1c);
      *param_2 = *param_2 + 0x1c;
      local_38[0] = param_4;
      break;
    case 0xd:
      iVar12 = 0;
      for (puVar9 = (ushort *)*param_2; puVar9 < param_3; puVar9 = puVar9 + 1) {
        if (*puVar9 == 0) goto LAB_00fd45ac;
        iVar12 = iVar12 + 1;
      }
      if (puVar9 == param_3) {
        return (ulonglong *)0x0;
      }
LAB_00fd45ac:
      if (iVar12 == -1) {
        return (ulonglong *)0x0;
      }
      uVar14 = (ulonglong)(iVar12 * 2 + 2);
      if ((ulonglong)param_1[4] * 2 < uVar14) goto LAB_00fd4a95;
      if (param_3 < (ushort *)(*param_2 + uVar14)) {
        return (ulonglong *)0x0;
      }
      *param_4 = *param_2;
      param_4 = param_4 + 1;
      *param_2 = *param_2 + uVar14;
      local_38[0] = param_4;
      break;
    case 0xe:
      iVar12 = 0;
      for (puVar9 = (ushort *)*param_2; puVar9 < param_3; puVar9 = (ushort *)((longlong)puVar9 + 1))
      {
        if ((char)*puVar9 == '\0') goto LAB_00fd4609;
        iVar12 = iVar12 + 1;
      }
      if (puVar9 == param_3) {
        return (ulonglong *)0x0;
      }
LAB_00fd4609:
      if (iVar12 == -1) {
        return (ulonglong *)0x0;
      }
      uVar15 = iVar12 + 1;
      if (param_1[4] < uVar15) goto LAB_00fd4a95;
      if (param_3 < (ushort *)((ulonglong)uVar15 + *param_2)) {
        return (ulonglong *)0x0;
      }
      *param_4 = *param_2;
      param_4 = param_4 + 1;
      *param_2 = *param_2 + (ulonglong)uVar15;
      local_38[0] = param_4;
      break;
    case 0xf:
      puVar9 = (ushort *)((char *)*param_2 + 1);
      if (param_3 < puVar9) {
        return (ulonglong *)0x0;
      }
      cVar2 = *(char *)*param_2;
      puVar10 = (ulonglong *)0x0;
      *param_2 = (ulonglong)puVar9;
      if (cVar2 != '\0') {
        uVar15 = param_1[8];
        if (uVar15 == 0) goto LAB_00fd4a95;
        puVar10 = (ulonglong *)Arena::Core::Collections::Array(param_6,uVar15);
        puVar16 = ParseWithSchema(*(uint **)(param_1 + 6),param_2,param_3,puVar10,
                                  (longlong)puVar10 + (ulonglong)uVar15,param_6,param_7,(uint *)0x0)
        ;
        if (extraout_RAX == 0) {
          return puVar16;
        }
      }
      *param_4 = (ulonglong)puVar10;
      param_4 = param_4 + 1;
      local_38[0] = param_4;
      break;
    case 0x10:
      if (param_1[8] == 0) goto LAB_00fd4a95;
      uVar15 = param_1[8] * param_1[4];
      puVar10 = (ulonglong *)Arena::Core::Collections::Array(param_6,uVar15);
      uVar14 = (ulonglong)uVar15 + (longlong)puVar10;
      *param_4 = (ulonglong)puVar10;
      uVar15 = 0;
      if (param_1[4] != 0) {
        do {
          puVar16 = ParseWithSchema(*(uint **)(param_1 + 6),param_2,param_3,puVar10,uVar14,param_6,
                                    param_7,(uint *)0x0);
          if (extraout_RAX_00 == 0) {
            return puVar16;
          }
          uVar15 = uVar15 + 1;
          puVar10 = (ulonglong *)((longlong)puVar10 + (ulonglong)param_1[8]);
        } while (uVar15 < param_1[4]);
      }
LAB_00fd4738:
      param_4 = param_4 + 1;
      local_38[0] = param_4;
      break;
    case 0x11:
      if (param_3 < (byte *)*param_2 + 1) {
        return (ulonglong *)0x0;
      }
      if (param_1[8] != 0) {
        bVar3 = *(byte *)*param_2;
        *(byte *)param_4 = bVar3;
        uVar15 = (uint)bVar3;
        if (uVar15 <= param_1[4]) {
          *param_2 = *param_2 + 1;
          uVar5 = param_1[8];
          puVar10 = (ulonglong *)Arena::Core::Collections::Array(param_6,uVar15 * uVar5);
          *(ulonglong **)((longlong)param_4 + 1) = puVar10;
          uVar13 = 0;
          local_38[0] = (ulonglong *)((ulonglong)(uVar15 * uVar5) + (longlong)puVar10);
          if (uVar15 != 0) {
            do {
              puVar16 = ParseWithSchema(*(uint **)(param_1 + 6),param_2,param_3,puVar10,
                                        (ulonglong)local_38[0],param_6,param_7,(uint *)0x0);
              if (extraout_RAX_01 == 0) {
                return puVar16;
              }
              uVar13 = uVar13 + 1;
              puVar10 = (ulonglong *)((longlong)puVar10 + (ulonglong)param_1[8]);
            } while (uVar13 < bVar3);
          }
LAB_00fd47ec:
          param_4 = (ulonglong *)((longlong)param_4 + 9);
          local_38[0] = param_4;
          break;
        }
      }
      goto LAB_00fd4a95;
    case 0x12:
      if (param_3 < (ushort *)*param_2 + 1) {
        return (ulonglong *)0x0;
      }
      if (param_1[8] != 0) {
        uVar4 = *(ushort *)*param_2;
        *(ushort *)param_4 = uVar4;
        uVar15 = (uint)uVar4;
        if (uVar15 <= param_1[4]) {
          *param_2 = *param_2 + 2;
          uVar5 = param_1[8];
          puVar10 = (ulonglong *)Arena::Core::Collections::Array(param_6,uVar15 * uVar5);
          *(ulonglong **)((longlong)param_4 + 2) = puVar10;
          uVar13 = 0;
          local_38[0] = (ulonglong *)((ulonglong)(uVar15 * uVar5) + (longlong)puVar10);
          if (uVar15 != 0) {
            do {
              puVar16 = ParseWithSchema(*(uint **)(param_1 + 6),param_2,param_3,puVar10,
                                        (ulonglong)local_38[0],param_6,param_7,(uint *)0x0);
              if (extraout_RAX_02 == 0) {
                return puVar16;
              }
              uVar13 = uVar13 + 1;
              puVar10 = (ulonglong *)((longlong)puVar10 + (ulonglong)param_1[8]);
            } while (uVar13 < uVar4);
          }
LAB_00fd489c:
          param_4 = (ulonglong *)((longlong)param_4 + 10);
          local_38[0] = param_4;
          break;
        }
      }
LAB_00fd4a95:
      *param_7 = 1;
LAB_00fd4aa7:
      return (ulonglong *)0x0;
    case 0x13:
      if (param_3 < (ushort *)((ulonglong)param_1[4] + *param_2)) {
        return (ulonglong *)0x0;
      }
      puVar11 = (undefined1 *)Arena::Core::Collections::Array(param_6,param_1[4]);
      uVar15 = 0;
      *param_4 = (ulonglong)puVar11;
      if (param_1[4] == 0) goto LAB_00fd4738;
      do {
        uVar15 = uVar15 + 1;
        *puVar11 = *(undefined1 *)*param_2;
        *param_2 = *param_2 + 1;
        puVar11 = puVar11 + 1;
      } while (uVar15 < param_1[4]);
      param_4 = param_4 + 1;
      local_38[0] = param_4;
      break;
    case 0x14:
      if (param_3 < (byte *)*param_2 + 1) {
        return (ulonglong *)0x0;
      }
      bVar3 = *(byte *)*param_2;
      *(byte *)param_4 = bVar3;
      uVar15 = (uint)bVar3;
      if ((ushort)param_1[4] < uVar15) goto LAB_00fd4a95;
      uVar14 = (ulonglong)uVar15;
      if (param_3 < (ushort *)(*param_2 + 1 + (ulonglong)bVar3)) {
        return (ulonglong *)0x0;
      }
      pbVar1 = (byte *)(*param_2 + 1);
      *param_2 = (ulonglong)pbVar1;
      uVar5 = param_1[4];
      if (((param_8 != (uint *)0x0) && (*param_8 = 0xffffffff, pbVar1 != (byte *)0x0)) &&
         ((uVar5 >> 0x10 & 1) != 0)) {
        *param_8 = (uint)*pbVar1;
      }
      puVar11 = (undefined1 *)Arena::Core::Collections::Array(param_6,uVar15);
      *(undefined1 **)((longlong)param_4 + 1) = puVar11;
      if (bVar3 == 0) goto LAB_00fd47ec;
      do {
        *puVar11 = *(undefined1 *)*param_2;
        *param_2 = *param_2 + 1;
        uVar14 = uVar14 - 1;
        puVar11 = puVar11 + 1;
      } while (uVar14 != 0);
      param_4 = (ulonglong *)((longlong)param_4 + 9);
      local_38[0] = param_4;
      break;
    case 0x15:
      if (param_3 < (ushort *)*param_2 + 1) {
        return (ulonglong *)0x0;
      }
      uVar4 = *(ushort *)*param_2;
      *(ushort *)param_4 = uVar4;
      uVar15 = (uint)uVar4;
      if ((ushort)param_1[4] < uVar15) goto LAB_00fd4a95;
      uVar14 = (ulonglong)uVar15;
      if (param_3 < (ushort *)(*param_2 + 2 + (ulonglong)uVar4)) {
        return (ulonglong *)0x0;
      }
      pbVar1 = (byte *)(*param_2 + 2);
      *param_2 = (ulonglong)pbVar1;
      uVar5 = param_1[4];
      if (((param_8 != (uint *)0x0) && (*param_8 = 0xffffffff, pbVar1 != (byte *)0x0)) &&
         ((uVar5 >> 0x10 & 1) != 0)) {
        *param_8 = (uint)*pbVar1;
      }
      puVar11 = (undefined1 *)Arena::Core::Collections::Array(param_6,uVar15);
      *(undefined1 **)((longlong)param_4 + 2) = puVar11;
      if (uVar4 == 0) goto LAB_00fd489c;
      do {
        *puVar11 = *(undefined1 *)*param_2;
        *param_2 = *param_2 + 1;
        uVar14 = uVar14 - 1;
        puVar11 = puVar11 + 1;
      } while (uVar14 != 0);
      param_4 = (ulonglong *)((longlong)param_4 + 10);
      local_38[0] = param_4;
      break;
    case 0x16:
      goto LAB_00fd4aa7;
    case 0x17:
      *(undefined4 *)param_4 = *(undefined4 *)*param_2;
      param_4 = (ulonglong *)((longlong)param_4 + 4);
      *param_2 = *param_2 + 4;
      local_38[0] = param_4;
    }
    uVar15 = param_1[10];
    param_1 = param_1 + 10;
    if (uVar15 == 0) {
      return puVar16;
    }
    if (uVar15 == 0x18) {
      return puVar16;
    }
  } while( true );
}

