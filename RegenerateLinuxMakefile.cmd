@echo off
echo Checking out project and filters file
p4 edit blue.linux.makefile
echo Regenerating
\python\2.7.3\windows\win32\python.exe ..\..\ProjectFileGenerator\ProjectFileGenerator.py -i blue.ccpproj -m -l
pause