// Purpose: Handles the result of a Game Server Encryption Callback.
// This function processes the outcome (success or failure) of an operation
// related to game server encryption, and sets up appropriate state or error messages.
//
// Internal Name Hint: "Gc::GameSrvEncryptCallback"
//
// Key actions:
// - Calls FUN_140239700 (likely the actual encryption callback or a related processing function).
// - If the callback fails, it sets up an error message ("IGcApi_RecvError", "Gc::GameSrvEncryptCallback").
// - If successful, it performs further setup by calling other functions and setting global flags.
undefined8 *
Gw2::Game::Net::Cli::GcGameCmd_HandleGameSrvEncryptCallbackResult(undefined8 *param_1,undefined8 param_2,undefined8 *param_3)

{
  bool bVar1;
  undefined7 extraout_var;
  
  bVar1 = FUN_140239700(DAT_1426283f4,param_3);
  if ((int)CONCAT71(extraout_var,bVar1) == 0) {
    param_1[3] = 0x91;
    *param_1 = "IGcApi_RecvError";
    param_1[1] = "Gc::GameSrvEncryptCallback";
    param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Game\\Net\\Cli\\GcGameCmd.cpp";
    param_1[4] = 0;
    return param_1;
  }
  FUN_140fd2870(DAT_142628290);
  FUN_140238750(DAT_142628290);
  DAT_1426283f8 = 1;
  *param_1 = 0;
  param_1[1] = 0;
  param_1[2] = 0;
  param_1[3] = 0;
  *(undefined4 *)(param_1 + 4) = 0;
  DAT_1426283fc = 1;
  *(undefined4 *)((longlong)param_1 + 0x24) = 1;
  return param_1;
}
