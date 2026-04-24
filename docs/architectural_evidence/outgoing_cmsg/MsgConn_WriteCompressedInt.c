
void MsgConn_WriteCompressedInt(longlong *param_1,longlong *param_2,int *param_3,int param_4)

{
  longlong lVar1;
  short *psVar2;
  uint uVar3;
  byte *pbVar4;
  byte *pbVar5;
  uint uVar6;
  uint uVar7;
  uint uVar8;
  byte local_28 [16];
  
  uVar6 = *(uint *)*param_2;
  if ((param_4 != 0) && ((uVar6 >> 0x1e & 1) == 0)) {
    uVar6 = 0;
  }
  lVar1 = *param_1;
  do {
    local_28[0] = (byte)uVar6 & 0x7f;
    if (uVar6 >> 7 != 0) {
      local_28[0] = local_28[0] | 0x80;
    }
    if (lVar1 == 0) {
      psVar2 = (short *)param_1[1];
      FUN_140240a60(psVar2,*(int *)(psVar2 + 10) + 1);
                    /* WARNING: Subroutine does not return */
      Reflect_GenericCopyDispatcher
                ((undefined8 *)((ulonglong)*(uint *)(psVar2 + 10) + *(longlong *)(psVar2 + 4)),
                 (undefined8 *)local_28,1);
    }
    uVar7 = 1;
    pbVar5 = local_28;
    while( true ) {
      uVar8 = ((int)lVar1 - *(int *)(lVar1 + 0xd0)) + 0x94c;
      uVar3 = uVar7;
      if (uVar8 <= uVar7) {
        uVar3 = uVar8;
      }
      pbVar4 = pbVar5 + uVar3;
      for (; pbVar5 < pbVar4; pbVar5 = pbVar5 + 1) {
        **(byte **)(lVar1 + 0xd0) = *pbVar5;
        *(longlong *)(lVar1 + 0xd0) = *(longlong *)(lVar1 + 0xd0) + 1;
      }
      if (uVar3 < uVar8) break;
      uVar7 = uVar7 - uVar3;
      MsgConn_FlushPacketBuffer();
    }
    *param_3 = *param_3 + 1;
    uVar7 = uVar6 >> 7;
    uVar6 = uVar6 >> 7;
  } while (uVar7 != 0);
  *param_2 = *param_2 + 4;
  return;
}

