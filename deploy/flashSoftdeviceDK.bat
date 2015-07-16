SET NRFJPROG_PATH=C:\nrf\tools\nrf51_tools\bin\nrfjprog.exe
SET SOFTDEVICE_HEX=C:\nrf\softdevices\sd130_1.0.0-prod\s130_nrf51_1.0.0_softdevice.hex

echo Parallel flashing Softdevice on DK
echo Softdevice path: %SOFTDEVICE_HEX%


SET SEGGER6=681702667


start /min %NRFJPROG_PATH% -s %SEGGER6% --erase --program %SOFTDEVICE_HEX%

echo "Flashing finished"