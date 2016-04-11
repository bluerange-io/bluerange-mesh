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

	public DatabaseManager(String host, String schema, String user, String password) {
		this.host = host;
		this.schema = schema;
		this.user = user;
		this.password = password;
		

	}
	
	public void connect(){
		// This will load the MySQL driver, each DB has its own driver
	      try {			
			connection = DriverManager.getConnection(host+schema+"?user="+user+"&password="+password);
		
	      
			//Prepare statements
			readBeaconsStatement = connection.prepareStatement("SELECT * FROM beacons WHERE uuid=?");
			insertBeaconStatement = connection.prepareStatement("INSERT INTO beacons"
					+ "(uuid, chipId, serialNumber, networkKey, "
					+ "boardId, seggerSerial, hardwareId, addressType,"
					+ "address, firmwareId, lastFlashed)"
					+" VALUES (?,?,?,?,?,?,?,?,?,?,NOW())");
			updateBeaconStatement = connection.prepareStatement("UPDATE beacons SET"
					+ " networkKey=?, boardId=?, seggerSerial=?, lastFlashed=NOW(), eraseCounter=?"
					);
					
			
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
		
		insertBeaconStatement.executeUpdate();

	}
	
	public void updateBeaconInDatabase(SmartBeaconDataset data) throws SQLException {
			
		updateBeaconStatement.setString(1, data.networkKey);
		updateBeaconStatement.setLong(2, data.boardId);
		updateBeaconStatement.setString(3, data.seggerSerial);
		updateBeaconStatement.setLong(4, data.eraseCounter);
		
		updateBeaconStatement.executeUpdate();
			
	}
}
