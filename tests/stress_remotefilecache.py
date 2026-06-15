# Copyright © 2014 CCP ehf.

import os
import time
import random

import blue
import walk


KB = 1024
def FormatMemory( val ):
    if val < KB:
        label = "B"
    elif val > KB and val < KB**2:
        label = "KB"
        val = val/KB
    elif val > KB**2 and val < KB**3:
        label = "MB"
        val = val/KB**2
    elif val > KB**3:
        label = "GB"
        val = val/KB**3
    return str( round(val,2) )+label

def LoadAllFilesInFolder(rootFolder):
    print("Loading files from", rootFolder)
    for root, dirs, files in walk.walk(rootFolder):
        for name in files:
            filename = root + "/" + name
            rf = blue.paths.open(filename, "rb")
            contents = rf.read()
            rf.close()
            time.sleep(0.5*random.random())

def Setup():
    if not os.path.exists("cache/ResFiles"):
        os.makedirs("cache/ResFiles")
    blue.paths.SetSearchPath("cache", "cache")
    blue.remoteFileCache.server = "127.0.0.1:5000"
    blue.remoteFileCache.DownloadFileIndex("127.0.0.1:5000/index")


if __name__ == "__main__":
    Setup()
    LoadAllFilesInFolder("res:/dx9/model/ship")
    print("Remote file cache statistics:")
    print(blue.remoteFileCache.filesDownloaded, "files downloaded,", FormatMemory(blue.remoteFileCache.bytesDownloaded))
    print(blue.remoteFileCache.filesCached, "files cached,", FormatMemory(blue.remoteFileCache.bytesCached))
    print(blue.remoteFileCache.filesUsedFromCache, "files used from cache")
