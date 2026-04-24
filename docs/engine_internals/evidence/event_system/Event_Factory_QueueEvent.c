
void Event::Factory_QueueEvent
               (undefined1 param_1,ulonglong param_2,undefined8 param_3,int param_4,char *param_5,
               undefined4 param_6,uint param_7)

{
  undefined1 (*pauVar1) [32];
  undefined1 *puVar2;
  char *pcVar3;
  undefined1 auStackY_98 [32];
  char local_68 [64];
  ulonglong local_28;
  
  local_28 = DAT_142551900 ^ (ulonglong)auStackY_98;
  if (0x7fffffefffe < param_2) {
                    /* WARNING: Subroutine does not return */
    FUN_1409cd550();
  }
  pauVar1 = (undefined1 (*) [32])(*DAT_14263eac0)(param_2);
  if (param_4 != 0) {
                    /* WARNING: Subroutine does not return */
    FUN_1409a9a20(pauVar1,param_2);
  }
  if (pauVar1 == (undefined1 (*) [32])0x0) {
    Arena::Core::Basics::Str(local_68,0x40,0x141b63008,param_2 & 0xffffffff);
    pcVar3 = "(nullptr)";
    if (param_5 != (char *)0x0) {
      pcVar3 = param_5;
    }
    Arena::Core::Platform::Windows::Exe::Error::ExeError(5,local_68,pcVar3,param_6,0);
  }
  puVar2 = (undefined1 *)(*DAT_14263eae0)(pauVar1);
  *(short *)(puVar2 + 1) = (short)param_7;
  *puVar2 = param_1;
  Dispatcher_ProcessEvent(param_2,param_7);
  (*DAT_14263eab8)();
                    /* WARNING: Subroutine does not return */
  FUN_140e37790(local_28 ^ (ulonglong)auStackY_98);
}

