@echo off
@echo GBxCart RW v1.1 PCB - Firmware R4 Update
@echo ----------------------------------------
@echo:
@echo This will update your Firmware to R4.
@echo:

setlocal
SET /P COMPORT=Please enter your COM Port Number: 

SET /P AREYOUSURE=Are you sure you wish to update the Firmware (Y/[N])?
IF /I "%AREYOUSURE%" NEQ "Y" GOTO END

tsb\gbxcart_rw_wdt_reset_v1.0.exe %COMPORT%
tsb\tsb.exe com%COMPORT%:57600 fw gbxcart_rw_pcb_1.1_r4.hex
pause

:END
endlocal