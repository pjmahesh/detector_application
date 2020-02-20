To train individual cameras - Preparing conf0x.csv :

1. Use a proper image with balls clearly visible .
2. If image conditions of all cameras are same: Make copies of the same image in the name of cameras.
   (Eg., Camera01.jpg, Camera02.jpg etc. depending the number of devices) 
3. Otherwise: Use seperate images with individual image conditions under device names.
4. Fill parameters: level pixel range, scale range, and offsets in cutoff0x.csv for each camera.
   (Eg., cutoff04.csv for image Camera04.jpg)
5. 'level pixel' range will be the same if one image is taken to train all cameras.
   (Eg., levelb 1300, levelt 1250)
6. Now to prepare conf csv:
   $ python3 makeConf.py Camera01.jpg cutoff01.csv
   Output: Output success - conf01.csv

Note: conf01.csv name is dependend on image name. 
      For Camera01.jpg conf csv will be output as conf01.csv and so on.

Note2: a.out can be used to take individual images for this excercise. 
