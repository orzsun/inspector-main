/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */
// Purpose: An initialization or configuration routine for the GcGameCmd system.
// It sets various global variables to configure the system's internal state,
// preparing it to receive and process game commands.
void Gw2::Game::Net::Cli::GcGameCmd_InitializeState
               (undefined4 param_1,undefined8 *param_2,uint param_3,undefined8 *param_4)

{
  if (DAT_1426283f4 != 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  _DAT_1426282a0 = 0;
  DAT_142628298 = 0;
  _DAT_1426283a0 = 0xb0000;
  _DAT_1426283a4 = 0x1880003;
  DAT_1426283f4 = param_1;
  FUN_140241450(0,param_2,param_4,param_3);
  return;
}
