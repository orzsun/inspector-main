
void Gw2::Game::Net::Cli::GcPortal(longlong *param_1,int param_2)

{
  undefined4 local_res18;
  undefined4 local_res1c;
  undefined8 local_80;
  int local_78;
  
  local_80 = 0;
  local_78 = param_2;
  if (param_2 != 7) {
    MsgConn::QueuePacket(DAT_142628800,0,0x10,&local_80);
    return;
  }
  if (*(int *)((longlong)param_1 + 0xac) != *(int *)((longlong)param_1 + 0x8c)) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  if (*(int *)((longlong)param_1 + 0x8c) == 0) {
    local_res18 = 0xb0004;
    local_res1c = 0x7990004;
    (**(code **)(*param_1 + 0x10))(param_1,&local_res18);
    return;
  }
                    /* WARNING: Subroutine does not return */
  FUN_1409a94a0();
}

