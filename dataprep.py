import glob, os
import csv
import time
from datetime import datetime
from PIL import Image
import numpy as np
import random
import sys
import os.path
import os.path
from os import path
import pandas as pd
from scipy import ndimage
from scipy import stats
from shutil import copyfile
import shutil
Image.LOAD_TRUNCATED_IMAGES = True

wisetty = '/dev/' + sys.argv[len(sys.argv) - 1]

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def log_to_html_csv( level_cm , ts ):
    level_log='/var/www/html/level_log.csv'
    if not os.path.exists(level_log):
        os.system('touch ' + level_log)

    with open(level_log, 'a') as file:
        writer = csv.writer(file)
        writer.writerow([ "TS", ts , "Level" , level_cm*10 ])
        file.close()
        print( bcolors.OKBLUE + "Saved values to:\n" + bcolors.ENDC + level_log )

    out = shutil.copy(level_log , './' )
    print(bcolors.OKBLUE + "Saved duplicate file to pwd:\n" + bcolors.ENDC + out )

def sort_and_save(IMGFILE):
    IMGNAME = IMGFILE.split(".")[0]

    curr_time, secs, min, dt = 0, 0, 0, 0
    curr_time = datetime.now()
    secs = int (curr_time.strftime('%S'))
    min = int (curr_time.strftime('%M'))
    dt = curr_time.strftime('%Y%m%d_%H%M%S')

    dest_bkp = './Images/' + curr_time.strftime("%Y%m%d_%H%M%S") + "_" + IMGNAME + '/'
    dest_html = '/var/www/html/'
    fname = curr_time.strftime("%Y%m%d_%H%M%S") + "_" + IMGFILE

    if path.isdir('./Images') != True:
       os.mkdir('./Images')

    if os.path.exists("./"+IMGFILE):
       try:
           os.mkdir(dest)
       except:
            pass
       temp=Image.open(IMGFILE)
       rotated=temp.rotate(270, expand=True)
       os.makedirs(dest_bkp)
       rotated.save(dest_bkp + fname)
       #out = shutil.copy(IMGFILE , dest_bkp + fname)
       print( bcolors.OKBLUE + 'Rotated and saved detected image:\n' + bcolors.ENDC + IMGFILE + ' to ' + dest_bkp + fname)
       rotated.save(dest_html + 'latest.jpg')
       #out = shutil.copy(IMGFILE , dest_html + 'latest.jpg' )
       print(bcolors.OKBLUE + 'Rotated and saved detected image:\n' + bcolors.ENDC + IMGFILE + ' to ' + dest_html + 'latest.jpg' )

    else:
        print( bcolors.WARNING + 'Error: ' + bcolors.ENDC + IMGFILE + ' does not exist!')

    return dt



for loop in range(1, len(sys.argv)-1):
    photo = sys.argv[loop]
    CAM_NUMBER = ''.join(filter(lambda i: i.isdigit(), photo))
    CUTTOFF_CSV = 'cutoff' + str (CAM_NUMBER) + '.csv'
    with open( './confs/'+ CUTTOFF_CSV , 'rt' ) as f:
        reader = csv.reader(f)
        offDict = {rows[0]:float(rows[1]) for rows in reader}
        f.close()
    CONF_CSV = 'conf' + str (CAM_NUMBER) + '.csv'
    with open( './confs/'+ CONF_CSV , 'rt' ) as f:
        reader = csv.reader(f)
        confDict = {rows[0]:(float(rows[1]),float(rows[2])) for rows in reader}
        f.close()

    r_mean=confDict['red'][0]
    g_mean=confDict['green'][0]
    b_mean=confDict['blue'][0]

    r_dev=confDict['red'][1]
    g_dev=confDict['green'][1]
    b_dev=confDict['blue'][1]


    curr=ndimage.rotate(np.array(Image.open(photo)), 270)

    curr_cutoff=curr[:,:750,:]
    curr_new=curr[:,:750,:]

    r=curr_new[:,:,0]
    g=curr_new[:,:,1]
    b=curr_new[:,:,2]

    r[(r<(r_mean+3*r_dev)) & (r>(r_mean-offDict['sd']*r_dev))]=1
    r[r !=1]=0

    g[(g<(g_mean+3*g_dev)) & (g>(g_mean-offDict['sd']*g_dev))]=1
    g[g !=1]=0

    b[(b<(b_mean+3*b_dev)) & (b>(b_mean-offDict['sd']*b_dev))]=1
    b[b !=1]=0

    curr_new=np.add(np.add(r,g),b)
    curr_new[curr_new==1]=0
    curr_new[curr_new==2]=0
    curr_new[curr_new==3]=1
    sums=np.sum(curr_new,axis=1)
    sums[sums<offDict['sumOff']]=0
    sums[sums>=offDict['sumOff']]=1

    if 1 in sums:
        level=list(sums).index(1)
        curr_new[curr_new==1]=255
        img = Image.fromarray(curr_new)
        img.save('latest_level.jpg')

        min_scale=offDict['min_scale']
        max_scale=offDict['max_scale']
        rng=max_scale-min_scale

        rows=curr.shape[0]
        scale=list(np.arange(min_scale,max_scale,rng/rows))[:rows]
        rows=list(range(rows))
        scale.reverse()
        offset=offDict['levelOff']
        level_cm = np.interp(level,rows,scale) - offset

        print( bcolors.OKGREEN + 'Water level detected in '+ photo + ':' )
        print( str (level_cm - 0.5) + ' cm - ' + str (level_cm) + ' cm' + bcolors.ENDC )

        #print('\nCalling sort_and_save ...')
        ts = sort_and_save(photo)

        #print('\nCalling log_to_html_csv ... ')
        log_to_html_csv(level_cm , ts)

        if os.path.exists(wisetty):
           ifserver_cmd = "./camera_if_sim.out " + str (level_cm * 10 )
           os.system(ifserver_cmd)
        else:
           print ("WSN port not detectable now. Check USB connections or reboot Linux")
           sys.exit()
        break

    else:
        print( bcolors.WARNING + 'No water level in image - ' + photo + bcolors.ENDC )
        level_cm = 1000
        print('Sending a junk level to WSN!')
        ifserver_cmd = "./camera_if_sim.out " + str (level_cm * 10 )
        os.system(ifserver_cmd)
