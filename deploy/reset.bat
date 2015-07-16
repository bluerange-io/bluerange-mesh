echo Resetting Devices

SET SEGGER0=680194045
SET SEGGER1=680449458
SET SEGGER2=680990847
SET SEGGER3=680603635
SET SEGGER4=680837880
SET SEGGER5=680966072
SET SEGGER6=680084304
SET SEGGER7=680116493


start /min nrfjprog -s %SEGGER0% -r
start /min nrfjprog -s %SEGGER1% -r
start /min nrfjprog -s %SEGGER2% -r
start /min nrfjprog -s %SEGGER3% -r
start /min nrfjprog -s %SEGGER4% -r
start /min nrfjprog -s %SEGGER5% -r
start /min nrfjprog -s %SEGGER6% -r
start /min nrfjprog -s %SEGGER7% -r