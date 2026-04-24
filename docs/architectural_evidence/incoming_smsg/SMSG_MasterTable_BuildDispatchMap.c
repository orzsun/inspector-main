// FUN_00fd5eb0
void SMSG_MasterTable_BuildDispatchMap
               (int param_1,int param_2,uint param_3,longlong *param_4,uint param_5,
               longlong *param_6,undefined4 param_7)

{
  longlong lVar1;
  
  Gw2::Services::Msg::MsgChannel((ulonglong)param_3,param_4);
  Gw2::Services::Msg::MsgChannel(param_5,param_6);
  if (DAT_02854960 != 0) {
    EnterCriticalSection((LPCRITICAL_SECTION)&DAT_02854964);
  }
  lVar1 = FUN_00fd5800(param_1,param_2);
  if (param_3 != 0) {
    SMSG_DispatchMap_MapSchemaWithInternalID(lVar1,param_7,param_3,param_4);
  }
  if (param_5 != 0) {
    FUN_00fd5490(lVar1,param_7,param_5,param_6);
  }
  if (DAT_02854960 != 0) {
                    /* WARNING: Subroutine does not return */
    LeaveCriticalSection((LPCRITICAL_SECTION)&DAT_02854964);
  }
  return;
}

