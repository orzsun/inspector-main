
void Reflect_CopyFunction(undefined8 *param_1,undefined8 *param_2,ulonglong *param_3,int param_4)

{
  ulonglong uVar1;
  
  uVar1 = readTypeTag(param_3);
                    /* WARNING: Subroutine does not return */
  Reflect_MemmoveDispatcher(param_1,param_2,(longlong)((int)uVar1 * param_4));
}

