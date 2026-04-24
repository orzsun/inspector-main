// Purpose: Main entry point for processing raw incoming network data for the MsgConn system.
// This function sits at the very beginning of the incoming Game Server (gs2c) pipeline.
// It takes the raw byte stream, performs initial framing and error checking, and then
// feeds individual messages into the Msg::DispatchStream for further processing.
//
// Internal Name Hint: "MsgConnDispatch"
//
// Key actions:
// - Manages connection state and performs initial validation.
// - Iteratively processes chunks of the raw incoming message buffer.
// - Dispatches individual messages to Msg::DispatchStream.
// - Uses a dispatch table (indexed by message opcode) for initial raw message handling.
void Gw2::Services::Msg::MsgConn_Dispatch
               (undefined8 *param_1,byte *param_2,byte *param_3,uint param_4,int param_5,
               undefined4 *param_6)

{
  undefined8 uVar1;
  undefined8 uVar2;
  undefined8 uVar3;
  undefined8 uVar4;
  undefined8 uVar5;
  bool bVar6;
  undefined7 extraout_var;
  longlong lVar7;
  undefined7 extraout_var_00;
  undefined8 *puVar8;
  byte bVar9;
  undefined8 local_58 [4];
  undefined8 local_38;
  ulonglong local_30;
  
  local_30 = DAT_142551900 ^ (ulonglong)local_58;
  bVar9 = *param_2 | 1;
  if (param_5 == 0) {
    bVar9 = *param_2 & 0xfe;
  }
  *param_2 = bVar9;
  bVar6 = FUN_1409a4290();
  if ((int)CONCAT71(extraout_var,bVar6) != 0) {
    FUN_140fd6740(0);
  }
  if ((*param_2 & 2) == 0) {
    *(undefined4 *)(param_1 + 3) = 0xf69;
    *param_1 = "msgConn already closed";
    param_1[1] = "MsgConnDispatch";
    param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
    *(undefined4 *)((longlong)param_1 + 0x1c) = *(undefined4 *)(param_2 + 0x40);
    *(undefined4 *)(param_1 + 4) = *(undefined4 *)(param_2 + 0x44);
    *(undefined4 *)((longlong)param_1 + 0x24) = 0;
  }
  else {
    do {
      if (*(int *)(param_2 + 0x108) == 3) {
      }
      if (*(int *)(param_2 + 0x108) == 1) {
        FUN_140fd6c60((short *)(param_2 + 0x80),(undefined8 *)param_3,param_4);
        puVar8 = ::Msg::DispatchStream(local_58,param_2,param_6);
        uVar1 = puVar8[4];
        uVar2 = *puVar8;
        uVar3 = puVar8[1];
        uVar4 = puVar8[2];
        uVar5 = puVar8[3];
        local_38._4_4_ = (int)((ulonglong)uVar1 >> 0x20);
        bVar6 = local_38._4_4_ == 0;
        local_38 = uVar1;
        if (((bVar6) && (*param_2 = *param_2 & 0xfd, *(int *)(param_2 + 0x10) == 0)) &&
           (*(int *)(param_2 + 0x40) != 1)) {
          *param_1 = uVar2;
          param_1[1] = uVar3;
          param_1[2] = uVar4;
          param_1[3] = uVar5;
          param_1[4] = uVar1;
          goto LAB_140fd2748;
        }
        break;
      }
      if (((param_4 < 2) || (param_4 < param_3[1])) ||
         ((2 < *param_3 ||
          (lVar7 = (*(code *)(&PTR_RecvInvalid_1420dea48)[*param_3])
                             (local_58,param_2,param_6,param_3), *(int *)(lVar7 + 0x24) == 0)))) {
LAB_140fd2771:
        *(undefined4 *)(param_1 + 3) = 0xfa5;
        *param_1 = "Raw dispatch failed";
        param_1[1] = "MsgConnDispatch";
        param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
        *(undefined4 *)((longlong)param_1 + 0x1c) = *(undefined4 *)(param_2 + 0x40);
        *(undefined4 *)(param_1 + 4) = *(undefined4 *)(param_2 + 0x44);
        *(undefined4 *)((longlong)param_1 + 0x24) = 0;
        goto LAB_140fd2748;
      }
      bVar9 = param_3[1];
      if (bVar9 == 0) goto LAB_140fd2771;
      param_3 = param_3 + bVar9;
      param_4 = param_4 - bVar9;
      bVar6 = FUN_1409a4290();
      if ((int)CONCAT71(extraout_var_00,bVar6) != 0) {
        FUN_140fd6740(1);
      }
    } while (param_4 != 0);
    *(undefined4 *)((longlong)param_1 + 0x24) = 1;
    *param_1 = 0;
    param_1[1] = 0;
    param_1[2] = 0;
    param_1[3] = 0;
    *(undefined4 *)(param_1 + 4) = 0;
  }
LAB_140fd2748:
                    /* WARNING: Subroutine does not return */
  FUN_140e37790(local_30 ^ (ulonglong)local_58);
}