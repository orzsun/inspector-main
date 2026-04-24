
void Portal::DynamicHandler_Entry(longlong param_1,int *param_2)

{
  int iVar1;
  longlong *plVar2;
  code *pcVar3;
  undefined4 uVar4;
  longlong lVar5;
  longlong lStack_38;
  undefined4 uStack_30;
  undefined2 uStack_28;
  undefined2 uStack_26;
  undefined4 uStack_24;
  
  lVar5 = FUN_1409a4490();
  DispatchMessage(*(longlong *)(lVar5 + 0x1f0) + 0x1d0,param_1,param_2);
  if (*param_2 == 0x60) {
    lVar5 = FUN_1409a4490();
    FUN_140267d20(*(longlong *)(lVar5 + 0x1f0));
    **(undefined4 **)(param_1 + 0x18) = 0;
  }
  if (*param_2 == 0x41) {
    lVar5 = FUN_1409a4490();
    iVar1 = param_2[0x24];
    plVar2 = *(longlong **)(lVar5 + 0x1f0);
    lStack_38 = 0;
    uStack_30 = 0;
    pcVar3 = *(code **)(*plVar2 + 0x1e0);
    uVar4 = FUN_1409a8e80();
    uStack_26 = (undefined2)uVar4;
    uStack_28 = 1;
    uStack_24 = 0;
    ManagedString_Build_FromAnsiLen1(&lStack_38,(char *)(param_2 + 4),(char *)0xffffffff,0);
    (*pcVar3)(plVar2,&lStack_38,iVar1);
    if (lStack_38 != 0) {
                    /* WARNING: Subroutine does not return */
      FUN_1409bf4b0(lStack_38);
    }
    **(undefined4 **)(param_1 + 0x18) = 0;
  }
  return;
}

