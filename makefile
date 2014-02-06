BITS = 64
PROJECT = blue

all: clang

gcc: $(PROJECT).linux$(BITS).g++.makefile
	ProjectFileGenerator.py -i $(PROJECT).ccpproj --linux$(BITS) --toolset=g++
	make -f $(PROJECT).linux$(BITS).g++.makefile -j 8

$(PROJECT).linux$(BITS).g++.makefile:

clang: $(PROJECT).linux$(BITS).clang++.makefile
	ProjectFileGenerator.py -i $(PROJECT).ccpproj --linux$(BITS) --toolset=clang++
	make -f $(PROJECT).linux$(BITS).clang++.makefile -j 8

$(PROJECT).linux$(BITS).clang++.makefile:

