## FruityMesh 0.3.50
New Features:
- New Terminal implementation, interrupt based and blocking
- Adds Button Handler
- Can be built using a simple makefile with GCC
- Adds ASSET mode for configuring the beacon as an asset with a buttonclick
- Implements some basic connection reconnection
- will now track assets
- adds macro for recurring timers
- allows segger RTT and UART simultaniously

Bugfixes:
- ModuleConfigs will now save with or without bootloader
- ignores non-mesh communication on other write handles
- uses deciseconds (1/10th seconds) to avoid timer overflows while enabling good enough resolution
- fixes bug with connection master bit handover
- various fixes

## FruityMesh 0.3

Important:

- There are some Bugs in the official SDK11 that need to be solved in order to compile Fruitymesh:
	- The file `SDK\components\softdevice\s130\headers\nrf_svc.h` has to be modified:
		- Line 55 should read like this: `#define GCC_CAST_CPP (uint16_t)`
	- The radio notification module is broken, use the fix that is described here:
		`https://devzone.nordicsemi.com/question/71636/ble_radio_notification-wont-compile/`

New Features:

- Upgraded Project to nRF5x SDK11 and S130 v2.0.0-prod
- Wireshark Dissector added (util/wireshark)
	- Can be used to dissect JOIN_ME messages and debug packets directly in Wireshark
- Segger RTT can be used instead of UART (Choose in Config.h)
- Clustering has been rewritten to be more reliable
	- Clustering Packets are now prioritized and clustering will not have to wait if other packets are sent in the mesh
	- Fixed some bugs where wrong data was sent
	- Cluster Size and Route to Sink should be more reliable now
- Custom bootloader can now be used with fruitymesh to enable Over-the-mesh firmware update (not on Github)
- Debugging has been improved (different LED codes for errors and retained error code variables)
- More error handling has been introduced
- Nearby nodes are now saved in a different buffer so they are not deleted by the clustering
- Added NewStorage class that allows to queue storage requests

Small changes:

- Config
	- Module instances have been removed to save space in packet headers (moduleID is now u8 instead of u16)
	- Activating and deactivating modules is now done in the config
	- Changed to way how different boards are set at compile time
	- Radio transmit power can be configured in Config
- Other
	- Connection states are now more easy to handle
	- Terminal Window Title (e.g. Putty) is now set to include some interesting information
- Debug module
	- Can now print the memory contents and cause a hardfault on request
- Advertising Module
	- Debug Advertising messages can be broadcasted to monitor the state of a node
- IO Module
	- LED_MODES does now include clustering (nodeIDs have to be set in Node.cpp for this to work)
- Scanning Module
	- Will now accumulate users and report the average of detected users in a timeslot
- StatusReporterModule
	- Now includes an initialized bit. This can be set after a Node has been configured. Will be set to 0 if a Node reboots.
- DFU Module
	- Has been removed from Github, DFU over-the-mesh is not open source currently

----------------------------------------------------
## Fruitymesh 0.1.9

- Did not have a changelog ;-)