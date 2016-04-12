package com.mwaysolutions.fruitymesh.fruitydeploy;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;


public class Main {	
	public static void main(String ... args) {
		
		if(getJavaVersion() < 1.8){
			System.out.println("Java version must be 8 or newer in order to support unsigned long");
			return;
		}
		
		args[0] = "interactive";
		
		if(args.length > 1 && args[0].equals("interactive")){
			System.out.println("Starting interactive mode");
			while(true){
				try {
					BufferedReader in = new BufferedReader(new InputStreamReader(System.in));
					System.out.print("Enter command: ");
				    String s = in.readLine();
				    
				    if(s != null){
				    	if(s.equals("exit")){
				    		break;
				    	}
	
						String[] interactiveArgs = s.split(" ");
						new FruityDeploy(interactiveArgs);
				    	
				    }
				} catch(IOException e){
					e.printStackTrace();
				}
			}
		} else {
			new FruityDeploy(args);
		}
		
	}
	

	
	static double getJavaVersion () {
	    String version = System.getProperty("java.version");
	    int pos = version.indexOf('.');
	    pos = version.indexOf('.', pos+1);
	    return Double.parseDouble (version.substring (0, pos));
	}
}
