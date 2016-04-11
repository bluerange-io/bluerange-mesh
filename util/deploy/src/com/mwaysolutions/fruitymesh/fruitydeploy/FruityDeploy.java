package com.mwaysolutions.fruitymesh.fruitydeploy;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

import com.beust.jcommander.JCommander;
import com.beust.jcommander.Parameter;
import com.beust.jcommander.Parameters;


@Parameters(separators = "=")
public class FruityDeploy {
	
	@Parameter(names={"--nrfjprogpath"},description="Path to Nordic nrfjprog Executable")
	String nrfjprogPath = "C:\\Program Files (x86)\\Nordic Semiconductor\\nrf5x\\bin\\nrfjprog.exe";
	
	@Parameter(names={"--jlinkpath"},description="Path to SEGGER JFlash Executable")
	String jlinkPath = "C:\\Program Files (x86)\\SEGGER\\JLink_V502a\\JFlash.exe";

	@Parameter(names = "--help", help=true, hidden=true)
	private boolean help;
	
	@Parameter(names ={"--serialnumbers", "-s"}, description="Space separated list of SEGGER serial numbers to flash", variableArity = true)
	public List<String> serialNumbers = new ArrayList<String>();
	
	@Parameter(names ={"--bootloader"}, description="Path to bootloader.hex")
	public String bootloaderHex = "C:\\nrf\\projects\\fruityloader\\Release\\FruityLoader.hex";
	
	@Parameter(names ={"--softdevice"}, description="Path to softdevice.hex")
	public String softdeviceHex = "C:\\nrf\\softdevices\\sd130_2.0.0-8.alpha\\s130_nrf51_2.0.0-8.alpha_softdevice.hex";

	@Parameter(names ={"--fruitymesh"}, description="Path to fruitymesh.hex")
	public String fruitymeshHex = "C:\\nrf\\projects\\fruitymesh\\Debug\\FruityMesh.hex";

	@Parameter(names ={"--family"}, description="Device Family (NRF51, NRF52)")
	public String family = "NRF51";
	
	@Parameter(names ={"--flash"}, description="Flash Fruitymesh and Softdevice")
	private Boolean flash = false;

	@Parameter(names ={"--loader"}, description="Flash loader")
	private Boolean loader = false;
	
	@Parameter(names ={"--reset"}, description="Reset Beacon")
	private Boolean reset = false;
	
	private final long NRF_FICR_CHIPID_BASE = 0x10000000;
	private final long NRF_UICR_CUSTOMER_BASE = 0x10001000;
	
