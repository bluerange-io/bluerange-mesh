![fruitymesh](https://cloud.githubusercontent.com/assets/5893428/9224084/1687644e-4100-11e5-93d3-02df8951ee91.png)

FruityMesh is the reference implementation of the **BlueRange** initiative. It is the first open source implementation of a **mesh network** that is based on standard Bluetooth Low Energy 4.1 connections. In contrast to meshes that use advertising and scanning, this allows for a network run with battery powered devices. FruityMesh works with the **Nordic nRF51** and **nRF52** chipsets in combination with the **S130/S132 SoftDevices** and enables tons of devices to connect to each other with its auto-meshing capabilities.

# BETA RELEASE
This is a beta version of FruityMesh that includes a set of new Features not available in the currently published master:
- **MultiAdvertister**: Does support a number of advertising jobs that it will schedule in slots
- **FruityHal**: Small HAL that will be used to abstract platform specific functions over time
- **nRF52 SDK 14 compatibility**: This release compiles with SDK14 for nRF52 and SDK11 for nRF51. The Softdevice S132 5.0 must be used for the nRF52.
- **AppConnections**: Includes an abstraction Layer for manaaging not only mesh connections, but different connections to offer services for e.g. Smartphones
- **RecordStorage**: Implements a small file system for providing power-loss safe storage of module configurations
- **Refactored**: A lot.
- **Bugfixes**: Countless
- **Watchdog**: Basic Watchdog implementation, including Safe boot mode.
- **Fast packet splitting**: Packets are now splitted using WRITE_CMD which allows a lot more throughput over the mesh

There is currently not much documentation available for this release, so contact us if anything is not clear.

# BETA RELEASE SETUP
- In order to compile this release, you need the nRF5 11 SDK for nRF51 and the nRF5 14 SDK for nRF52.
- Setup steps for the nRF51 SDK are documented in the Make toolchain.pdf file
- For the nRF52 SDK, you need to adjust three more things:
	- Change the file /components/toolchain/gcc/Makefile.posix/windows to point to your compiler
	- In the components/toolchain/gcc folder, the gcc_startup*.S files need to be renamed with a lowercase .s
	- The /components/ble/ble_radio_notification/ble_radio_notification.h file is missing an import: #include "nrf_nvic.h"


# Documentation in the Wiki
[![Documentation](https://cloud.githubusercontent.com/assets/5893428/8722473/5a89169c-2bc5-11e5-9aea-02a16b3b189e.png)](https://github.com/mwaylabs/fruitymesh/wiki)

Documentation is the key and therefore, you should have a look at the wiki to get you started:
[https://github.com/mwaylabs/fruitymesh/wiki](https://github.com/mwaylabs/fruitymesh/wiki)

# Development
This project is under active development at the [M-Way Solutions GmbH](http://www.mwaysolutions.com/), Germany.

#Additional Features
Over-the-mesh firmware updates are currently not open sourced. Interested parties can contact sales@mwaysolutions.com to get access to advanced features.
