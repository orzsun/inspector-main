
void Reflect_FieldInitDispatcher
               (longlong param_1,longlong param_2,ulonglong *param_3,uint param_4,int param_5)

{
  ulonglong *puVar1;
  undefined *UNRECOVERED_JUMPTABLE;
  
  puVar1 = Reflect_FindBlueprint(param_3,0x400);
  if (puVar1 == (ulonglong *)0x0) {
    UNRECOVERED_JUMPTABLE = (undefined *)0x0;
  }
  else {
    UNRECOVERED_JUMPTABLE = (undefined *)*puVar1;
  }
  Reflect_MemberHandler(UNRECOVERED_JUMPTABLE,param_1,param_2,param_3,param_4,param_5);
  return;
}

