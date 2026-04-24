// Purpose: A high-level dispatcher for control-related events or commands within the game engine.
// This function processes specific control command IDs and either delegates their handling
// to the more general Arena::Core::Engine::SystemEventHandler or handles them directly.
//
// Key actions:
// - Checks param_1[1] (likely a control command ID).
// - Delegates to Arena::Core::Engine::SystemEventHandler for general event processing.
// - Contains specific logic for certain control commands (e.g., 0x13, 0x34).
void Gw2::Engine::Controls::CtlInstance(uint *param_1,float *param_2,undefined8 *param_3)

{
  longlong lVar1;
  
  if (param_1[1] != 0x13) {
    if (param_1[1] != 0x34) {
      Arena::Core::Engine::SystemEventHandler(param_1,param_2,param_3);
      return;
    }
    lVar1 = FUN_14028b440((longlong)param_1);
    *(undefined8 **)(lVar1 + 0x50) = param_3;
    return;
  }
  if (*(longlong *)(param_1 + 2) == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  if (**(longlong **)(param_1 + 2) == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  *param_3 = **(undefined8 **)(param_1 + 2);
  return;
}
