@echo off

echo Arguments:
echo     1: softdevice or fruitymesh or erase or reset (what to flash)
echo     2: name of textfile which contains newline seperated segger ids (or current.txt to flash all connected devices)
echo     3: Debug or Release (The folder from which to take the application hex)
echo     4: NRF51 or NRF52 (device family to flash)

REM setting some defaults

SET TYPE=fruitymesh
SET IDS=dongle_segger_ids.txt
SET BUILD=Debug
SET FAMILY=NRF51

IF NOT [%1] == [] SET TYPE=%1
IF NOT [%2] == [] SET IDS=%2
IF NOT [%3] == [] SET BUILD=%3
IF NOT [%4] == [] SET FAMILY=%4

SET EXEC_FOLDER=C:\nrf\projects\fruitymesh\deploy
SET NRFJPROG_PATH=C:\nrf\tools\nrf_tools\nrfjprog.exe

SET FRUITYMESH_HEX=C:\nrf\projects\fruitymesh\%BUILD%\FruityMesh.hex
SET SOFTDEVICE_HEX=C:\nrf\softdevices\sd130_1.0.0-prod\s130_nrf51_1.0.0_softdevice.hex

echo.
echo.

echo Parallel flashing on all dongles in %IDS% with %TYPE% and Build %BUILD%

echo.
echo.

echo IDs:

if %IDS% == current.txt (
	REM Get currently connected IDs
	%NRFJPROG_PATH% --ids > current.txt
)


cd %EXEC_FOLDER%
for /f "tokens=* delims=" %%x in (%IDS%) do (

	echo | set /p dummyName=%%x, 

	if %TYPE% == fruitymesh (
		start /min cmd /C "%NRFJPROG_PATH% -s %%x --program %FRUITYMESH_HEX% --sectorerase --family %FAMILY% && %NRFJPROG_PATH% -s %%x --reset"
	)
	if %TYPE% == softdevice (
		start /min %NRFJPROG_PATH% -s %%x --program %SOFTDEVICE_HEX% --chiperase --family %FAMILY%
	)
	if %TYPE% == full (
		start /min cmd /C "%NRFJPROG_PATH% -s %%x --program %SOFTDEVICE_HEX% --chiperase --family %FAMILY% && %NRFJPROG_PATH% -s %%x --program %FRUITYMESH_HEX% --family %FAMILY% && %NRFJPROG_PATH% -s %%x --reset"
	)
	if %TYPE% == erase (
		start /min nrfjprog -s %%x --eraseall --family %FAMILY%
	)
	if %TYPE% == reset (
		start /min nrfjprog -s %%x --reset --family %FAMILY%
	)

)

echo.
echo.
echo "Flashing finished"