package com.mwaysolutions.fruitymesh.fruitydeploy;


public class Main {	
	public static void main(String ... args) {
		
		if(getJavaVersion() < 1.8){
			System.out.println("Java version must be 8 or newer in order to support unsigned long");
			return;
		}
		
		new FruityDeploy(args);
	}
	

	
	static double getJavaVersion () {
	    String version = System.getProperty("java.version");
	    int pos = version.indexOf('.');
	    pos = version.indexOf('.', pos+1);
	    return Double.parseDouble (version.substring (0, pos));
	}
}
