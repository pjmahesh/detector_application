import time
from datetime import datetime
import os
import os.path
import sys
import glob
from os import path
import subprocess
from subprocess import Popen,PIPE
import shutil
import numpy as np
from PIL import Image
from shutil import copyfile
from PIL import ImageFile
ImageFile.LOAD_TRUNCATED_IMAGES = True
#import concurrent.futures
curr_time, secs, min, dt = 0, 0, 0, 0

count = 0
noc = 7 #Taking only 6 and 7
cap_int = 2

IMGFILE = sys.argv[1] #Eg. format received: example.jpg
IMGNAME = IMGFILE.split(".")[0]

def sort():
    dest = 'Images/' + curr_time.strftime("%Y%m%d_%H%M%S") + "_" + IMGNAME + '/'
    fname = curr_time.strftime("%Y%m%d_%H%M%S") + "_" + IMGFILE
    if os.path.exists("./"+IMGFILE):
       try:
           os.mkdir(dest)
       except:
            pass
       #for i in range(6,8):
       out = shutil.copy(IMGFILE , dest + fname)
       print('Saved duplicate: ' + IMGFILE + ' to ' + out)
    else:
        print('\033[91mError: \033[0m' + IMGFILE + ' does not exist!')


def compress(ts):
    os.system('gzip=-9 tar -cvzf' + ts + '.tgz Images')
    os.rename('Images', ts)
    os.mkdir('Images')
    return ts


def upload(zipfile):
    print ('At Upload')
    Process=Popen(['./test.sh', (zipfile + '.tgz')], shell=True,stdin=PIPE,stderr=PIPE)
    Process.communicate()
    #subprocess.call('send.sh ' + zipfile + '.tgz')
    #process = subprocess.Popen(['send.sh', (zipfile + '.tgz')], shell=True, stdout=subprocess.PIPE)
    #process.wait()


#print(">Image file:" , IMGFILE , "Image name:" , IMGNAME )
if path.isdir('Images') != True:
   os.mkdir('Images')

curr_time = datetime.now()
secs = int (curr_time.strftime('%S'))
min = int (curr_time.strftime('%M'))
dt = curr_time.strftime('%Y%m%d_%H%M%S')
sort()
jpgCounter = len(glob.glob1("./","*.jpg"))
if jpgCounter >= 50:
   zipfile = compress(dt)
   print('\033[1m' +  '\033[92m' + 'Zipfile ready ' + zipfile + '.tgz\n' + '\033[0m')
   #upload(zipfile)
   #print('\n- - - - - - - - - - - - - - - - - - - - - - -\n')