	private final DatabaseManager db;
	
	
	public FruityDeploy(String[] args) {
		
			
		System.out.println("FruityDeploy starting");
		
		args = new String[0];//"--help".split(" ");
		
		db = new DatabaseManager("jdbc:mysql://localhost/", "beaconproduction", "root", "asdf");
		db.connect();
		
		//Parse command line arguments
		JCommander jCommander = new JCommander(this, args);
        jCommander.setProgramName("FruityDeploy");
        if (help) {
            jCommander.usage();
            return;
        }
        
        //if no serial numbers are given, get it from nrfjprog
        if(serialNumbers.size() == 0){
        	serialNumbers = callNrfjprogBlocking("--ids");
        }
        
        ArrayList<Thread> flashingThreads = new ArrayList<Thread>();
		
        //For every device, we spawn a flashing thread
		for(final String serial : serialNumbers){				
			Thread t = new Thread(new Runnable() {
				public void run() {
					ArrayList<String> tempResult;
					String result = "Beacon "+serial+": ";
					
					SmartBeaconDataset beaconData = new SmartBeaconDataset();
					
					//Read data from FICR
					tempResult = callNrfjprogBlocking("-s "+serial+" --family "+family+" --memrd "+NRF_FICR_CHIPID_BASE+" --w 32 --n "+(50*4));
					ArrayList<Long> ficrData = parseNrfjprogMemoryReadings(tempResult);

					beaconData.setFICRData(ficrData);

					//Read Data from UICR
					tempResult = callNrfjprogBlocking("-s "+serial+" --family "+family+" --memrd "+NRF_UICR_CUSTOMER_BASE+" --w 32 --n "+(40*4));
					ArrayList<Long> uicrData = parseNrfjprogMemoryReadings(tempResult);
					
					beaconData.setUICRData(uicrData);
					
					//Additional Data
					beaconData.setSeggerSerial(serial);
					
					//Query other data from database
					try {
						db.fillWithDatabaseInformation(beaconData);
					} catch (SQLException e1) {
						// TODO Auto-generated catch block
						e1.printStackTrace();
					}
					
					
					System.out.println(beaconData);

					beaconData.generateRandomNetworkKey();
					System.out.println(beaconData.networkKey);
					
					beaconData.generateRandomNetworkKey();
					System.out.println(beaconData.networkKey);
					
					if(beaconData.readFromDatabase){
					
						try {
							db.updateBeaconInDatabase(beaconData);
						} catch (SQLException e) {
							// TODO Auto-generated catch block
							e.printStackTrace();
						}
						
					} else {
						
						try {
							db.addBeaconToDatabase(beaconData);
						} catch (SQLException e) {
							// TODO Auto-generated catch block
							e.printStackTrace();
						}
						
					}
					

					
					if(true) return;
					
					/*
					if(beaconData.isValid()){
						//Keep this data
						//maybe increment a flashed-counter in the database
					} else {
						beaconData = db.addBeaconToDatabase(beaconData);
					}*/
									
					
					//What to save
					/*
					 Available: 32*4 Byte = 128 byte
					  0-7: serialNumber 
					  8-23: securityKey
					  24-31: 2-byte magic number 0xF077 + 2 byte checksum of serial number and security key
					  32-39: boardType (PCA10031, PCA10036, ARS100748)
					  40-47: flags
					  20 byte: available Sensors (1 byte each device)
					
					
					*/

					
					//Then, we flash them
					if(flash){
						//Softdevice
						result += "Softdevice: ";
						tempResult = callNrfjprogBlocking("-s "+serial+" --program "+softdeviceHex+" --chiperase --family "+family);
						if(String.join(", ", tempResult).contains("Programing device")) result += "OK, ";
						else result += "FAIL "+String.join(", ", tempResult);
						
						//Fruitymesh
						result += "Fruitymesh: ";
						tempResult = callNrfjprogBlocking("-s "+serial+" --program "+fruitymeshHex+" --family "+family);
						if(String.join(", ", tempResult).contains("Programing device")) result += "OK, ";
						else result += "FAIL "+String.join(", ", tempResult);
					}
					if(flash && loader){
						//FruityLoader
						result += "FruityLoader: ";
						tempResult = callNrfjprogBlocking("-s "+serial+" --program "+bootloaderHex+" --family "+family);
						if(String.join(", ", tempResult).contains("Programing device")) result += "OK, ";
						else result += "FAIL "+String.join(", ", tempResult);
					}
					if(flash || reset){
						//FruityLoader
						tempResult = callNrfjprogBlocking("-s "+serial+" --reset");
						result += "Reset: "+String.join(", ", tempResult);
					}
					
					//In the end, we need to write back the data to the beacon
					
					System.out.println(result);
				}
			});
			flashingThreads.add(t);
			t.start();
		}
		
		while(flashingThreads.size() > 0){
			Thread t = flashingThreads.get(0);
			try {
				t.join();
				flashingThreads.remove(0);
			} catch (InterruptedException e) {
			}
		}

		System.out.println("FruityDeploy finished");
	}
	
	ArrayList<String> callNrfjprogBlocking(String arguments){
		try {
			Process p = Runtime.getRuntime().exec(nrfjprogPath + " " + arguments);
			
			try {
				p.waitFor();
			} catch(InterruptedException  e){
				e.printStackTrace();
			}
			
			ArrayList<String> lines = new ArrayList<String>();
			String line;
			BufferedReader input = new BufferedReader(new InputStreamReader(p.getInputStream()));
			  while ((line = input.readLine()) != null) {
				  lines.add(line);
			  }
			  input.close();
			  
			  return lines;
			
		} catch (IOException e) {
			e.printStackTrace();
			
			return null;
		}
	}
	
	void callNrfjprogNonBlocking(String arguments){
		try {
			Process p = Runtime.getRuntime().exec(nrfjprogPath + " " + arguments);
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
	
	ArrayList<Long> parseNrfjprogMemoryReadings(ArrayList<String> lines){
		
		ArrayList<String> processedLines = new ArrayList<String>();
		
		//Prepare lines
		int numBytes = 0;
		for(String line : lines){
			line = line.replace(" ", "");
			line = line.substring(line.indexOf(':')+1, line.indexOf('|'));
			processedLines.add(line);
			numBytes += line.length() / 2;
		}
		
		ArrayList<Long> result = new ArrayList<Long>();
				
		for(String line : processedLines){
			for(int i=0; i<line.length(); i+=8){
				String numberString = line.substring(i, i+8);
				long l = Long.parseUnsignedLong(numberString, 16);
				result.add(l);
			}
		}
		
		return result;
	}
}
