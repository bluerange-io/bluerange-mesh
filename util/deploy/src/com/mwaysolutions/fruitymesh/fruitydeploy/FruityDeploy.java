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
	public String softdeviceHex = "C:\\nrf\\softdevices\\sd130_2.0.0-prod\\s130_nrf51_2.0.0_softdevice.hex";

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
	
	@Parameter(names ={"--verify"}, description="Verify what has been written")
	private Boolean verify = false;
	
	@Parameter(names ={"--modSerial"}, description="Usage: --modSerial <serialNumber> to write custom serial to UICR")
	public String modSerial = null;
	
	@Parameter(names ={"--modBoardId"}, description="Usage: --modBoardId <boardId> to write custom boardId to UICR")
	public Long modBoardId = null;
	
	public static final int VERSION = 1;
	
	private final long NRF_FICR_CHIPID_BASE = 0x10000000;
	private final long NRF_UICR_CUSTOMER_BASE = 0x10001000;
	private final long NRF_SOFTDEVICE_VERSION_BASE = 0x3008;
	
	private final DatabaseManager db;
	
	
	public FruityDeploy(String[] args) {
		
			
		System.out.println("FruityDeploy starting");
				
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
					
					SmartBeaconDataset beaconData = new SmartBeaconDataset();
					
					beaconData.deviceFamily = family;
					
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
					
					//Check if a manually entered serialNumber should be used / modified
					if(modSerial != null){
						beaconData.serialNumber = modSerial;
						beaconData.manualInit = true;
					}
					
					//Check if entered boardId should be used or modified
					if(modBoardId != null){
						beaconData.boardId = modBoardId;
						beaconData.manualInit = true;
					}
					
					System.out.println("READ:"+ beaconData);
					
					//Then, we flash them
					String result = "Beacon "+serial+": ";
					
					if(flash){
						
						//Try to generate a serialNumber from the Database if none exists
						if (beaconData.serialNumber == null){
							try {
								long index = db.getNewSerialNumberIndex();
								if(index >= 0){
									beaconData.serialNumber = beaconData.generateSerialForIndex(index);
								}
							} catch (SQLException e1) {
								e1.printStackTrace();
							}
						}
						
						//Generate a network key by random if none exists
						if(beaconData.networkKey == null){
							beaconData.generateRandomNetworkKey();
						}
						
						//Softdevice + Full Erase
						result += "Softdevice: ";
						tempResult = callNrfjprogBlocking("-s "+serial+" --program "+softdeviceHex+" --chiperase --family "+family);
						if(String.join(", ", tempResult).contains("Programing device")) result += "OK, ";
						else result += "FAIL "+String.join(", ", tempResult);
						
						//Fruitymesh
						result += "Fruitymesh: ";
						tempResult = callNrfjprogBlocking("-s "+serial+" --program "+fruitymeshHex+" --family "+family);
						if(String.join(", ", tempResult).contains("Programing device")) result += "OK, ";
						else result += "FAIL "+String.join(", ", tempResult);
						
						//Write UICR
						writeUICRDataBlocking(beaconData);
						
						//Get SoftDevice version
						tempResult = callNrfjprogBlocking("-s "+serial+" --family "+family+" --memrd "+NRF_SOFTDEVICE_VERSION_BASE+" --w 32 --n 8");
						ArrayList<Long> softdeviceVersionData = parseNrfjprogMemoryReadings(tempResult);
						long softdeviceSize = softdeviceVersionData.get(0);
						beaconData.softdeviceVersion = softdeviceVersionData.get(1) & 0xFFFF;
						
						long appInfoAddress = softdeviceSize + 1024L/*iVector*/;
						
						//Get Fruitymesh version
						tempResult = callNrfjprogBlocking("-s "+serial+" --family "+family+" --memrd "+appInfoAddress+" --w 32 --n 16");
						ArrayList<Long> appInfo = parseNrfjprogMemoryReadings(tempResult);
						beaconData.fruitymeshVersion = appInfo.get(0);
						
						
						//Now we update or insert the beacon into our database
						try {
							if(beaconData.readFromDatabase){
								beaconData.eraseCounter++;
								db.updateBeaconInDatabase(beaconData);
							} else {
								db.addBeaconToDatabase(beaconData);
							}
						} catch (SQLException e) {
							e.printStackTrace();
						}
						
					}
					if(flash && loader){
						//FruityLoader
						result += "FruityLoader: ";
						tempResult = callNrfjprogBlocking("-s "+serial+" --program "+bootloaderHex+" --family "+family);
						if(String.join(", ", tempResult).contains("Programing device")) result += "OK, ";
						else result += "FAIL "+String.join(", ", tempResult);
					}
					if(flash || reset){
						//Reset Beacon
						tempResult = callNrfjprogBlocking("-s "+serial+" --reset");
						result += "Reset: "+String.join(", ", tempResult);
					}
					
					if(verify){
						System.out.println("Verify not yet supported");
					}
					
					//In the end, we need to write back the data to the beacon
					
					System.out.println(result+" END");
				}

				private void writeUICRDataBlocking(SmartBeaconDataset beaconData) {
					/*public Long magicNumber; //0x10001080 (4 bytes)
					public Long boardId; //0x10001084 (4 bytes)
					public String serialNumber; //0x10001088 (5 bytes + 1 byte \0)
					public String networkKey; //0x10001096 (16 bytes)*/
					
					beaconData.magicNumber = SmartBeaconDataset.MAGIC_NUMBER;
										
					ArrayList<String> tempResult;
					tempResult = callNrfjprogBlocking("-s "+beaconData.seggerSerial+" --memwr 0x10001080 --val "+SmartBeaconDataset.MAGIC_NUMBER);
					tempResult = callNrfjprogBlocking("-s "+beaconData.seggerSerial+" --memwr 0x10001084 --val "+beaconData.boardId);
					
					//Serial number
					if(beaconData.serialNumber != null && !beaconData.serialNumber.equals("")){
						tempResult = callNrfjprogBlocking("-s "+beaconData.seggerSerial+" --memwr 0x10001088 --val "+HexDecUtils.ASCIIStringToLong(beaconData.serialNumber.substring(0, 4)));
						tempResult = callNrfjprogBlocking("-s "+beaconData.seggerSerial+" --memwr 0x1000108C --val "+HexDecUtils.ASCIIStringToLong(beaconData.serialNumber.substring(4, 5)));
					}
				}
			});
			flashingThreads.add(t);
			t.start();
		}
		
		//Wait for all flashing threads to finish
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
	
	//This method calls nrfjprog and waits until it is finished
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
