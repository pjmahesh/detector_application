#!/bin/bash

#cd /home/pi/detector_application

cd "$(dirname "$0")"
echo -e "\npwd:""$(pwd)"

SECONDS=0

if [ ! -d "/dev/serial/by-id" ] ; then
  echo -e "No devices found! Check USB connections or reboot Linux."
  exit 0
fi

#Order of capture will be order of Args. DEVS will hold device names in args.
declare -a DEVS=()

if [ $# -lt 2 ] ; then
   echo "Insufficient arguments!"
   echo -e "Usage: $ ./capp.sh <device01> <device02> <device03> <device04> <time>"
   exit 0
fi

#Checking last argument is a single digit integer number or not
re='^[0-9]+$'
if [[ ! "${@: -1}" =~ $re ]] || [ ${@: -1} -gt 9 ]  ; then
   echo "Time argument invalid!"
   echo "Usage: $ ./capp.sh <device01> <device02> <device03> <device04> <time>"
   exit 0
fi

#Checking number of arguments.
if [ $# -gt 5 ] ; then
   echo "Max devices four! (or) Extra arguments present!"
   echo "Usage: $ ./capp.sh <device01> <device02> <device03> <device04> <time>"
   exit 0
fi

#Checking duplicate device names in arguments
for (( x=1 ; x<$# ; x++ )) ; do
  for (( y=1 ; y<$# ; y++ )) ; do
    if [ "${!x}" == "${!y}" ] && [ "$x" != "$y" ] ; then
       echo -e "Duplicate device names not allowed! - ""${!x}"
       echo "Usage: $ ./capp.sh <device01> <device02> <device03> <device04> <time>"
       exit 0
    fi
  done
done


#Taking time stamps
min="$(date +%M)"
min_end="${min:1:1}"

#Matching time arguments with timestamp inorder to decide capture. Method implemented for end use will be on cron.
if [ "$min_end" -eq ${@: -1} ] ; then
    #At this point args are as expected.
    echo -e "\nArgs verified ..."

    #Checking devices.
    for (( x=1 ; x<$# ; x++ )) ; do
      tty=$(ls -l /dev/serial/by-id | grep -n "${!x}" | tail -c 8)
      if [ "$tty" = "" ] ; then
         echo "\nCould not find device - ""${!x}"
         exit 0
      else
         DEVS[$x]="$tty"
         #echo -e "Device found : ""${!x}"" - ""${DEVS[$x]}"
      fi
    done

    #Getting WSN serial port.
    wisetty=$(ls -l /dev/serial/by-id | grep -n "WSN01" | tail -c 8)

    if [ "$wisetty" = "" ] ; then
        echo -e "\nCould not find device : WSN-Node"
        wisetty="N/A"
        echo -e "\nDevices found:"
    else
        echo -e "\nAll devices found ..."
    fi

    #Printng detected devices.
    for (( x=1 ; x<$# ; x++ )) ; do
      echo -e "${!x} : ${DEVS[$x]}"
    done
    echo -e "WSN-Node : ${wisetty}"

    #All devices detected and entering capture and detection loop.
    echo -e "\nCapture starting ..."
    for (( x=1 ; x<$# ; x++ )) ; do
        echo -e "\nCapturing from ${!x} ..."
        if [ -e "/dev/${DEVS[$x]}" ] ; then
          echo -e "$(tput setaf 2)Port ""${DEVS[$x]}"" - OK$(tput sgr 0)"
        else
          echo -e "${!x}"" is undetectable now. Check USB connections or reboot Linux."
          exit 0
        fi
        cmd="./a.out /dev/${DEVS[$x]} ${!x}.jpg"
        echo -e "$cmd"
        capp_time1=$SECONDS
        #Opening camera port and captute starts. Analysing output for open "failed"
        resp=$($cmd | tee /dev/stderr | grep -c "failed")
        #echo -e "Grep pattern occurance: ${resp}"
        capp_time2=$SECONDS
        capp_time=`expr $capp_time2 - $capp_time1`
        if [ $resp -gt 0 ]; then
          echo -e "app.c failed to open port - ""${DEVS[$x]}""\nScript exiting ..."
          echo -e "$(tput setaf 1)Your port </dev/${DEVS[$x]}> carrying ${!x} was present, but app.c could not open it. Replug and retry.$(tput sgr 0)"
          exit 0
        else
          echo -e "$(tput setaf 2)Capture and transfer took: $(($capp_time / 60)) min $(($capp_time % 60)) sec$(tput sgr 0)"
          cmd_dprep="python3 dataprep.py "
          cmd_dprep+="${!x}.jpg ""${wisetty}"
          echo -e "Analysing images for water level ...\nExecuting ..."
          echo "$cmd_dprep"
          #Starting to analyse image captured. Searching for string "detected" to confirm if a level data was detected.
          resp=$($cmd_dprep | tee /dev/stderr | grep -c "detected")
          #echo -e "Grep pattern occurance: ${resp}"
          if [[ $resp -gt 0  ]]; then
            break
          fi
        fi
    done

else
  echo -e  "\nTime ends other! Time argument doesn't match also review \"crontab -e\"!"
  echo -e "Usage: $ ./capp.sh <device01> <device02> <device03> <device04> <time>"
fi

duration=$SECONDS
echo -e "\nTime elapsed: $(($duration / 60)) min $(($duration % 60)) sec"
