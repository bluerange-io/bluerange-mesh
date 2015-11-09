# Custom module examples

## RSSIModule

This is example is based on the PingModule example and shows how to get signal RSSI value changes to an RGB LED connected to a node.
Use two or more nodes with one "mobile" (connected to a cellphone battery, for example).
Wait for the nodes to join the mesh and then slowly walk away from the one connected to the LED. If all goes well, is should turn from green to orange and then red as the signal strength changes.

### Disclaimer

This is a quick-and-dirty implementation! Some improvements can be made by derving it from the PingModule, and sharing RSSI code with StatusReporterModule by moving RSSI functions to a common base class.
The latter was not done as at the time of this writing, there's a ticket open that will change RSSI handling.

### Changed files

#### RSSIModule.cpp

Module file, basically copied from PingModule that adds RSSI polling as seen in StatusReporterModule:
- Implement MeshConnectionChangedHandler to trigger measuring RSSI
- Implement StartConnectionRSSIMeasurement to start measuring RSSI
- Implement BleEventHandler to update RSSI info for our node
- Implement update_led_colour to update LED colours of the RGB LEF based on arbitrary RSSI values.
- Added ```makefile.rssi``` that builds FruityMesh with the module incorporated

#### Config.h

Added module ID for the RSSI module

#### Node.cpp

Added support for RSSI module (include RSSIModule.h, added module to active module array)

#### Makefile

To build this example add the module to the makefile list:

```
CPP_SOURCE_FILES += ./src_examples/RSSIModule.cpp
```

And add its include pathL

```
INC_PATHS += -I./src_examples
```

If you want to work and customise this example further, you should move its files to ```./src``` and modify the makefile accordingly.

### RGB LED setup

[Wire your node to an RGB using this schema]
(https://cloud.githubusercontent.com/assets/19006/11022371/fc4614e8-862a-11e5-9618-0ed1a32d75aa.png)

