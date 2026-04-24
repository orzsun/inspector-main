// Purpose: A primary initialization or setup routine for the client's connection to the Portal (Platform) server.
// This function orchestrates the setup of the Portal client, tying together the event system and game parameters.
//
// Key actions:
// - Initializes the event API for the Portal connection.
// - Retrieves game parameters.
// - Calls Gw2::Services::PortalCli::PortalCli (the Portal client constructor) to initiate the connection.
void PortalCli::Startup(char *param_1)

{
  u_short uVar1;
  int iVar2;
  uint uVar3;
  ushort *puVar4;
  undefined6 extraout_var;
  undefined8 uVar5;
  uint uVar6;
  short *psVar7;
  undefined1 auStackY_68 [32];
  short local_30 [16];
  ulonglong local_10;
  
  local_10 = DAT_142551900 ^ (ulonglong)auStackY_68;
  DAT_142628800 = Gw2::Engine::Event::EvtApi();
  puVar4 = (ushort *)Gw2::Game::Param::Param(0x84);
  uVar6 = 0;
  psVar7 = (short *)0x0;
  FUN_1409b0f50((undefined1 (*) [32])local_30);
  if (*puVar4 != 0) {
    iVar2 = Arena::Core::Platform::Common::Address((undefined1 (*) [32])local_30,puVar4,0x19c8);
    if (iVar2 != 0) {
      DAT_142628818 = 1;
      psVar7 = local_30;
      uVar1 = FUN_1409b2ce0(local_30);
      uVar3 = (uint)CONCAT62(extraout_var,uVar1);
      goto LAB_140243f44;
    }
  }
  DAT_142628818 = 0;
  uVar3 = FUN_1402418a0();
LAB_140243f44:
  if ((param_1 == (char *)0x0) || (*param_1 == '\0')) {
    uVar6 = 1;
  }
  uVar5 = FUN_1409a2170();
  Gw2::Services::PortalCli::PortalCli((uint)uVar5,psVar7,(ulonglong)uVar3,(ulonglong)uVar6,param_1);
  FUN_140fdc960(0x142511590);
                    /* WARNING: Subroutine does not return */
  FUN_140e37790(local_10 ^ (ulonglong)auStackY_68);
}
