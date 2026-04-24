
void Reflect_TypeDecoder(undefined8 *param_1,undefined8 *param_2,ulonglong *param_3,uint param_4)

{
  byte bVar1;
  undefined2 uVar2;
  undefined2 uVar3;
  ulonglong *puVar4;
  ulonglong *puVar5;
  code *UNRECOVERED_JUMPTABLE;
  undefined6 extraout_var;
  ulonglong uVar6;
  undefined6 extraout_var_00;
  undefined6 extraout_var_01;
  ulonglong uVar7;
  undefined6 extraout_var_02;
  int iVar8;
  ulonglong *puVar9;
  longlong lVar10;
  ulonglong *local_48;
  longlong local_40;
  
  puVar4 = Reflect_FindBlueprint(param_3,0x4000000);
  puVar9 = (ulonglong *)0x0;
  puVar5 = puVar9;
  if (puVar4 != (ulonglong *)0x0) {
    puVar5 = (ulonglong *)*puVar4;
  }
  puVar4 = puVar5 + 1;
  uVar7 = *puVar5;
  local_48 = puVar4 + (short)uVar7;
  puVar5 = FUN_141589790(param_3,1);
  bVar1 = 0;
  if (puVar5 != (ulonglong *)0x0) {
    bVar1 = (byte)*puVar5;
  }
  puVar5 = puVar9;
  if ((bVar1 & 0xf) == 7) {
    puVar5 = param_3;
  }
  if (puVar5 != (ulonglong *)0x0) {
    bVar1 = (byte)*puVar5;
    while ((~bVar1 & 1) != 0) {
      puVar5 = (ulonglong *)puVar5[1];
      bVar1 = (byte)*puVar5;
    }
    puVar5 = (ulonglong *)puVar5[1];
    if (puVar5 != (ulonglong *)0x0) {
      UNRECOVERED_JUMPTABLE = (code *)Reflect_GetMemberHandler(puVar5);
      if ((int)((longlong)(puVar4 + (short)uVar7) - (longlong)puVar4 >> 3) == 0) {
        (*UNRECOVERED_JUMPTABLE)(param_1,param_2,puVar5,param_4);
      }
      else {
        uVar2 = readVarUint16(puVar4);
        uVar7 = readTypeTag(puVar5);
        if ((int)CONCAT62(extraout_var,uVar2) < (int)uVar7) {
          if (UNRECOVERED_JUMPTABLE == Reflect_CopyFunction) {
            if (0 < (int)param_4) {
              readTypeTag(param_3);
              readTypeTag(param_3);
              uVar2 = readVarUint16(puVar4);
                    /* WARNING: Subroutine does not return */
              Reflect_ArrayCopyHelper(param_1,param_2,(uint)CONCAT62(extraout_var_02,uVar2));
            }
          }
          else {
            uVar7 = readTypeTag(param_3);
            Reflect_MemberHandler
                      (UNRECOVERED_JUMPTABLE,(longlong)param_1,(longlong)param_2,puVar5,param_4,
                       (int)uVar7);
          }
        }
        else if (0 < (int)param_4) {
          do {
            uVar7 = readTypeTag(param_3);
            iVar8 = (int)puVar9;
            uVar6 = readTypeTag(param_3);
            (*UNRECOVERED_JUMPTABLE)
                      ((longlong)((int)uVar6 * iVar8) + (longlong)param_1,
                       (longlong)((int)uVar7 * iVar8) + (longlong)param_2,puVar5,1);
            puVar9 = (ulonglong *)(ulonglong)(iVar8 + 1U);
          } while ((int)(iVar8 + 1U) < (int)param_4);
        }
      }
    }
  }
  lVar10 = 0;
  local_40 = (longlong)(int)((longlong)local_48 - (longlong)puVar4 >> 3);
  if (0 < local_40) {
    do {
      puVar5 = (ulonglong *)puVar4[lVar10];
      local_48 = puVar5;
      uVar2 = readVarUint16(&local_48);
      uVar3 = readVarUint16(&local_48);
      uVar7 = readTypeTag(param_3);
      Reflect_FieldInitDispatcher
                ((longlong)(int)CONCAT62(extraout_var_01,uVar3) + (longlong)param_1,
                 (longlong)(int)CONCAT62(extraout_var_00,uVar2) + (longlong)param_2,puVar5,param_4,
                 (int)uVar7);
      lVar10 = lVar10 + 1;
    } while (lVar10 < local_40);
  }
  return;
}

