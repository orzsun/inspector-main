
void Msg::Raw::ClientRecvEncrypt
               (undefined8 *param_1,longlong param_2,undefined8 param_3,longlong param_4)

{
  undefined8 *puVar1;
  undefined8 *puVar2;
  undefined4 uVar3;
  undefined4 uVar4;
  undefined4 uVar5;
  undefined4 uVar6;
  undefined4 uVar7;
  undefined4 uVar8;
  undefined4 uVar9;
  undefined4 uVar10;
  undefined8 uVar11;
  undefined8 uVar12;
  undefined8 uVar13;
  undefined8 uVar14;
  longlong lVar15;
  undefined8 *puVar16;
  undefined8 *puVar17;
  undefined1 auStack_88 [32];
  undefined4 uStack_68;
  undefined4 uStack_64;
  undefined8 uStack_5e;
  undefined8 uStack_56;
  undefined4 uStack_4e;
  byte bStack_48;
  byte bStack_47;
  byte bStack_46;
  byte bStack_45;
  byte bStack_44;
  byte bStack_43;
  byte bStack_42;
  byte bStack_41;
  byte bStack_40;
  byte bStack_3f;
  byte bStack_3e;
  byte bStack_3d;
  byte bStack_3c;
  byte bStack_3b;
  byte bStack_3a;
  byte bStack_39;
  byte bStack_38;
  byte bStack_37;
  byte bStack_36;
  byte bStack_35;
  ulonglong local_30;
  
  local_30 = DAT_142551900 ^ (ulonglong)auStack_88;
  if (*(int *)(param_2 + 0x108) != 2) {
    uVar3 = *(undefined4 *)(param_2 + 0x44);
    uVar4 = *(undefined4 *)(param_2 + 0x40);
    *param_1 = "!MSGCONN_MODE_CLIENT_START";
    param_1[1] = "Msg::Raw::ClientRecvEncrypt";
    param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
    *(undefined4 *)((longlong)param_1 + 0x24) = 0;
    *(undefined4 *)(param_1 + 3) = 0xc1c;
    *(undefined4 *)((longlong)param_1 + 0x1c) = uVar4;
    *(undefined4 *)(param_1 + 4) = uVar3;
    FUN_140fd1201();
    return;
  }
  if (*(char *)(param_4 + 1) != '\x16') {
    uVar3 = *(undefined4 *)(param_2 + 0x40);
    uVar4 = *(undefined4 *)(param_2 + 0x44);
    param_1[1] = "Msg::Raw::ClientRecvEncrypt";
    *(undefined4 *)((longlong)param_1 + 0x1c) = uVar3;
    param_1[2] = "D:\\Perforce\\Live\\NAEU\\v2\\Code\\Gw2\\Services\\Msg\\MsgConn.cpp";
    *param_1 = "Connect packet too small";
    *(undefined4 *)(param_1 + 3) = 0xc22;
    *(undefined4 *)(param_1 + 4) = uVar4;
    *(undefined4 *)((longlong)param_1 + 0x24) = 0;
    FUN_140fd1201();
    return;
  }
  if ((undefined8 *)(param_4 + 2U) < (undefined8 *)(param_4 + 0x16U)) {
    uStack_5e = *(undefined8 *)(param_4 + 2U);
    uStack_56 = *(undefined8 *)(param_4 + 10);
    uStack_4e = *(undefined4 *)(param_4 + 0x12);
  }
  bStack_48 = (byte)uStack_5e ^ *(byte *)(param_2 + 0x118);
  bStack_47 = uStack_5e._1_1_ ^ *(byte *)(param_2 + 0x119);
  bStack_46 = uStack_5e._2_1_ ^ *(byte *)(param_2 + 0x11a);
  bStack_45 = uStack_5e._3_1_ ^ *(byte *)(param_2 + 0x11b);
  bStack_44 = uStack_5e._4_1_ ^ *(byte *)(param_2 + 0x11c);
  bStack_43 = uStack_5e._5_1_ ^ *(byte *)(param_2 + 0x11d);
  bStack_42 = uStack_5e._6_1_ ^ *(byte *)(param_2 + 0x11e);
  bStack_41 = uStack_5e._7_1_ ^ *(byte *)(param_2 + 0x11f);
  bStack_40 = (byte)uStack_56 ^ *(byte *)(param_2 + 0x120);
  bStack_3f = uStack_56._1_1_ ^ *(byte *)(param_2 + 0x121);
  bStack_3e = uStack_56._2_1_ ^ *(byte *)(param_2 + 0x122);
  bStack_3d = uStack_56._3_1_ ^ *(byte *)(param_2 + 0x123);
  bStack_3c = uStack_56._4_1_ ^ *(byte *)(param_2 + 0x124);
  bStack_3b = uStack_56._5_1_ ^ *(byte *)(param_2 + 0x125);
  bStack_3a = uStack_56._6_1_ ^ *(byte *)(param_2 + 0x126);
  bStack_39 = uStack_56._7_1_ ^ *(byte *)(param_2 + 0x127);
  bStack_38 = (byte)uStack_4e ^ *(byte *)(param_2 + 0x128);
  bStack_37 = uStack_4e._1_1_ ^ *(byte *)(param_2 + 0x129);
  bStack_36 = uStack_4e._2_1_ ^ *(byte *)(param_2 + 0x12a);
  bStack_35 = uStack_4e._3_1_ ^ *(byte *)(param_2 + 299);
  *(undefined4 *)(param_2 + 0x108) = 3;
  Gw2::Services::Msg::MsgUtil((undefined8 *)(param_2 + 300),0x14);
  lVar15 = 2;
  puVar16 = (undefined8 *)(param_2 + 0x234);
  puVar17 = (undefined8 *)(param_2 + 300);
  do {
    puVar1 = puVar16 + 0x10;
    uVar11 = puVar17[1];
    uVar12 = puVar17[2];
    uVar13 = puVar17[3];
    puVar2 = puVar17 + 0x10;
    *puVar16 = *puVar17;
    puVar16[1] = uVar11;
    uVar11 = puVar17[4];
    uVar14 = puVar17[5];
    puVar16[2] = uVar12;
    puVar16[3] = uVar13;
    uVar12 = puVar17[6];
    uVar13 = puVar17[7];
    puVar16[4] = uVar11;
    puVar16[5] = uVar14;
    uVar11 = puVar17[8];
    uVar14 = puVar17[9];
    puVar16[6] = uVar12;
    puVar16[7] = uVar13;
    uVar12 = puVar17[10];
    uVar13 = puVar17[0xb];
    puVar16[8] = uVar11;
    puVar16[9] = uVar14;
    uVar3 = *(undefined4 *)(puVar17 + 0xc);
    uVar4 = *(undefined4 *)((longlong)puVar17 + 100);
    uVar5 = *(undefined4 *)(puVar17 + 0xd);
    uVar6 = *(undefined4 *)((longlong)puVar17 + 0x6c);
    puVar16[10] = uVar12;
    puVar16[0xb] = uVar13;
    uVar7 = *(undefined4 *)(puVar17 + 0xe);
    uVar8 = *(undefined4 *)((longlong)puVar17 + 0x74);
    uVar9 = *(undefined4 *)(puVar17 + 0xf);
    uVar10 = *(undefined4 *)((longlong)puVar17 + 0x7c);
    *(undefined4 *)(puVar16 + 0xc) = uVar3;
    *(undefined4 *)((longlong)puVar16 + 100) = uVar4;
    *(undefined4 *)(puVar16 + 0xd) = uVar5;
    *(undefined4 *)((longlong)puVar16 + 0x6c) = uVar6;
    *(undefined4 *)(puVar16 + 0xe) = uVar7;
    *(undefined4 *)((longlong)puVar16 + 0x74) = uVar8;
    *(undefined4 *)(puVar16 + 0xf) = uVar9;
    *(undefined4 *)((longlong)puVar16 + 0x7c) = uVar10;
    lVar15 = lVar15 + -1;
    puVar16 = puVar1;
    puVar17 = puVar2;
  } while (lVar15 != 0);
  *puVar1 = *puVar2;
  uStack_68 = 0x300000;
  uStack_64 = 0xc3b1b58;
  (**(code **)(param_2 + 0x110))(param_1,param_3,&uStack_68);
                    /* WARNING: Subroutine does not return */
  FUN_140e37790(local_30 ^ (ulonglong)auStack_88);
}

