#!/bin/bash
(crontab -l 2>/dev/null; echo "*/10 * * * * /home/pi/detector_application/capp.sh Camera01 Camera02 Camera03 Camera04 0 >> /home/pi/detector_application/cronLog.txt 2>&1") | crontab -
