-- This is a custom dissector for the FruityMesh Protocol in Wireshark
-- Autoloading is possible by editing the init.lua in the Wireshark Program directory
-- Copy this file in the wireshark directory and add the following line at the end: dofile("C:/path/to/this/file/fruitymesh.lua")
-- Afterwards start wireshark and the dissector should be active

-- Fur debugging arrays
function print_r(arr, indentLevel)
    local str = ""
    local indentStr = "#"

    if(indentLevel == nil) then
        print(print_r(arr, 0))
        return
    end

    for i = 0, indentLevel do
        indentStr = indentStr.."\t"
    end

    for index,value in pairs(arr) do
        if type(value) == "table" then
            str = str..indentStr..index..": \n"..print_r(value, (indentLevel + 1))
        else 
            str = str..indentStr..index..": "..value.."\n"
        end
    end
    return str
end

-- Create a new dissector
Proto_Fruitymesh = Proto("fruitymesh", "FruityMesh Protocol")
Proto_Fruitymesh_Debug = Proto("fruitymesh_debug", "FruityMesh Debug")

-- The dissector function
function Proto_Fruitymesh.dissector (buffer, pinfo, tree)
	nordic_dissector:call(buffer, pinfo, tree)
  
  local manufacturer_id = buffer(34, 2)
  local manufacturer_data = buffer(36, 23)
  
  -- check if this packet was sent from our manufacturer id
  if manufacturer_id(0, 2):uint() == 0x4d02 then
    -- check if it is a fruitymesh message
    if manufacturer_data(0, 1):uint() == 0xf0 then
    
      -- Add fruitymesh protocol to the tree
      local t = tree:add(Proto_Fruitymesh);
     
      -- Add values
      t:add(packet_identifier, manufacturer_data(0, 1):le_uint())
      t:add(network_id, manufacturer_data(1,2):le_uint())
      t:add(message_type, manufacturer_data(3,1):le_uint())
      t:add(sender_id, manufacturer_data(4,2):le_uint())
      local c = t:add(cluster_id, manufacturer_data(6,4):le_uint())
      c:add(cluster_id_node_id_part, manufacturer_data(6,2):le_uint())
      c:add(cluster_id_loss_part, manufacturer_data(8,2):le_uint())
      t:add(cluster_size, manufacturer_data(10,2):le_uint())
      
      local freeInOut = manufacturer_data(12,1):uint();
      local freeIn = bit.band(freeInOut, 0x07)
      local freeOut = bit.rshift(freeInOut, 3)
      
      t:add(free_in, freeIn)
      t:add(free_out, freeOut)
      
      t:add(batteryRuntime, manufacturer_data(13,1):le_uint())
      t:add(txPower, manufacturer_data(14,1):le_int())
      t:add(deviceType, manufacturer_data(15,1):le_uint())
      t:add(hopsToSink, manufacturer_data(16,2):le_uint())
      t:add(meshWriteHandle, manufacturer_data(18,2):le_uint())
      t:add(ackField, manufacturer_data(20,2):le_uint())
    end

    -- check if it is a debug message
    if manufacturer_data(0, 1):uint() == 0xde then
      local t = tree:add(Proto_Fruitymesh_Debug);

      t:add(packet_identifier, manufacturer_data(0,1):le_uint())
      t:add(sender_id, manufacturer_data(1,2):le_uint())
      t:add(debug_connLossCounter, manufacturer_data(3,2):le_uint())
      t:add(debug_partner1, manufacturer_data(5,2):le_uint())
      t:add(debug_partner2, manufacturer_data(7,2):le_uint())
      t:add(debug_partner3, manufacturer_data(9,2):le_uint())
      t:add(debug_partner4, manufacturer_data(11,2):le_uint())
      t:add(debug_rssiVal1, manufacturer_data(13,1):le_int())
      t:add(debug_rssiVal2, manufacturer_data(14,1):le_int())
      t:add(debug_rssiVal3, manufacturer_data(15,1):le_int())
      t:add(debug_rssiVal4, manufacturer_data(16,1):le_int())
      t:add(debug_droppedVal1, manufacturer_data(17,1):le_uint())
      t:add(debug_droppedVal2, manufacturer_data(18,1):le_uint())
      t:add(debug_droppedVal3, manufacturer_data(19,1):le_uint())
      t:add(debug_droppedVal4, manufacturer_data(20,1):le_uint())

    end


  end  
end

