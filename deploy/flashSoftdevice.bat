SET NRFJPROG_PATH=C:\nrf\tools\nrf51_tools\bin\nrfjprog.exe
SET SOFTDEVICE_HEX=C:\nrf\softdevices\sd130_1.0.0-prod\s130_nrf51_1.0.0_softdevice.hex

echo Parallel flashing Softdevice on all dongles
echo Softdevice path: %SOFTDEVICE_HEX%

SET SEGGER0=680194045
SET SEGGER1=680449458
SET SEGGER2=680990847
SET SEGGER3=680603635
SET SEGGER4=680837880
SET SEGGER5=680966072
SET SEGGER6=680084304
SET SEGGER7=680116493

start /min %NRFJPROG_PATH% -s %SEGGER0% --erase --program %SOFTDEVICE_HEX%
start /min %NRFJPROG_PATH% -s %SEGGER1% --erase --program %SOFTDEVICE_HEX%
start /min %NRFJPROG_PATH% -s %SEGGER2% --erase --program %SOFTDEVICE_HEX%
start /min %NRFJPROG_PATH% -s %SEGGER3% --erase --program %SOFTDEVICE_HEX%
start /min %NRFJPROG_PATH% -s %SEGGER4% --erase --program %SOFTDEVICE_HEX%
start /min %NRFJPROG_PATH% -s %SEGGER5% --erase --program %SOFTDEVICE_HEX%
start /min %NRFJPROG_PATH% -s %SEGGER6% --erase --program %SOFTDEVICE_HEX%
start /min %NRFJPROG_PATH% -s %SEGGER7% --erase --program %SOFTDEVICE_HEX%

echo "Flashing finished"