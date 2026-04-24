// [OLD_NAME]: EnqueuePacketForSending
void MsgConn::EnqueuePacket
               (byte *param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,
               undefined8 param_5,undefined8 param_6)

{
  undefined1 auStack_b8 [32];
  undefined4 local_98;
  undefined8 local_80;
  undefined8 local_78;
  ushort local_70;
  LARGE_INTEGER local_68;
  undefined4 local_60;
  undefined4 local_5c;
  ulonglong local_48;
  
  local_48 = DAT_142551900 ^ (ulonglong)auStack_b8;
  local_80 = param_5;
  local_78 = param_6;
  local_70 = 0x23;
  local_98 = param_4;
  local_68 = Arena::Core::Platform::Windows::Common::WinTime();
  if (0x7fff < local_70) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  if ((*param_1 & 2) != 0) {
    local_70 = local_70 | 0x8000;
  }
  local_60 = param_3;
  local_5c = param_2;
  EnterCriticalSection((LPCRITICAL_SECTION)(param_1 + 0x68));
  if ((*param_1 & 1) != 0) {
                    /* WARNING: Subroutine does not return */
    LeaveCriticalSection((LPCRITICAL_SECTION)(param_1 + 0x68));
  }
                    /* WARNING: Subroutine does not return */
  FUN_1409a4530((int *)&DAT_142857044);
}

