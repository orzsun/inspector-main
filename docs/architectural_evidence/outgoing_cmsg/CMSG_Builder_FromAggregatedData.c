// [NEW_NAME]: CMSG_Builder_FromAggregatedData (FUN_141018E00)
// Purpose: This is a small wrapper function that receives the aggregated player
// action data and acts as the final step before serialization. It retrieves the
// global MsgConn context and passes the data to the generic schema-driven
// packet builder, `CMSG::BuildAndSendPacket`.

void CMSG_Builder_FromAggregatedData
               (undefined8 param_1,undefined8 param_2,uint param_3,ushort *param_4)
{
  byte *pbVar1;
  
  pbVar1 = (byte *)Arena::Core::Collections::Array(); // Likely gets the MsgConn context
  
  // Calls the generic packet builder which uses the schema system
  CMSG::BuildAndSendPacket(pbVar1,param_3,param_4);
}