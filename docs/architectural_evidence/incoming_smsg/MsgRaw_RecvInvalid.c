// Purpose: Generic error reporting function for invalid incoming messages within the Msg::Raw namespace.
// This function is called when the client receives a message that it deems malformed or unexpected.
// It populates an error structure with details about the invalid reception.
void Msg::Raw::RecvInvalid(undefined8 *param_1,longlong param_2)

{
  *(undefined4 *)(param_1 + 3) = 0xc57;
  *param_1 = "RecvInvalid";
  param_1[1] = "Msg::Raw::RecvInvalid";
  param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
  *(undefined4 *)((longlong)param_1 + 0x1c) = *(undefined4 *)(param_2 + 0x40);
  *(undefined4 *)(param_1 + 4) = *(undefined4 *)(param_2 + 0x44);
  *(undefined4 *)((longlong)param_1 + 0x24) = 0;
  return param_1;
}
