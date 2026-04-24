
void Event::Dispatcher_ProcessEvent(undefined8 param_1,uint param_2)

{
  longlong lVar1;
  
  if ((param_2 != 0xffffffff) && (param_2 < (uint)DAT_14263f360)) {
    lVar1 = (**(code **)(*(longlong *)(&DAT_14263eb60)[param_2] + 0x10))();
                    /* WARNING: Subroutine does not return */
    FUN_1409a4530((int *)(lVar1 + 8));
  }
                    /* WARNING: Subroutine does not return */
  FUN_1409cd700("D:\\Perforce\\Live\\NAEU\\v2\\Code\\Arena\\Core\\Platform\\Windows\\Exe\\ExeMemFlex ible.cpp"
                ,0x5a3,0x141b63030,(ulonglong)param_2);
}

