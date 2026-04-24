
ulonglong Reflect_GetMemberHandler(ulonglong *param_1)

{
  ulonglong *puVar1;
  
  puVar1 = Reflect_FindBlueprint(param_1,0x400);
  if (puVar1 != (ulonglong *)0x0) {
    return *puVar1;
  }
  return 0;
}

