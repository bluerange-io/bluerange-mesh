package com.mwaysolutions.fruitymesh.fruitydeploy;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.UUID;

import java.sql.PreparedStatement;

public class DatabaseManager {
	private String host;
	private String user;
	private String password;
	private String schema;
	private Connection connection;

	private PreparedStatement readBeaconsStatement;
	private PreparedStatement updateBeaconStatement;
	private PreparedStatement insertBeaconStatement;
	private PreparedStatement getNewSerialNumberIndexStatement;

	public DatabaseManager(String host, String schema, String user, String password) {
		this.host = host;
		this.schema = schema;
		this.user = user;
		this.password = password;
		

	}
	
	public void connect(){
		// This will load the MySQL driver, each DB has its own driver
	      try {			
			connection = DriverManager.getConnection(host+schema+"?user="+user+"&password="+password+"&allowMultiQueries=true");
		
	      
			//Prepare statements
			readBeaconsStatement = connection.prepareStatement("SELECT * FROM beacons WHERE uuid=?");
			
			insertBeaconStatement = connection.prepareStatement("INSERT INTO beacons"
					+ "(uuid, chipId, serialNumber, networkKey, "
					+ "boardId, seggerSerial, hardwareId, addressType,"
					+ "address, firmwareId, lastFlashed, eraseCounter, fruitydeployVersion, "
					+ "family, softdeviceVersion, fruitymeshVersion)"
					+" VALUES (?,?,?,?,?,?,?,?,?,?,NOW(),0, ?, ?, ?, ?)");
			
			updateBeaconStatement = connection.prepareStatement("UPDATE beacons SET"
					+ " networkKey=?, boardId=?, seggerSerial=?, lastFlashed=NOW(),"
					+ "eraseCounter=?, fruitydeployVersion=?, family=?, softdeviceVersion=?, "
					+ "fruitymeshVersion=?"
					+ " WHERE uuid=?");
			
			getNewSerialNumberIndexStatement = connection.prepareStatement("BEGIN; "
					+ "SELECT counter FROM serialcounter WHERE id = 0 FOR UPDATE; "
					+ "UPDATE serialcounter SET counter = counter+1 WHERE id = 0; "
					+ "COMMIT;");
					
			
	      } catch (SQLException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	      
	}
	
	public void disconnect(){
		if (connection != null) {
	        try {
	            connection.close();
	            connection = null;
	        } catch (SQLException e) {
	            e.printStackTrace();
	        }
	    }
	}
	
	public SmartBeaconDataset fillWithDatabaseInformation(SmartBeaconDataset beacon) throws SQLException{
		
		
		Statement stmt = connection.createStatement();
		ResultSet rs = stmt.executeQuery("SELECT * FROM beacons WHERE uuid='"+beacon.getUUID()+"'");
		
		if(rs.next()){
			beacon.boardId = rs.getLong("boardId");
			beacon.serialNumber = rs.getString("serialNumber");
			beacon.networkKey = rs.getString("networkKey");
			
			
			beacon.eraseCounter = rs.getLong("eraseCounter");
			
			beacon.readFromDatabase = true;
		} else {
			beacon.readFromDatabase = false;
		}
		
		return beacon;
	}
	
	public void addBeaconToDatabase(SmartBeaconDataset data) throws SQLException {

		insertBeaconStatement.clearParameters();
		insertBeaconStatement.setString(1, data.getUUID().toString());
		insertBeaconStatement.setString(2, data.chipId);
		insertBeaconStatement.setString(3, data.serialNumber);
		insertBeaconStatement.setString(4, data.networkKey);
		insertBeaconStatement.setLong(5, data.boardId);
		insertBeaconStatement.setString(6, data.seggerSerial);
		insertBeaconStatement.setLong(7, data.hardwareId);
		insertBeaconStatement.setLong(8, data.addressType);
		insertBeaconStatement.setString(9, data.address);
		insertBeaconStatement.setLong(10, data.firmwareId);
		insertBeaconStatement.setInt(11, FruityDeploy.VERSION);
		insertBeaconStatement.setString(12, data.deviceFamily);
		insertBeaconStatement.setLong(13, data.softdeviceVersion);
		insertBeaconStatement.setLong(14, data.fruitymeshVersion);
		
		insertBeaconStatement.executeUpdate();

	}
	
	public void updateBeaconInDatabase(SmartBeaconDataset data) throws SQLException {
			
		updateBeaconStatement.setString(1, data.networkKey);
		updateBeaconStatement.setLong(2, data.boardId);
		updateBeaconStatement.setString(3, data.seggerSerial);
		updateBeaconStatement.setLong(4, data.eraseCounter);
		updateBeaconStatement.setInt(5, FruityDeploy.VERSION);
		updateBeaconStatement.setString(6, data.deviceFamily);
		updateBeaconStatement.setLong(7, data.softdeviceVersion);
		updateBeaconStatement.setLong(8, data.fruitymeshVersion);
		
		updateBeaconStatement.setString(9, data.getUUID().toString());
		
		updateBeaconStatement.executeUpdate();
			
	}

	public long getNewSerialNumberIndex() throws SQLException{
		
		ResultSet rs;
		try{
			connection.createStatement().executeQuery("BEGIN");
			rs = connection.createStatement().executeQuery("SELECT counter FROM serialcounter WHERE id=0 FOR UPDATE");
			connection.createStatement().executeUpdate("UPDATE serialcounter SET counter = counter+1 WHERE id=0");
			connection.createStatement().executeQuery("COMMIT");
		} catch(SQLException e){
			connection.createStatement().executeQuery("ROLLBACK");
			throw new SQLException();
		}
				
		if(rs != null && rs.next()){
			return rs.getLong("counter");
		}
		
		return -1;
	}
}
