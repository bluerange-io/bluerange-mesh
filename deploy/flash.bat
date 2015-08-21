@echo off

echo Arguments:
echo     1: softdevice or fruitymesh or reset (what to flash)
echo     2: name of textfile which contains newline seperated segger ids
echo     3: Debug or Release (The folder from which to take the application hex)

REM setting some defaults

SET TYPE=fruitymesh
SET IDS=dongle_segger_ids.txt
SET BUILD=Debug

IF NOT [%1] == [] SET TYPE=%1
IF NOT [%2] == [] SET IDS=%2
IF NOT [%3] == [] SET BUILD=%3

SET EXEC_FOLDER=C:\nrf\projects\fruitymesh\deploy
SET NRFJPROG_PATH=C:\nrf\tools\nrf51_tools\bin\nrfjprog.exe

SET FRUITYMESH_HEX=C:\nrf\projects\fruitymesh\%BUILD%\FruityMesh.hex
SET SOFTDEVICE_HEX=C:\nrf\softdevices\sd130_1.0.0-prod\s130_nrf51_1.0.0_softdevice.hex

echo.
echo.

echo Parallel flashing on all dongles in %IDS% with %TYPE% and Build %BUILD%

echo.
echo.

echo IDs:

cd %EXEC_FOLDER%
for /f "tokens=* delims=" %%x in (%IDS%) do (

	echo | set /p dummyName=%%x, 

	if %TYPE% == fruitymesh (
		start /min %NRFJPROG_PATH% -s %%x --program %FRUITYMESH_HEX% -r
	)
	if %TYPE% == softdevice (
		start /min %NRFJPROG_PATH% -s %%x --erase --program %SOFTDEVICE_HEX%
	)
	if %TYPE% == reset (
		start /min nrfjprog -s %%x -r
	)

)

echo.
echo.
echo "Flashing finished"