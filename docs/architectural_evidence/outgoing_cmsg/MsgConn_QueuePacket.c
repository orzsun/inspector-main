// [OLD_NAME]: QueueOutgoingPacket
bool MsgConn::QueuePacket(longlong param_1,int param_2,undefined4 param_3,undefined8 param_4)

{
  int iVar1;
  undefined8 local_res20;
  
  if (param_1 == 0) {
    return false;
  }
  if ((*(uint *)(param_1 + 0x34) & 0x20) != 0) {
    return false;
  }
  if (((*(uint *)(param_1 + 0x34) & 4) != 0) && (param_2 == 0)) {
    return true;
  }
  local_res20 = param_4;
  iVar1 = EnqueuePacket((byte *)(param_1 + 0x680),param_2,param_3,1,&stack0x00000028,&local_res20);
  return iVar1 != 0;
}

