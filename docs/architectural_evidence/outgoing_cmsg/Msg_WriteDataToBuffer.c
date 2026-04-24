
void Msg::WriteDataToBuffer(longlong *param_1,uint param_2,undefined8 *param_3,uint param_4)

{
  short *psVar1;
  
  if (*param_1 != 0) {
    MsgConn_WriteToPacketBuffer(*param_1,param_2,param_3,param_4);
    return;
  }
  psVar1 = (short *)param_1[1];
  FUN_140240a60(psVar1,*(int *)(psVar1 + 10) + param_2 * param_4);
                    /* WARNING: Subroutine does not return */
  thunk_FUN_1418c47c0((undefined8 *)((ulonglong)*(uint *)(psVar1 + 10) + *(longlong *)(psVar1 + 4)),
                      param_3,(ulonglong)(param_2 * param_4));
}

