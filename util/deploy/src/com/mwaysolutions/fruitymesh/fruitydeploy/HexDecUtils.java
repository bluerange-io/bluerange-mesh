package com.mwaysolutions.fruitymesh.fruitydeploy;

import java.nio.ByteBuffer;

public class HexDecUtils {
	public static byte[] hexStringToByteArray(String s) {
	    int len = s.length();
	    byte[] data = new byte[len / 2];
	    for (int i = 0; i < len; i += 2) {
	        data[i / 2] = (byte) ((Character.digit(s.charAt(i), 16) << 4)
	                             + Character.digit(s.charAt(i+1), 16));
	    }
	    return data;
	}
	
	public static String byteArrayToHexString(byte[] bytes){
		char[] hexArray = "0123456789ABCDEF".toCharArray();
		char[] hexChars = new char[bytes.length * 2];
	    for ( int j = 0; j < bytes.length; j++ ) {
	        int v = bytes[j] & 0xFF;
	        hexChars[j * 2] = hexArray[v >>> 4];
	        hexChars[j * 2 + 1] = hexArray[v & 0x0F];
	    }
	    return new String(hexChars);
	}
	
	public static String longToASCIIString(long l){
		return new String( ByteBuffer.allocate(8).putLong(l).array()).substring(4, 8);
	}
	
	public static long ASCIIStringToLong(String s){
		if(s.length() < 1 || s.length() > 4) throw new RuntimeException("Wrong length");
		
		char[] chars = s.toCharArray();
		long r = 0;
		for(int i=0; i< 4; i++){
			if(chars.length > i) r += (short)chars[i] << (8*i);
		}

		return r;
	}
}
