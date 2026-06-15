# Copyright © 2014 CCP ehf.

import blue
import random
import logging

FAILED_OPEN = 0.01
FAILED_LOAD = 0.02
SHORT_LOAD = 0.4

FAILED_PREPARE = 0.01
SHORT_PREPARE = 0.7

def GatherResourceNames( count ):
    resourceNames = []
    for i in range(count):
        load = random.random()
        prepare = random.random()

        if load < FAILED_OPEN:
            loadChar = 'f'
        else:
            load -= FAILED_OPEN
            if load < FAILED_LOAD:
                loadChar = 'n'
            else:
                load -= FAILED_LOAD
                if load < SHORT_LOAD:
                    loadChar = 's'
                else:
                    loadChar = 'l'

        if prepare < FAILED_PREPARE:
            prepChar = 'f'
        else:
            prepare -= FAILED_PREPARE
            if prepare < SHORT_PREPARE:
                prepChar = 's'
            else:
                prepChar = 'l'

        name = loadChar + prepChar + str(i) + ".blueasync"
        resourceNames.append(name)

    return resourceNames

allResources = GatherResourceNames(5000)
random.shuffle( allResources )

slices = []
while len( allResources ) > 0:
    n = random.randint( 8, 20 )
    if n > len( allResources ):
        n = len( allResources )
    singleSlice = allResources[0:n]
    del allResources[0:n]
    slices.append( singleSlice )
    
def LoadAndWait( resourceNames ):
    resources = []
    for resPath in resourceNames:
        print(resPath)
        res = blue.resMan.GetResource( str(resPath) )
        resources.append( res )

    # Once in a while, cancel a load request
    if random.random() < 0.1:
        del resources[random.randint( 0, len( resources ) - 1)]
        
    blue.resMan.Wait()
    
    for res in resources:
        if res.isLoading:
            logging.error("Wait failed! Resource is ", res.path)
            
def LoadList( listToLoad ):
    for each in listToLoad:
        LoadAndWait( each )
        
workPackets = []
while len( slices ) > 0:
    n = random.randint( 5, 15 )
    if n > len( slices ):
        n = len( slices )
    packet = slices[0:n]
    del slices[0:n]
    workPackets.append( packet )

blue.motherLode.ClearCached()

tasklets = []
for each in workPackets:
    #blue.synchro.Sleep( random.randint( 0, 3000 ) )
    #log.LogInfo( "Creating tasklet for {0} packets".format( len( each ) ) )
    #tasklets.append( uthread.new( LoadList, each ) )
    LoadList(each)
