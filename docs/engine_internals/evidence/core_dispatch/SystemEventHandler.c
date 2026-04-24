
void Arena::Core::Engine::SystemEventHandler(uint *param_1,float *param_2,undefined8 *param_3)

{
  uint *puVar1;
  short sVar2;
  float fVar3;
  short *psVar4;
  code *pcVar5;
  int iVar6;
  uint uVar7;
  uint uVar8;
  longlong *plVar9;
  longlong lVar10;
  longlong lVar11;
  undefined8 *puVar12;
  ulonglong uVar13;
  uint local_res8 [2];
  
  switch(param_1[1]) {
  case 4:
  case 5:
  case 6:
  case 0x13:
  case 0x34:
    break;
  case 10:
    if (**(longlong **)(param_1 + 2) != 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    uVar7 = FUN_1409a9320();
                    /* WARNING: Subroutine does not return */
    Event::Factory_QueueEvent(1,0xa8,0,0,(char *)0x0,0,uVar7);
  case 0xb:
    lVar10 = FUN_14028b440((longlong)param_1);
    pcVar5 = (code *)**(undefined8 **)(lVar10 + 0x10);
    lVar11 = FUN_14028b440((longlong)param_1);
    (*pcVar5)(lVar10 + 0x10,lVar11 + 0x20);
  case 1:
  case 2:
  case 3:
  case 7:
  case 8:
  case 9:
  case 0xe:
  case 0xf:
  case 0x10:
  case 0x11:
  case 0x12:
  case 0x14:
  case 0x15:
  case 0x16:
  case 0x17:
  case 0x18:
  case 0x19:
  case 0x1a:
  case 0x1b:
  case 0x1c:
  case 0x1d:
  case 0x1e:
  case 0x1f:
  case 0x20:
  case 0x21:
  case 0x22:
  case 0x23:
  case 0x24:
  case 0x25:
  case 0x26:
  case 0x27:
  case 0x28:
  case 0x29:
  case 0x2a:
  case 0x2b:
  case 0x2c:
  case 0x2d:
  case 0x2e:
  case 0x2f:
  case 0x31:
  case 0x32:
  case 0x33:
  case 0x35:
  case 0x39:
  case 0x3a:
  case 0x3b:
  case 0x3c:
  case 0x3d:
  case 0x3e:
  case 0x3f:
  case 0x41:
  case 0x42:
  case 0x43:
  case 0x44:
  case 0x45:
  case 0x46:
  case 0x47:
  case 0x48:
  case 0x49:
  case 0x4a:
  case 0x4b:
    FUN_14028b440((longlong)param_1);
    break;
  case 0xc:
    lVar10 = FUN_14028b440((longlong)param_1);
    iVar6 = Gw2::Engine::Frame::FrApi(*(uint *)(lVar10 + 0x18));
    if (iVar6 != 0) {
      thunk_FUN_141026b60(lVar10 + 0x20);
      (*(code *)**(undefined8 **)(lVar10 + 0x10))(lVar10 + 0x10,lVar10 + 0x20);
                    /* WARNING: Subroutine does not return */
      Gw2::Engine::Frame::FrApi(*(uint *)(lVar10 + 0x18));
    }
    break;
  case 0xd:
    puVar12 = (undefined8 *)FUN_14028b440((longlong)param_1);
    uVar13 = 0;
    *puVar12 = &PTR_FUN_1422b22b8;
    puVar1 = (uint *)((longlong)puVar12 + 0x84);
    puVar12[1] = &PTR_LAB_141906c70;
    puVar12[2] = &PTR__guard_check_icall_1419052b0;
    uVar7 = *puVar1;
    if (uVar7 != 0) {
      do {
        uVar8 = (uint)uVar13;
        local_res8[0] = uVar8;
        if (uVar7 <= uVar8) {
          FUN_140233f00("D:\\Perforce\\Live\\NAEU\\v2\\Code\\Arena\\Core\\Collections\\Array.h",
                        0x2ab,0x1418fc310,local_res8,puVar1);
          pcVar5 = (code *)swi(3);
          (*pcVar5)();
          return;
        }
        (**(code **)**(undefined8 **)(puVar12[0xf] + uVar13 * 8))();
        uVar7 = *puVar1;
        uVar13 = (ulonglong)(uVar8 + 1);
      } while (uVar8 + 1 < uVar7);
    }
    lVar10 = puVar12[0xf];
    if (lVar10 != 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409bf4b0(lVar10);
    }
    puVar12[0xf] = 0;
    puVar12[0x10] = 0;
    if (puVar12[0xb] != 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409bf4b0(puVar12[0xb]);
    }
    FUN_141026900(puVar12 + 4);
    Gw2::Engine::Frame::FrApi((longlong)puVar12,0xa8);
    **(undefined8 **)(param_1 + 2) = 0;
    break;
  case 0x30:
    lVar10 = FUN_14028b440((longlong)param_1);
    if (param_2[1] == 0.0) {
      fVar3 = param_2[2];
      if (fVar3 == 4.2039e-45) {
        FUN_14147ca80(lVar10,*(uint **)(param_2 + 4));
      }
      else if (fVar3 == 5.60519e-45) {
        Collections::Array(lVar10,*(uint **)(param_2 + 4));
      }
      else if (2 < (uint)fVar3) {
                    /* WARNING: Subroutine does not return */
        Gw2::Engine::Frame::FrApi
                  (*(uint *)(lVar10 + 0x18),(uint)fVar3,*(undefined8 *)(param_2 + 4),
                   *(undefined8 *)(param_2 + 6));
      }
    }
    else if (param_2[1] == 2.8026e-45) {
      if ((param_2[2] == 4.2039e-45) &&
         ((((*(int *)(lVar10 + 0x98) != 0 || (*(int *)(lVar10 + 0x9c) != 0)) ||
           (*(int *)(lVar10 + 0xa0) != 0)) || (*(int *)(lVar10 + 0xa4) != 0)))) {
        plVar9 = (longlong *)FUN_14146d130();
        (**(code **)(*plVar9 + 0x88))(plVar9,lVar10 + 0x98,0);
      }
      *(undefined8 *)(lVar10 + 0xa0) = 0;
      *(undefined8 *)(lVar10 + 0x98) = 0;
    }
    break;
  case 0x36:
    lVar10 = FUN_14028b440((longlong)param_1);
    Gw2::Game::Ui::Widgets::ContextMenu::CmInt(*(uint *)(lVar10 + 0x18),1);
    break;
  case 0x37:
    thunk_FUN_14106a190(param_1,param_2,param_3);
    lVar10 = FUN_14028b440((longlong)param_1);
    uVar7 = FUN_141055060(*(uint *)(lVar10 + 0x18),0);
                    /* WARNING: Subroutine does not return */
    Gw2::Engine::Frame::FrApi((undefined8 *)local_res8,uVar7,param_2);
  case 0x38:
    lVar10 = FUN_14028b440((longlong)param_1);
    psVar4 = *(short **)(param_2 + 2);
    uVar13 = 0;
    if (psVar4 != (short *)0x0) {
      sVar2 = *psVar4;
      while (sVar2 != 0) {
        uVar13 = (ulonglong)((int)uVar13 + 1);
        sVar2 = psVar4[uVar13];
      }
    }
    lVar11 = FUN_14147dcd0();
    if (lVar11 == 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    if ((*(byte *)(lVar10 + 0x90) & 2) != 0) {
      uVar7 = FUN_141055060(*(uint *)(lVar10 + 0x18),0);
      FUN_14102ba50(uVar7,0);
    }
    uVar7 = FUN_141055060(*(uint *)(lVar10 + 0x18),0);
                    /* WARNING: Subroutine does not return */
    Gw2::Engine::Controls::CtlText(uVar7,lVar11);
  case 0x40:
    FUN_14028b440((longlong)param_1);
    *param_3 = L"TCtlInstance";
    break;
  default:
    if ((*(longlong **)(param_1 + 2) != (longlong *)0x0) && (**(longlong **)(param_1 + 2) == 0)) {
                    /* WARNING: Subroutine does not return */
      FUN_1409cd550();
    }
    if (*(longlong *)(param_1 + 2) != 0) {
      puVar12 = (undefined8 *)FUN_14028b440((longlong)param_1);
      uVar7 = param_1[1];
      if (uVar7 == 0x5d) {
        (**(code **)*puVar12)(puVar12,param_2,0);
      }
      else if (0x4b < uVar7) {
        uVar8 = FUN_141055060(*(uint *)(puVar12 + 3),0);
                    /* WARNING: Subroutine does not return */
        Gw2::Engine::Frame::FrApi(uVar8,uVar7,param_2,param_3);
      }
    }
  }
  thunk_FUN_14106a190(param_1,param_2,param_3);
  return;
}

