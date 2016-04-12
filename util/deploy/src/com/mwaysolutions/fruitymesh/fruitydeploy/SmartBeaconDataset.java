package com.mwaysolutions.fruitymesh.fruitydeploy;

import java.nio.ByteBuffer;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.UUID;


/*
 * memory layout


FICR (0x10000000)

DeviceID0 = 0x10000060
DeviceID1 = 0x10000064

HardwareID = 0x1000005C
DeviceAddressType = 0x100000A0
DeviceAddress0 = 0x100000A4
DeviceAddress1 = 0x100000A8


UICR (0x10001000)

FirmwareId = 0x10001010

magicNumber = 0x10001080
boardId = 0x10001084
serialNumber = 0x10001088 (2)
networkKey = 0x10001096 (16)
 * */

public class SmartBeaconDataset {
	//Database UUID unique to this beacon
	private UUID uuid;
	
	//FICR Data
	public String chipId;

	public Long hardwareId;
	public Long addressType;
	public String address;
	
	//UICR Data
	public Long firmwareId; // 
	
	public Long magicNumber; //0x10001080 (4 bytes)
	public Long boardId; //0x10001084 (4 bytes)
	public String serialNumber; //0x10001088 (5 bytes + 1 byte \0)
	public String networkKey; //0x10001096 (16 bytes)
	
	//Additional Data
	public Boolean readFromDatabase;

	public String seggerSerial;
	public Long eraseCounter = 0L;

	
	public static final Long MAGIC_NUMBER = 0xF07700L;
	
	public Boolean isValid(){
		//TODO: check for checksum
		if(magicNumber == MAGIC_NUMBER){
			return true;
		}
		return false;
	}
	
	public void setFICRData(ArrayList<Long> ficrData){
		
		//ChipID is read from Device ID and part of Encryption root, these are generated FIPS randomly
		//https://devzone.nordicsemi.com/question/59455/unique-id-of-a-device-needed/
		this.chipId = String.format("%08X", ficrData.get(24)) //DEVICEID0
				+ String.format("%08X", ficrData.get(25)) //DEVICEID1
				+ String.format("%08X", ficrData.get(32)) //Encryption Root 0
				+ String.format("%08X", ficrData.get(33)); //Encryption Root 1
		
		this.hardwareId = (ficrData.get(23) & 0x0000FFFFL);
		this.addressType = 0L;//ficrData.get(40);
		
		this.address = String.format("%08X", ficrData.get(41)) + String.format("%08X", ficrData.get(42));

	}
	
	public void setUICRData(ArrayList<Long> uicrData){
		
		this.firmwareId = uicrData.get(4);
		if(this.firmwareId.equals(0xFFFFFFFFL)) this.firmwareId = 0L;
		
		Long magicNumber = uicrData.get(32);
		
		if(magicNumber.equals(MAGIC_NUMBER)){
			this.magicNumber = MAGIC_NUMBER;
			this.boardId = uicrData.get(33);
						
			//Need to reverse String because of Little Endian
			this.serialNumber = new StringBuilder(HexDecUtils.longToASCIIString(uicrData.get(34))).reverse().toString();
			this.serialNumber += HexDecUtils.longToASCIIString(uicrData.get(35)).substring(3, 4);
			
			this.networkKey = "";
			for(int i=0; i<4; i++){
				this.networkKey += String.format("%08X", uicrData.get(36+i));
			}
		}
	}
	
	//Returns true if data from this chip has been read out by the debugger
	public Boolean isReadFromChip(){
		return magicNumber == MAGIC_NUMBER;
	}
	
	//Returns true if data of this beacon has been read from the database
	public Boolean isReadFromDatabase(){
		return readFromDatabase;
	}

	//Sets the serial of the segger debugger that was used to flash the chip
	public void setSeggerSerial(String serial) {
		this.seggerSerial = serial;
	}
	
	public UUID getUUID(){
		if(chipId == null) return null;
		
		if(this.uuid == null){
			byte[] randomBytes = HexDecUtils.hexStringToByteArray(chipId);
			
			randomBytes[6]  &= 0x0f;  /* clear version        */
	        randomBytes[6]  |= 0x40;  /* set to version 4     */
	        randomBytes[8]  &= 0x3f;  /* clear variant        */
	        randomBytes[8]  |= 0x80;  /* set to IETF variant  */
	        
	        long msb = 0;
	        long lsb = 0;
	        assert randomBytes.length == 16 : "data must be 16 bytes in length";
	        for (int i=0; i<8; i++)
	            msb = (msb << 8) | (randomBytes[i] & 0xff);
	        for (int i=8; i<16; i++)
	            lsb = (lsb << 8) | (randomBytes[i] & 0xff);
	        
	        this.uuid = new UUID(msb, lsb);
		}
		return this.uuid;
	}
	
	public String generateSerialForIndex(long index){
		String serial = "";
		String alphabet = "BCDFGHJKLMNPQRSTVWXYZ123456789";
		
		while(serial.length() < 5){			
			int rest = (int)(index % alphabet.length());
			serial += alphabet.substring(rest, rest+1);
			index /= alphabet.length();
		}
		
		return new StringBuilder(serial).reverse().toString();
	}
	
	public void generateRandomNetworkKey(){
		SecureRandom random = new SecureRandom();
		byte bytes[] = new byte[16];
		random.nextBytes(bytes);
		
		this.networkKey = HexDecUtils.byteArrayToHexString(bytes);
	}
	
	@Override
	public String toString() {
		String result = "------------------\n";
		result += "Beacon ("+serialNumber+") Segger ID: "+seggerSerial + " UniqueID: "+chipId+" UUID "+getUUID()+"\n";
		result += "boardType "+boardId+" hardware id "+String.format("0x%04X", hardwareId)+" networkKey "+networkKey+" eraseCounter "+eraseCounter+"\n";
		result += "------------------\n";
		
		return result;
	}
	

}
