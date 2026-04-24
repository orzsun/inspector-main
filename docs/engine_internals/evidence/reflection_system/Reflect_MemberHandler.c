
void Reflect_MemberHandler
               (undefined *UNRECOVERED_JUMPTABLE,longlong param_2,longlong param_3,
               ulonglong *param_4,uint param_5,int param_6)

{
  ulonglong uVar1;
  longlong lVar2;
  
  uVar1 = readTypeTag(param_4);
  if (param_6 != (int)uVar1) {
    if (0 < (int)param_5) {
      lVar2 = param_3 - param_2;
      uVar1 = (ulonglong)param_5;
      do {
        (*(code *)UNRECOVERED_JUMPTABLE)(param_2,lVar2 + param_2,param_4,1);
        param_2 = param_2 + param_6;
        uVar1 = uVar1 - 1;
      } while (uVar1 != 0);
    }
    return;
  }
                    /* WARNING: Could not recover jumptable at 0x000141588c88. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (*(code *)UNRECOVERED_JUMPTABLE)(param_2,param_3,param_4,param_5);
  return;
}

