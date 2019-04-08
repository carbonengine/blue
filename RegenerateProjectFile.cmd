@echo off
echo Checking out project and filters file
p4 edit blue.vcxproj
p4 edit blue.vcxproj.filters
REM p4 edit blue.v110_xp.vcxproj
REM p4 edit blue.v110_xp.vcxproj.filters
REM p4 edit blue.orbis.vcxproj
REM p4 edit blue.orbis.vcxproj.filters
echo Regenerating
..\..\..\..\..\..\shared_tools\python\27\python.exe ..\..\tools\ProjectFileGenerator\ProjectFileGenerator.py -i blue.ccpproj
REM ..\..\..\..\..\..\shared_tools\python\27\python.exe ..\..\tools\ProjectFileGenerator\ProjectFileGenerator.py -i blue.ccpproj --toolset=v110_xp
REM ..\..\..\..\..\..\shared_tools\python\27\python.exe ..\..\tools\ProjectFileGenerator\ProjectFileGenerator.py -i blue.ccpproj --orbis
pause