
void CMSG::BuildAndSendPacket(byte *param_1,uint param_2,ushort *param_3)

{
  ushort uVar1;
  undefined4 *puVar2;
  uint auStackX_8 [2];
  int aiStackX_10 [2];
  ushort *apuStackX_18 [2];
  byte *pbStack_88;
  undefined8 uStack_80;
  undefined8 auStack_78 [4];
  int iStack_54;
  undefined8 auStack_50 [5];
  
  if (param_1 == (byte *)0x0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  if (param_3 == (ushort *)0x0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  if (param_2 < 2) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  if ((*(int *)(param_1 + 0x108) == 3) && ((*param_1 & 4) != 0)) {
    uVar1 = *param_3;
    puVar2 = (undefined4 *)FUN_140fd5b50(*(longlong *)(param_1 + 0x18),(uint)uVar1);
    if (puVar2 == (undefined4 *)0x0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    if (*(int *)(*(longlong *)(puVar2 + 2) + 0x20) == 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    if (param_2 != *(uint *)(*(longlong *)(puVar2 + 2) + 0x20)) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    apuStackX_18[0] = param_3;
    FUN_140fd6990(auStack_50,param_1);
    auStackX_8[0] = 0xffffffff;
    aiStackX_10[0] = 0;
    uStack_80 = 0;
    pbStack_88 = param_1;
    Msg::MsgPack(auStack_78,(longlong *)&pbStack_88,*(uint **)(puVar2 + 2),(longlong *)apuStackX_18,
                 (longlong)param_3 + (ulonglong)param_2,aiStackX_10,auStackX_8);
    if (iStack_54 == 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    thunk_FUN_140fd6400(*(undefined4 *)(param_1 + 0x10),*puVar2,(uint)uVar1,auStackX_8[0],
                        aiStackX_10[0]);
    if (*(int **)(param_1 + 0x38) != (int *)0x0) {
                    /* WARNING: Subroutine does not return */
      FUN_140239770(*(int **)(param_1 + 0x38),(longlong)param_1,2,0);
    }
    FUN_1409ad580();
    FUN_140fd69f0((longlong)auStack_50);
  }
  return;
}

