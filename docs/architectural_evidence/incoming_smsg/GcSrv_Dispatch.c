// Purpose: A top-level dispatcher for "Game Server" (GcSrv) related commands.
// It processes commands originating from the game server's perspective, which can then
// lead to various client-side actions, including the generation of outgoing CMSG packets.
//
// Key actions:
// - Dispatches based on a command ID (param_2) to various code paths.
// - Can call GcGameCmd::Handler for specific commands, showing a command hierarchy.
// - Can directly queue outgoing packets (e.g., via MsgConn::QueuePacket).
// - Asserts with "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Game\\Net\\Cli\\GcSrv.cpp".
void GcSrv::Dispatch(longlong param_1,int *param_2,undefined8 param_3,undefined8 param_4)

{
  uint uVar1;
  int iVar2;
  code *pcVar3;
  uint *puVar4;
  IMAGE_DOS_HEADER *pIVar5;
  int *piVar6;
  int *piVar7;
  longlong lVar8;
  int *piVar9;
  undefined8 uVar10;
  ulonglong uVar11;
  undefined8 uVar12;
  char *pcVar13;
  undefined8 uVar14;
  int *piVar15;
  undefined8 uVar16;
  uint local_res10 [2];
  
  uVar1 = *param_2 - 1;
  if (uVar1 < 0xb) {
    pIVar5 = &IMAGE_DOS_HEADER_140000000;
    pcVar13 = IMAGE_DOS_HEADER_140000000.e_magic +
              (&switchD_14023e024::switchdataD_14023e2b4)[uVar1];
    switch(*param_2) {
    case 1:
      if (param_2[4] == 0) {
        pcVar3 = GcGameCmd::Handler;
      }
      else {
        if (param_2[4] != 3) {
                    /* WARNING: Subroutine does not return */
          FUN_1409cd700("D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Game\\Net\\Cli\\GcSrv.cpp",0x47,
                        0x1418fc638,param_4);
        }
        pcVar3 = FUN_14023f330;
      }
      uVar16 = 0;
      uVar14 = 0;
      uVar10 = 0;
      uVar12 = 0;
      (*pcVar3)();
      **(undefined4 **)(param_1 + 0x10) = 1;
      FUN_140239390(uVar12,uVar10,uVar14,uVar16);
      return;
    case 2:
      piVar7 = param_2 + 4;
      FUN_14023e2e0();
      **(undefined4 **)(param_1 + 0x10) = 1;
      FUN_140239390(piVar7,param_2,pcVar13,param_4);
      return;
    case 3:
      uVar1 = param_2[1];
      local_res10[0] = uVar1;
      puVar4 = Arena::Core::Collections::Array(0x142628480,local_res10);
      (**(code **)(puVar4 + 0x12))(2,*(undefined8 *)(puVar4 + 0xc),0,0,puVar4 + 0x10);
      lVar8 = Gw2::Engine::Event::EvtApi();
      uVar10 = 0;
      uVar12 = 4;
      uVar11 = (ulonglong)uVar1;
      MsgConn::QueuePacket(lVar8,uVar1,4,0);
      **(undefined4 **)(param_1 + 0x10) = 1;
      FUN_140239390(lVar8,uVar11,uVar12,uVar10);
      return;
    case 4:
      local_res10[0] = param_2[1];
      puVar4 = Arena::Core::Collections::Array(0x142628480,local_res10);
      uVar14 = 0;
      uVar10 = 0;
      (**(code **)(puVar4 + 0x12))(3,*(undefined8 *)(puVar4 + 0xc),0,0,puVar4 + 0x10);
      uVar12 = 1;
      FUN_14023d900((longlong)puVar4,1);
      **(undefined4 **)(param_1 + 0x10) = 1;
      FUN_140239390(puVar4,uVar12,uVar10,uVar14);
      return;
    case 5:
      local_res10[0] = param_2[1];
      uVar1 = param_2[2];
      puVar4 = Arena::Core::Collections::Array(0x142628480,local_res10);
      piVar7 = param_2 + 4;
      uVar11 = (ulonglong)uVar1;
      uVar12 = *(undefined8 *)(puVar4 + 0xc);
      lVar8 = 4;
      iVar2 = (**(code **)(puVar4 + 0x12))();
      if (iVar2 == 0) {
        lVar8 = *(longlong *)(puVar4 + 0xc);
        uVar12 = 1;
        FUN_140fcc5f0(lVar8);
      }
      **(undefined4 **)(param_1 + 0x10) = 1;
      FUN_140239390(lVar8,uVar12,uVar11,piVar7);
      return;
    case 7:
      FUN_140239390(&IMAGE_DOS_HEADER_140000000,param_2,pcVar13,param_4);
      **(undefined4 **)(param_1 + 0x10) = 1;
      FUN_140239390(pIVar5,param_2,pcVar13,param_4);
      return;
    case 0xb:
      piVar15 = param_2 + 0x54;
      piVar7 = param_2 + 0x14;
      piVar6 = param_2 + 6;
      piVar9 = param_2 + 10;
      FUN_140239440();
      **(undefined4 **)(param_1 + 0x10) = 1;
      FUN_140239390(piVar6,piVar9,piVar7,piVar15);
      return;
    }
  }
  **(undefined4 **)(param_1 + 0x18) = 1;
  return;
}