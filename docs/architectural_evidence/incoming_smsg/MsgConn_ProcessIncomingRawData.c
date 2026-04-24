// Purpose: A wrapper or intermediary function for calling Msg::MsgConn_Dispatch.
// It serves as a specific entry point for feeding raw incoming data into the MsgConn system,
// potentially from a network thread or a specific game component.
//
// Key actions:
// - Checks if a global MsgConn instance (DAT_142628290) is valid.
// - Calls Msg::MsgConn_Dispatch with the global MsgConn instance and provided data/size.
// - Handles return status, potentially calling other MsgConn overloads for status/error handling.
int MsgConn::ProcessIncomingRawData(uint param_1,byte *param_2)
{
  uint local_res18 [2];
  undefined4 local_res20 [2];
  undefined8 local_38;
  undefined8 local_30 [4];
  int local_c;
  
  if (DAT_142628290 == (byte *)0x0) {
    return 0;
  }
  local_38 = 0;
  Msg::MsgConn_Dispatch(local_30,DAT_142628290,param_2,param_1,0,(undefined4 *)&local_38);
  if (local_c == 0) {
    Gw2::Services::Msg::MsgConn((longlong)DAT_142628290,local_res18,local_res20);
    if (3 < local_res18[0]) {
      local_res18[0] = 0;
    }
    FUN_14023d700();
  }
  return local_c;
}

