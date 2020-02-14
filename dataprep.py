import glob, os
import csv
from PIL import Image
import numpy as np
import random
import sys
import pandas as pd
from scipy import ndimage
from scipy import stats
import serial

# import tf2api as tf2x
# from tf2api.tfrecords import ds

wisetty = '/dev/' + sys.argv[len(sys.argv) - 1]
baud=38400
#Two image names + node's tty will be received as arguments


for loop in range(1, len(sys.argv) - 1):
    photo = sys.argv[loop]
    CAM_NUMBER = ''.join(filter(lambda i: i.isdigit(), photo))
    CUTTOFF_CSV = 'cutoff' + str (CAM_NUMBER) + '.csv'
    with open(CUTTOFF_CSV, 'rt') as f:
        reader = csv.reader(f)
        offDict = {rows[0]:float(rows[1]) for rows in reader}
        f.close()
    CONF_CSV = 'conf' + str (CAM_NUMBER) + '.csv'
    with open(CONF_CSV, 'rt') as f:
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
        scale=list(np.arange(min_scale,max_scale,rng/rows))
        rows=list(range(rows))
        scale.reverse()
        offset=offDict['levelOff']
        level_cm = np.interp(level,rows,scale)-offset
        print("Water level detected in " + photo)
        #print(str (level) + ' pixels, ')

        print(str (level_cm - 0.5) + ' cm - ' + str (level_cm) + ' cm' )
        if os.path.exists(wisetty):
           ser = serial.Serial( wisetty , baud)
           print ("WSN Port : " + sys.argv[len(sys.argv) - 1 ] + " - OK\nPassed to node! Baud: " + str (baud) )
        else:
           print ("WSN port not detectable now. Check USB connections or reboot Linux")
           sys.exit()
        ser.write(level_cm)
        break


    else:
        print('No water level in image - ' + photo)
