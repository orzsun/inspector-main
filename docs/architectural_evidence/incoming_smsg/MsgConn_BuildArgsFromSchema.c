// [OLD_NAME]: Msg_BuildArgs_FromSchema
undefined8 MsgConn::BuildArgsFromSchema(short *param_1,uint *param_2,uint param_3,ulonglong param_4)

{
  uint uVar1;
  ulonglong *puVar2;
  undefined8 uVar3;
  int local_res18 [2];
  ulonglong local_res20;
  uint local_38 [8];
  
  local_res20 = param_4;
  if (*param_2 != 1) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  if (param_2[8] == 0) {
    uVar1 = FUN_140fd1fc0((int *)param_2);
    param_2[8] = uVar1;
    uVar1 = FUN_140fd2040((int *)param_2);
    param_2[9] = uVar1;
  }
  uVar3 = 0;
  param_1[10] = 0;
  param_1[0xb] = 0;
  uVar1 = *(uint *)(param_1 + 8);
  if (*(uint *)(param_1 + 8) <= param_2[9]) {
    uVar1 = param_2[9];
  }
  FUN_140240a60(param_1,uVar1);
  FUN_140fd6a90(local_38,param_1);
  uVar1 = param_2[8];
  puVar2 = (ulonglong *)Arena::Core::Collections::Array(local_38,uVar1);
  local_res18[0] = 0;
  MsgUnpack::ParseWithSchema
            (param_2,&local_res20,(ushort *)(param_3 + local_res20),puVar2,
             (longlong)puVar2 + (ulonglong)uVar1,local_38,local_res18,(uint *)0x0);
  if (0xffff < *(uint *)(param_1 + 10)) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  if (local_res18[0] == 0) {
    uVar3 = *(undefined8 *)(param_1 + 4);
  }
  FUN_140fd6ab0((longlong)local_38);
  return uVar3;
}