-- Create the protocol fields for the JOIN_ME message
packet_identifier = ProtoField.uint8("fruitymesh.packet_identifier","Mesh Identifier",base.HEX)
network_id = ProtoField.uint16("fruitymesh.network_id","Network Id",base.DEC)
message_type = ProtoField.uint8("fruitymesh.message_type","Message Type",base.DEC)
sender_id = ProtoField.uint16("fruitymesh.sender_id","Sender Id",base.DEC)
cluster_id = ProtoField.uint32("fruitymesh.cluster_id","Cluster Id",base.HEX)
cluster_id_node_id_part = ProtoField.uint16("fruitymesh.cluster_id_node_id_part","Node Id",base.DEC)
cluster_id_loss_part = ProtoField.uint16("fruitymesh.cluster_id_loss_part","Loss",base.DEC)
cluster_size = ProtoField.uint16("fruitymesh.cluster_size","Cluster Size",base.DEC)
free_in = ProtoField.uint8("fruitymesh.free_in","FreeIn",base.DEC)
free_out = ProtoField.uint8("fruitymesh.free_out","FreeOut",base.DEC)
batteryRuntime = ProtoField.uint8("fruitymesh.batteryRuntime","Battery",base.DEC)
txPower = ProtoField.int8("fruitymesh.txPower","TX Power",base.DEC)
deviceType = ProtoField.uint8("fruitymesh.deviceType","Device Type",base.DEC)
hopsToSink = ProtoField.uint16("fruitymesh.hopsToSink","Hops to Sink",base.DEC)
meshWriteHandle = ProtoField.uint16("fruitymesh.meshWriteHandle","Write Handle",base.DEC)
ackField = ProtoField.uint16("fruitymesh.ackField","Ack Field",base.DEC)

-- add the fields to the protocol
Proto_Fruitymesh.fields = {
  packet_identifier,
  network_id,
  message_type,
  sender_id,
  cluster_id,
  cluster_size,
  free_in,
  free_out,
  batteryRuntime,
  txPower,
  deviceType,
  hopsToSink,
  meshWriteHandle,
  ackField,
}

-- Create the protocol fields for the Debug Packet
debug_connLossCounter = ProtoField.uint16("fruitymesh_debug.connLossCounter","Loss Counter",base.DEC)
debug_partner1 = ProtoField.uint16("fruitymesh_debug.partner1","Partner Id 1",base.DEC)
debug_partner2 = ProtoField.uint16("fruitymesh_debug.partner2","Partner Id 2",base.DEC)
debug_partner3 = ProtoField.uint16("fruitymesh_debug.partner3","Partner Id 3",base.DEC)
debug_partner4 = ProtoField.uint16("fruitymesh_debug.partner4","Partner Id 4",base.DEC)
debug_rssiVal1 = ProtoField.int8("fruitymesh_debug.rssiVal1","RSSI 1",base.DEC)
debug_rssiVal2 = ProtoField.int8("fruitymesh_debug.rssiVal2","RSSI 2",base.DEC)
debug_rssiVal3 = ProtoField.int8("fruitymesh_debug.rssiVal3","RSSI 3",base.DEC)
debug_rssiVal4 = ProtoField.int8("fruitymesh_debug.rssiVal4","RSSI 4",base.DEC)
debug_droppedVal1 = ProtoField.uint8("fruitymesh_debug.droppedVal1","Dropped 1",base.DEC)
debug_droppedVal2 = ProtoField.uint8("fruitymesh_debug.droppedVal2","Dropped 2",base.DEC)
debug_droppedVal3 = ProtoField.uint8("fruitymesh_debug.droppedVal3","Dropped 3",base.DEC)
debug_droppedVal4 = ProtoField.uint8("fruitymesh_debug.droppedVal4","Dropped 4",base.DEC)

-- Ad the debug fields to the other protocol
Proto_Fruitymesh_Debug.fields = {
  debug_connLossCounter,
  debug_partner1,
  debug_partner2,
  debug_partner3,
  debug_partner4,
  debug_rssiVal1,
  debug_rssiVal2,
  debug_rssiVal3,
  debug_rssiVal4,
  debug_droppedVal1,
  debug_droppedVal2,
  debug_droppedVal3,
  debug_droppedVal4
}

-- Register the dissector
wtap_table = DissectorTable.get("wtap_encap")
nordic_dissector = wtap_table:get_dissector(55)
wtap_table:add (55, Proto_Fruitymesh)

debug("FruityMesh Dissector registered")

