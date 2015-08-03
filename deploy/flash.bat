SET NRFJPROG_PATH=C:\nrf\tools\nrf51_tools\bin\nrfjprog.exe
SET SOFTDEVICE_HEX=C:\nrf\softdevices\sd130_1.0.0-prod\s130_nrf51_1.0.0_softdevice.hex

REM pass the folder name, either Debug or Release to this script file
if("%1" == "") SET 1 Debug

echo Parallel flashing on all dongles

SET SEGGER0=680194045
SET SEGGER1=680449458
SET SEGGER2=680990847
SET SEGGER3=680603635
SET SEGGER4=680837880
SET SEGGER5=680966072
SET SEGGER6=680084304
SET SEGGER7=680116493
SET SEGGER8=680554309

SET APPHEXFILE=..\%1\FruityMesh.hex

start /min %NRFJPROG_PATH% -s %SEGGER0% --program %APPHEXFILE% -r
start /min %NRFJPROG_PATH% -s %SEGGER1% --program %APPHEXFILE% -r
start /min %NRFJPROG_PATH% -s %SEGGER2% --program %APPHEXFILE% -r
start /min %NRFJPROG_PATH% -s %SEGGER3% --program %APPHEXFILE% -r
start /min %NRFJPROG_PATH% -s %SEGGER4% --program %APPHEXFILE% -r
start /min %NRFJPROG_PATH% -s %SEGGER5% --program %APPHEXFILE% -r
start /min %NRFJPROG_PATH% -s %SEGGER6% --program %APPHEXFILE% -r
start /min %NRFJPROG_PATH% -s %SEGGER7% --program %APPHEXFILE% -r
start /min %NRFJPROG_PATH% -s %SEGGER8% --program %APPHEXFILE% -r

echo "Flashing finished"