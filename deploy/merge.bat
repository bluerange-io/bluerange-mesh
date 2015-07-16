SET NRFJPROG_PATH=C:\nrf\tools\nrf51_tools\bin\
SET SOFTDEVICE_HEX=C:\nrf\softdevices\sd130_1.0.0-prod\s130_nrf51_1.0.0_softdevice.hex

REM pass the folder name, either Debug or Release to this script file
SET RELEASE_TYPE=%1
if [%1] == [] SET RELEASE_TYPE=Debug

SET APPHEXFILE=..\%RELEASE_TYPE%\FruityMesh.hex

echo Merging %SOFTDEVICE_HEX% with %RELEASE_TYPE%



%NRFJPROG_PATH%mergehex.exe --merge %SOFTDEVICE_HEX% %APPHEXFILE% --output FruityMeshWithS130.hex

echo "Finished"

PAUSE