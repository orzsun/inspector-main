
void Gw2::Services::PortalCli::PortalCli
               (uint param_1,undefined8 param_2,undefined8 param_3,undefined8 param_4,char *param_5)

{
  undefined1 auStackY_138 [32];
  char local_b8 [128];
  ulonglong local_38;
  
  local_38 = DAT_142551900 ^ (ulonglong)auStackY_138;
  if (DAT_142854cb0 != 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  Arena::Core::Platform::Common::Address(1,(undefined8 *)&DAT_142854c70,param_3,param_4);
  DAT_142854cac = (undefined4)param_3;
  DAT_142854cf0 = param_1;
  DAT_142854cb0 = FUN_14155f850(L"Cli2PortalAuth",&PTR_PTR_14256af60,400,0);
  if ((int)param_4 == 0) {
    if ((param_5 != (char *)0x0) && (*param_5 != '\0')) {
                    /* WARNING: Subroutine does not return */
      FUN_1409aaaf0(local_b8,param_5,0x80);
    }
    FUN_140fc79f0(1,"cligate",0,(ulonglong)param_1,0x80,local_b8);
  }
  else {
    DnsName::DnsName(1,"cligate",0,(ulonglong)param_1,0x80,local_b8);
  }
  FUN_1409ab7e0(local_b8,0xffffffffffffffff);
                    /* WARNING: Subroutine does not return */
  FUN_1409aaaf0(&DAT_142854d00,local_b8,0x80);
}

