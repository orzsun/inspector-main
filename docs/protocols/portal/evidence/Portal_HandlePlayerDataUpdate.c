
void Portal::HandlePlayerDataUpdate(longlong param_1,ulonglong param_2,uint param_3,uint param_4)

{
  longlong lVar1;
  undefined4 uVar2;
  uint uVar3;
  longlong lVar4;
  undefined1 auStack_128 [32];
  short local_108;
  undefined2 local_106;
  longlong local_100;
  undefined8 local_f8;
  undefined4 local_f0;
  undefined8 local_e8;
  undefined8 uStack_e0;
  undefined8 local_d8;
  undefined8 uStack_d0;
  undefined8 local_c8;
  undefined8 uStack_c0;
  undefined8 local_b8;
  undefined8 uStack_b0;
  undefined8 local_a8;
  undefined8 uStack_a0;
  undefined8 local_98;
  undefined8 uStack_90;
  undefined8 local_88;
  undefined8 uStack_80;
  undefined8 local_78;
  undefined8 uStack_70;
  undefined8 local_68;
  undefined8 uStack_60;
  undefined8 local_58;
  undefined8 uStack_50;
  uint local_48;
  ulonglong local_38;
  
  local_38 = DAT_142551900 ^ (ulonglong)auStack_128;
  uVar2 = FUN_1409a35f0();
  local_106 = (undefined2)uVar2;
  local_108 = 1;
  local_f8 = 0;
  local_100 = 0;
  local_f0 = 0x100;
  lVar4 = MsgConn::BuildArgsFromSchema(&local_108,(uint *)&DAT_142514e00,param_3,param_2);
  if (lVar4 != 0) {
    local_48 = *(uint *)(param_1 + 0x160);
    lVar1 = param_1 + 0xb0;
    local_e8 = *(undefined8 *)(param_1 + 0xc0);
    uStack_e0 = *(undefined8 *)(param_1 + 200);
    local_d8 = *(undefined8 *)(param_1 + 0xd0);
    uStack_d0 = *(undefined8 *)(param_1 + 0xd8);
    local_c8 = *(undefined8 *)(param_1 + 0xe0);
    uStack_c0 = *(undefined8 *)(param_1 + 0xe8);
    local_b8 = *(undefined8 *)(param_1 + 0xf0);
    uStack_b0 = *(undefined8 *)(param_1 + 0xf8);
    local_a8 = *(undefined8 *)(param_1 + 0x100);
    uStack_a0 = *(undefined8 *)(param_1 + 0x108);
    local_98 = *(undefined8 *)(param_1 + 0x110);
    uStack_90 = *(undefined8 *)(param_1 + 0x118);
    local_88 = *(undefined8 *)(param_1 + 0x120);
    uStack_80 = *(undefined8 *)(param_1 + 0x128);
    local_68 = *(undefined8 *)(param_1 + 0x140);
    uStack_60 = *(undefined8 *)(param_1 + 0x148);
    local_78 = *(undefined8 *)(param_1 + 0x130);
    uStack_70 = *(undefined8 *)(param_1 + 0x138);
    local_58 = *(undefined8 *)(param_1 + 0x150);
    uStack_50 = *(undefined8 *)(param_1 + 0x158);
    uVar3 = FUN_14025a290(lVar1,lVar4);
    if ((uVar3 & 1) != 0) {
      FUN_14026c460(lVar1);
    }
    if ((uVar3 & 6) != 0) {
      FUN_14026c4a0(lVar1);
    }
    if ((uVar3 >> 0xe & 1) != 0) {
      FUN_14026c4e0(lVar1);
    }
    if ((uVar3 >> 0x10 & 1) != 0) {
      FUN_14026c5a0(lVar1);
    }
    if ((uVar3 & 8) != 0) {
      FUN_14026c520(lVar1);
    }
    if ((uVar3 & 0x10) != 0) {
      FUN_14026c560(lVar1);
    }
    if ((uVar3 & 0x40) != 0) {
      FUN_14026c5e0(lVar1);
    }
    if ((char)uVar3 < '\0') {
      FUN_14026c620(lVar1);
    }
    if ((uVar3 >> 0x11 & 1) != 0) {
      FUN_14026c660(lVar1);
    }
    if (((uVar3 >> 8 & 1) != 0) || (param_4 != 0)) {
      FUN_14026c6a0(lVar1,(ulonglong)(*(byte *)(lVar4 + 0x6b) ^ local_48),(ulonglong)param_4,param_2
                   );
    }
    if ((uVar3 >> 10 & 1) != 0) {
      FUN_14026c6c0(lVar1,(undefined4 *)((longlong)&uStack_a0 + 4));
    }
    if ((uVar3 >> 0xb & 1) != 0) {
      FUN_14026c730(lVar1);
    }
    if ((uVar3 >> 0xc & 1) != 0) {
      FUN_14026c770(lVar1,(longlong)&uStack_60 + 4);
    }
  }
  if (local_100 != 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409bf4b0(local_100);
  }
                    /* WARNING: Subroutine does not return */
  FUN_140e37790(local_38 ^ (ulonglong)auStack_128);
}

