from scipy import ndimage
from scipy import stats
import csv
import sys
from PIL import Image
import numpy as np

if (len(sys.argv) < 3):
    print ("\033[91mInsufficient arguments.\033[0m\nPrototype: $ python3 makeConf.py <device0x> <cutoff0x>")
    print ("Note: Ending digits \"0x\" need not be the same.\nCreated conf0x.csv name will depend only on the ending digits of device name.")
    sys.exit()

PHOTO = sys.argv[1]
CUTTOFF_CSV = sys.argv[2]
CAM_NUMBER = ''.join(filter(lambda i: i.isdigit(), PHOTO))

curr=ndimage.rotate(np.array(Image.open(PHOTO)), 270)

with open(CUTTOFF_CSV, 'rt') as f:
    reader = csv.reader(f)
    offDict = {rows[0]:rows[1] for rows in reader}
    f.close()

print (offDict)
curr_cutoff=curr[int(offDict['levelt']):int(offDict['levelb']),:int(offDict['tapeOff']),:]

r_mean=np.average(curr_cutoff[:,:,0])
g_mean=np.average(curr_cutoff[:,:,1])
b_mean=np.average(curr_cutoff[:,:,2])

r_dev=np.std(curr_cutoff[:,:,0])
g_dev=np.std(curr_cutoff[:,:,1])
b_dev=np.std(curr_cutoff[:,:,2])

CONF_CSV = 'conf' + str(CAM_NUMBER) + '.csv'

with open(CONF_CSV, 'w') as file:
    writer = csv.writer(file)
    #writer.writerow(["color", "mean", "sd"])
    writer.writerow(["red", r_mean, r_dev])
    writer.writerow(["green", g_mean, g_dev])
    writer.writerow(["blue", b_mean, b_dev])
    file.close()
    print("Output success: " + CONF_CSV)
