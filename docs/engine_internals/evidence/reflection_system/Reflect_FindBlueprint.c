
ulonglong * Reflect_FindBlueprint(ulonglong *param_1,int param_2)

{
  ulonglong uVar1;
  uint uVar2;
  
  do {
    uVar1 = *param_1;
    if (((longlong)param_2 & uVar1) != 0) {
      uVar2 = param_2 - 1U & (uint)uVar1;
      uVar2 = uVar2 - (uVar2 >> 1 & 0x55555555);
      uVar2 = (uVar2 >> 2 & 0x33333333) + (uVar2 & 0x33333333);
      return param_1 + (ulonglong)(((uVar2 >> 4) + uVar2 & 0xf0f0f0f) * 0x1010101 >> 0x18) + 2;
    }
  } while (((uVar1 & 1) == 0) && (param_1 = (ulonglong *)param_1[1], param_1 != (ulonglong *)0x0));
  return (ulonglong *)0x0;
}

