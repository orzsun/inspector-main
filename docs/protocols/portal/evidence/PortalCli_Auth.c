
void Gw2::Services::PortalCli::PortalCliAuth(longlong param_1)

{
  longlong *plVar1;
  short local_res8 [4];
  int local_28 [8];
  
  if (*(longlong *)(param_1 + 0x880) == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  plVar1 = *(longlong **)(param_1 + 0x880);
  *(undefined8 *)(param_1 + 0x880) = 0;
  local_res8[0] = 0x2a;
  local_res8[1] = 1000;
  local_res8[2] = 0x1b5e;
  local_res8[3] = 0x4ed;
  FUN_140e176a0(local_28,local_res8);
  Arena::Services::StsInetSocket::StsInetSocket(plVar1[4],local_28,0);
  *(longlong *)
   (((plVar1[1] & 0xfffffffffffffffeU) - (*(ulonglong *)(*plVar1 + 8) & 0xfffffffffffffffe)) +
   (longlong)plVar1) = *plVar1;
  *(longlong *)(*plVar1 + 8) = plVar1[1];
                    /* WARNING: Subroutine does not return */
  thunk_FUN_1409bf4b0((longlong)plVar1);
}

