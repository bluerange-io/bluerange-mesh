### Notice to Mac OS X Users

You can flash the device using the JLinkExe command-utility that can be downloaded [here](https://www.segger.com/j-link-software.html).

To flash the device, use the following command line:

```
prompt > jlinkexe upload.jlink
```
The output will look similar to this:

```
SEGGER J-Link Commander V5.00l ('?' for help)
Compiled Aug  7 2015 15:49:26

Script file read successfully.
DLL version V5.00l, compiled Aug  7 2015 15:49:17
Firmware: J-Link OB-SAM3U128-V2-NordicSemi compiled Jun 16 2015 17:08:23
Hardware: V1.00
S/N: 681574174 
Emulator has Trace capability
VTarget = 3.300V
Info: TotalIRLen = ?, IRPrint = 0x..000000000000000000000000
Info: TotalIRLen = ?, IRPrint = 0x..000000000000000000000000
No devices found on JTAG chain. Trying to find device on SWD.
Info: Found SWD-DP with ID 0x0BB11477
Info: Found Cortex-M0 r0p0, Little endian.
Info: FPUnit: 4 code (BP) slots and 0 literal slots
Info: CoreSight components:
Info: ROMTbl 0 @ F0000000
Info: ROMTbl 0 [0]: F00FF000, CID: B105100D, PID: 000BB471 ROM Table
Info: ROMTbl 1 @ E00FF000
Info: ROMTbl 1 [0]: FFF0F000, CID: B105E00D, PID: 000BB008 SCS
Info: ROMTbl 1 [1]: FFF02000, CID: B105E00D, PID: 000BB00A DWT
Info: ROMTbl 1 [2]: FFF03000, CID: B105E00D, PID: 000BB00B FPB
Info: ROMTbl 0 [1]: 00002000, CID: B105900D, PID: 000BB9A3 ???
Cortex-M0 identified.
Target interface speed: 100 kHz
Processing script file...

Info: Device "NRF51822_XXAA" selected.
Reconnecting to target...
Info: Found SWD-DP with ID 0x0BB11477
Info: Found Cortex-M0 r0p0, Little endian.
Info: FPUnit: 4 code (BP) slots and 0 literal slots
Info: CoreSight components:
Info: ROMTbl 0 @ F0000000
Info: ROMTbl 0 [0]: F00FF000, CID: B105100D, PID: 000BB471 ROM Table
Info: ROMTbl 1 @ E00FF000
Info: ROMTbl 1 [0]: FFF0F000, CID: B105E00D, PID: 000BB008 SCS
Info: ROMTbl 1 [1]: FFF02000, CID: B105E00D, PID: 000BB00A DWT
Info: ROMTbl 1 [2]: FFF03000, CID: B105E00D, PID: 000BB00B FPB
Info: ROMTbl 0 [1]: 00002000, CID: B105900D, PID: 000BB9A3 ???

Target interface speed: 1000 kHz

Reset delay: 0 ms
Reset type NORMAL: Resets core & peripherals via SYSRESETREQ & VECTRESET bit.

Downloading file [FruityMesh_0.1.7_app_only.hex]...Info: J-Link: Flash download: Flash download skipped. Flash contents already match
O.K.

Reset delay: 0 ms
Reset type NORMAL: Resets core & peripherals via SYSRESETREQ & VECTRESET bit.



Script processing completed.

```