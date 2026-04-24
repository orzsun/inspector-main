// FUN_00fd5680
void SMSG_DispatchMap_MapSchemaWithInternalID
               (longlong param_1,undefined4 param_2,uint param_3,longlong *param_4)

{
  uint *puVar1;
  int *piVar2;
  code *pcVar3;
  uint uVar4;
  int iVar5;
  longlong *plVar6;
  undefined4 *puVar7;
  ulonglong uVar8;
  uint uVar9;
  uint auStackX_18 [2];
  
  uVar9 = 0;
  plVar6 = param_4;
  uVar8 = (ulonglong)param_3;
  if (param_3 != 0) {
    do {
      if (*(int *)*plVar6 != 1) {
                    /* WARNING: Subroutine does not return */
        FUN_009cd550();
      }
      uVar4 = *(uint *)(*plVar6 + 0x10);
      if (*(uint *)(*plVar6 + 0x10) <= uVar9) {
        uVar4 = uVar9;
      }
      uVar9 = uVar4;
      uVar8 = uVar8 - 1;
      plVar6 = plVar6 + 1;
    } while (uVar8 != 0);
  }
  uVar9 = uVar9 + 1;
  puVar1 = (uint *)(param_1 + 0x5c);
  if (*puVar1 < uVar9) {
    FUN_00238140((short *)(param_1 + 0x48),uVar9);
                    /* WARNING: Subroutine does not return */
    FUN_009a9a20((undefined1 (*) [32])((ulonglong)*puVar1 * 0x10 + *(longlong *)(param_1 + 0x50)),
                 (ulonglong)(uVar9 - *puVar1) << 4);
  }
  plVar6 = param_4 + param_3;
  if (param_4 != plVar6) {
    do {
      if (*(int *)*param_4 != 1) {
                    /* WARNING: Subroutine does not return */
        FUN_009cd550();
      }
      auStackX_18[0] = *(uint *)(*param_4 + 0x10);
      if (*(uint *)(param_1 + 0x5c) <= auStackX_18[0]) {
        FUN_00233f00("D:\\Perforce\\Live\\NAEU\\v2\\Code\\Arena\\Core\\Collections\\Array.h",0x2ab,
                     0x18fc310,auStackX_18,puVar1);
        pcVar3 = (code *)swi(3);
        (*pcVar3)();
        return;
      }
      puVar7 = (undefined4 *)((ulonglong)auStackX_18[0] * 0x10 + *(longlong *)(param_1 + 0x50));
      if (*(longlong *)(puVar7 + 2) != 0) {
                    /* WARNING: Subroutine does not return */
        FUN_009cd550();
      }
      piVar2 = (int *)*param_4;
      *(int **)(puVar7 + 2) = piVar2;
      iVar5 = FUN_00fd1fc0(piVar2);
      *(int *)(*(longlong *)(puVar7 + 2) + 0x20) = iVar5;
      iVar5 = FUN_00fd2040(*(int **)(puVar7 + 2));
      param_4 = param_4 + 1;
      *(int *)(*(longlong *)(puVar7 + 2) + 0x24) = iVar5;
      *puVar7 = param_2;
    } while (param_4 != plVar6);
  }
  return;
}

