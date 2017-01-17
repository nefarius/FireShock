@echo off
SET mypath=%~dp0

cd /d "%mypath%"

rem amd64
makecab /d SourceDir="%mypath%\..\x64\Release\fireshock" /d CabinetNameTemplate=FireShock_x64.cab /f FireShock.ddf
rem x86
rem makecab /d SourceDir="%mypath%\..\Release\ViGEm Drivers Redistributable" /d CabinetNameTemplate=ViGEm_x86.cab /f ViGEm.ddf

SignTool sign /v /ac "C:\Program Files (x86)\Windows Kits\10\CrossCertificates\GlobalSign Root CA.crt" /n "Wohlfeil.IT e.U." /t http://timestamp.verisign.com/scripts/timstamp.dll "%mypath%\Disk1\*.cab"
pause