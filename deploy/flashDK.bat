SET NRFJPROG_PATH=C:\nrf\tools\nrf51_tools\bin\nrfjprog.exe
SET SOFTDEVICE_HEX=C:\nrf\softdevices\sd130_1.0.0-prod\s130_nrf51_1.0.0_softdevice.hex

echo flashing DK


SET SEGGER6=681702667

SET APPHEXFILE=..\Release\FruityMesh.hex


start /min %NRFJPROG_PATH% -s %SEGGER6% --program %APPHEXFILE% -r

echo "Flashing finished"