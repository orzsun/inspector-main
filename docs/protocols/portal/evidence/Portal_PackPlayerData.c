
void Portal::PackPlayerData(void)

{
  int iVar1;
  byte *pbVar2;
  longlong lVar3;
  char *pcVar4;
  ulonglong uVar5;
  short *psVar6;
  ulonglong uVar7;
  undefined8 uVar8;
  uint uVar9;
  ulonglong uVar10;
  longlong unaff_RBP;
  longlong *unaff_RSI;
  undefined **unaff_RDI;
  code *pcVar11;
  longlong unaff_R13;
  byte *pbVar12;
  uint unaff_R15D;
  char *in_stack_00000030;
  char *in_stack_00000058;
  char *in_stack_00000060;
  uint *in_stack_00000068;
  char *in_stack_00000070;
  uint *in_stack_00000078;
  
code_r0x00014027b63a:
  do {
    unaff_RSI = (longlong *)(**(code **)(*unaff_RSI + 0x160))(unaff_RSI,0,0xffffffff);
    if (unaff_RSI == (longlong *)0x0) {
      if (unaff_R15D < 0xff) {
        *(char *)(unaff_RBP + -4) = (char)unaff_R15D;
      }
      else {
        *(undefined1 *)(unaff_RBP + -4) = 0xff;
      }
      *(longlong *)(unaff_RBP + -3) = unaff_R13;
      uVar8 = MsgConn::BuildPacketFromSchema
                        (*(short **)(unaff_RBP + -0x80),(uint *)&DAT_142514e00,0x75,
                         unaff_RBP + -0x70,in_stack_00000078);
      **(undefined8 **)(unaff_RBP + -0x78) = uVar8;
      if (unaff_R13 == 0) {
                    /* WARNING: Subroutine does not return */
        FUN_140e37790(*(ulonglong *)(unaff_RBP + 0x30) ^ (ulonglong)&stack0x00000000);
      }
                    /* WARNING: Subroutine does not return */
      FUN_1409bf4b0(unaff_R13);
    }
    (**(code **)*unaff_RSI)(unaff_RSI);
    pbVar2 = (byte *)(**(code **)(*unaff_RSI + 0x10))(unaff_RSI);
    pcVar11 = *(code **)(*unaff_RSI + 0x80);
    lVar3 = (*pcVar11)(unaff_RSI,"scope",0);
  } while ((lVar3 != 0) && (uVar8 = FUN_140268cc0(lVar3), (int)uVar8 == 0));
  uVar10 = 0;
LAB_14027b330:
  pcVar4 = (char *)(**(code **)*unaff_RSI)(unaff_RSI);
  uVar8 = FUN_1409aaa50((longlong)*unaff_RDI,pcVar4,-1);
  if ((int)uVar8 != 0) goto code_r0x00014027b34e;
  if ((&DAT_141902258)[uVar10 * 8] == 4) {
    uVar8 = (**(code **)(*unaff_RSI + 0x10))(unaff_RSI);
    iVar1 = FUN_140dc86b0(uVar8,(undefined1 (*) [32])(unaff_RBP + 0x20));
    if ((((iVar1 != 0) && (*(int *)(unaff_RBP + 0x20) == 0)) && (*(int *)(unaff_RBP + 0x24) == 0))
       && ((*(int *)(unaff_RBP + 0x28) == 0 && (*(int *)(unaff_RBP + 0x2c) == 0))))
    goto LAB_14027b3aa;
  }
  if (uVar10 * 0x20 != -0x141902250) {
    pbVar12 = (byte *)(unaff_RBP + -0x70 + (&DAT_141902260)[uVar10 * 4]);
    switch((&DAT_141902258)[uVar10 * 8]) {
    case 0:
      uVar7 = thunk_FUN_140e4935c((longlong)pbVar2,(longlong *)&stack0x00000058,10);
      if ((uVar7 < 0x100) && (*in_stack_00000058 == '\0')) {
        *pbVar12 = (byte)uVar7;
        goto LAB_14027b62d;
      }
      break;
    case 1:
      if (0xfe < (uint)(&DAT_14190226c)[uVar10 * 8]) {
                    /* WARNING: Subroutine does not return */
        FUN_1409cd550();
      }
      if ((&DAT_14190226c)[uVar10 * 8] == 0) {
                    /* WARNING: Subroutine does not return */
        FUN_1409cd550();
      }
      if (*pbVar2 == 0) {
        *pbVar12 = *pbVar12 & ~*(byte *)(&DAT_14190226c + uVar10 * 8);
      }
      else {
        *pbVar12 = *pbVar12 | *(byte *)(&DAT_14190226c + uVar10 * 8);
      }
      goto LAB_14027b62d;
    case 2:
      uVar7 = thunk_FUN_140e491e8((longlong)pbVar2,(longlong *)&stack0x00000060,10,(int)pcVar11);
      if ((uVar7 + 0x80000000 < 0x180000000) && (*in_stack_00000060 == '\0')) {
        *(int *)pbVar12 = (int)uVar7;
        goto LAB_14027b62d;
      }
      break;
    case 3:
      uVar7 = thunk_FUN_140e491e8((longlong)pbVar2,(longlong *)&stack0x00000030,10,(int)pcVar11);
      if ((uVar7 + 0x80000000 < 0x180000000) && (*in_stack_00000030 == ',')) {
        in_stack_00000030 = in_stack_00000030 + 1;
        uVar5 = thunk_FUN_140e491e8((longlong)in_stack_00000030,(longlong *)&stack0x00000030,10,
                                    (int)pcVar11);
        if ((uVar5 + 0x80000000 < 0x180000000) && (*in_stack_00000030 == '\0')) {
          *(int *)pbVar12 = (int)uVar7;
          *(int *)(pbVar12 + 4) = (int)uVar5;
          goto LAB_14027b62d;
        }
      }
      break;
    case 4:
      if (*pbVar2 == 0) {
        *(undefined8 *)(unaff_RBP + 0x18) = 0;
        *(undefined8 *)(unaff_RBP + 0x10) = 0;
        uVar8 = *(undefined8 *)(unaff_RBP + 0x18);
        *(undefined8 *)pbVar12 = *(undefined8 *)(unaff_RBP + 0x10);
        *(undefined8 *)(pbVar12 + 8) = uVar8;
      }
      else {
        iVar1 = FUN_140dc86b0(pbVar2,(undefined1 (*) [32])(unaff_RBP + 0x10));
        if (iVar1 == 0) break;
        uVar8 = *(undefined8 *)(unaff_RBP + 0x18);
        *(undefined8 *)pbVar12 = *(undefined8 *)(unaff_RBP + 0x10);
        *(undefined8 *)(pbVar12 + 8) = uVar8;
      }
      goto LAB_14027b62d;
    case 5:
      if ((&DAT_14190226c)[uVar10 * 8] == 0) {
                    /* WARNING: Subroutine does not return */
        FUN_1409cd550();
      }
      psVar6 = (short *)FUN_140e2e910(in_stack_00000068,
                                      (ulonglong)(uint)(&DAT_14190226c)[uVar10 * 8] * 2);
      Arena::Core::Basics::Str(psVar6,pbVar2,(ulonglong)(uint)(&DAT_14190226c)[uVar10 * 8],-1);
      *(short **)pbVar12 = psVar6;
LAB_14027b62d:
      *(uint *)(unaff_RBP + -0x6e) = *(uint *)(unaff_RBP + -0x6e) | (&DAT_141902268)[uVar10 * 8];
      break;
    case 6:
      uVar7 = thunk_FUN_140e4935c((longlong)pbVar2,(longlong *)&stack0x00000070,10);
      if ((uVar7 < 0x10000) && (*in_stack_00000070 == '\0')) {
        *(short *)pbVar12 = (short)uVar7;
        goto LAB_14027b62d;
      }
      break;
    default:
                    /* WARNING: Subroutine does not return */
      FUN_1409cd700("D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Game\\Portal\\PlPack.cpp",0x112,
                    0x141902510,pcVar11);
    }
    unaff_RDI = &PTR_DAT_141902250;
    goto code_r0x00014027b63a;
  }
  goto LAB_14027b3aa;
code_r0x00014027b34e:
  uVar9 = (int)uVar10 + 1;
  uVar10 = (ulonglong)uVar9;
  unaff_RDI = unaff_RDI + 4;
  if (0x13 < uVar9) goto LAB_14027b3aa;
  goto LAB_14027b330;
LAB_14027b3aa:
  unaff_RDI = &PTR_DAT_141902250;
  if ((pbVar2 != (byte *)0x0) && (*pbVar2 != 0)) {
                    /* WARNING: Subroutine does not return */
    FUN_140242c60((short *)&stack0x00000038,unaff_R15D + 1);
  }
  goto code_r0x00014027b63a;
}

