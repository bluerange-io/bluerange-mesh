package com.mwaysolutions.fruitymesh.fruitydeploy;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;

public class DatabaseManager {
	private String host;
	private String user;
	private String password;
	private String schema;
	private Connection connection;

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
	
	public SmartBeaconDataset queryForSerialNumber(String serialNumber) throws SQLException{
		
		
		Statement stmt = connection.createStatement();
		ResultSet rs = stmt.executeQuery("SELECT * FROM beacons");

		while (rs.next()) {
		    String test = rs.getString("serialNumber");
		}
		
		
		return new SmartBeaconDataset();
	}
	
	public SmartBeaconDataset addBeaconToDatabase(SmartBeaconDataset data){
		
		//Add all data and data that was acquired during the test
		
		//INSERT INTO beacons (beaconType, serialNumber, networkKey) VALUES ('1', "BBSS8", "afaf9875a946b");
		//SELECT LAST_INSERT_ID();
		
		data.serialNumber = "BBSS8";
		
		//Returns the serial number that was inserted into the database
		return data;
	}
}
