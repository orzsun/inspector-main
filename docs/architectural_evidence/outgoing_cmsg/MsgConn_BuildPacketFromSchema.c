// [OLD_NAME]: Msg_BuildPacketFromSchema
undefined8
MsgConn::BuildPacketFromSchema
          (short *param_1,uint *param_2,uint param_3,longlong param_4,uint *param_5)

{
  uint uVar1;
  longlong local_res20;
  longlong local_58;
  short *local_50;
  undefined8 local_48 [4];
  int local_24;
  
  if (param_5 != (uint *)0x0) {
    if ((param_2 != (uint *)0x0) && (*param_2 == 1)) {
      local_res20 = param_4;
      if (param_2[8] == 0) {
        uVar1 = FUN_140fd1fc0((int *)param_2);
        param_2[8] = uVar1;
        uVar1 = FUN_140fd2040((int *)param_2);
        param_2[9] = uVar1;
      }
      param_1[10] = 0;
      param_1[0xb] = 0;
      uVar1 = *(uint *)(param_1 + 8);
      if (*(uint *)(param_1 + 8) <= param_2[9]) {
        uVar1 = param_2[9];
      }
      FUN_140240a60(param_1,uVar1);
      *param_5 = 0;
      local_58 = 0;
      local_50 = param_1;
      Msg::MsgPack(local_48,&local_58,param_2,&local_res20,(ulonglong)param_3 + param_4,
                   (int *)param_5,(uint *)0x0);
      if (*(uint *)(param_1 + 10) < 0x10000) {
        if (local_24 != 0) {
          *param_5 = *(uint *)(param_1 + 10);
          return *(undefined8 *)(param_1 + 4);
        }
      }
      else {
        FUN_140294630((longlong)param_1);
      }
    }
    *param_5 = 0;
  }
  return 0;
}

