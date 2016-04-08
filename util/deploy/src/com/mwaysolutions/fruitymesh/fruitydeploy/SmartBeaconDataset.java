package com.mwaysolutions.fruitymesh.fruitydeploy;

public class SmartBeaconDataset {
	public String serialNumber;
	public String networkKey;
	public Long chipID0;
	public Long chipID1;
	public Long magicNumber;
	public Long checksum;
	public Long boardType;
	
	public Boolean isValid(){
		//TODO: check for checksum
		if(magicNumber == 0xF077){
			return true;
		}
		return false;
	}
}
