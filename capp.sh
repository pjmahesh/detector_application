#!/bin/bash

cd /home/pi/detector_application

if [ ! -d "/dev/serial/by-id" ] ; then
  echo -e "No devices found! Check USB connections or reboot Linux."
  exit 0
fi


#camera numbers are relative to the order of arguments given. order of capture will also follow the order.
wisetty=$(ls -l /dev/serial/by-id | grep -n "WISEGATE" | tail -c 8)
declare -a DEVS=()

if [ $# -lt 2 ] ; then
   echo "Insufficient arguments!"
   echo -e "Prototype: $ ./capp.sh <device01> <device02> <device03> <device04> <time>"
   exit 0
fi

#Checking last argument is a single digit integer number or not
#echo "${@: -1}"
re='^[0-9]+$'
if [[ ! "${@: -1}" =~ $re ]] || [ ${@: -1} -gt 9 ]  ; then
   echo "Time argument invalid!"
   echo "Prototype: $ ./capp.sh <device01> <device02> <device03> <device04> <time>"
   exit 0
fi

if [ $# -gt 5 ] ; then
   echo "Max devices four! (or) Extra arguments present!"
   echo "Prototype: $ ./capp.sh <device01> <device02> <device03> <device04> <time>"
   exit 0
fi

#Checking duplicate device names in arguments
#echo "${@:1:$#}"
for (( x=1 ; x<$# ; x++ )) ; do
  for (( y=1 ; y<$# ; y++ )) ; do
    #echo -e "$x""-""$y"
    #echo -e "${!x}""-""${!y}"
    if [ "${!x}" == "${!y}" ] && [ "$x" != "$y" ] ; then
       echo -e "Duplicate device names not allowed! - ""${!x}"
       echo "Prototype: $ ./capp.sh <device01> <device02> <device03> <device04> <time>"
       exit 0
    fi
  done
done

echo -e "Args verified ..."

for (( x=1 ; x<$# ; x++ )) ; do
  tty=$(ls -l /dev/serial/by-id | grep -n "${!x}" | tail -c 8)
  if [ "$tty" = "" ] ; then
  {
    echo "Could not find device - ""${!x}"
    exit 0
  }
  else
    DEVS[$x]="$tty"
    #echo -e "Device found : ""${!x}"" - ""${DEVS[$x]}"
  fi
done

echo -e "All devices found ..."
for (( x=1 ; x<$# ; x++ )) ; do
  echo -e "${!x}"" : ""${DEVS[$x]}"
done

#echo -e ${DEVS[3]}

min="$(date +%M)"
min_end="${min:1:1}"

if [ "$min_end" -eq ${@: -1} ] ; then
    echo -e "\nCapture starting ..."
    cmd_dprep="python3 dataprep.py"
    for (( x=1 ; x<$# ; x++ )) ; do
        echo -e "\nExecuting ..."
        cmd="./a.out /dev/${DEVS[$x]} ${!x}.jpg"
        echo -e "$cmd"
        if [ -e "/dev/${DEVS[$x]}" ] ; then
          echo "Port ""${DEVS[$x]}"" - OK"
          eval "$cmd"
        else
          echo -e "${!x}"" is undetectable now. Check USB connections or reboot Linux."
          exit 0
        fi
        cmd="python3 sort_script.py ${!x}.jpg"
        echo -e "Executing image backup script ..."
        echo "$cmd"
        eval "$cmd"
        echo -e "Execution success!!"
        cmd_dprep+=" ""${!x}.jpg" #${cam02}.jpg ${wisetty}"
    done
    echo -e "\nAnalysing images for water level ...\nExecuting ..."
    cmd_dprep+=" ""${wisetty}"
    echo "$cmd_dprep"
    eval "$cmd_dprep"
else
  echo "Time ends other! Time argument doesn't match also review \"crontab -e\"!"
  echo "Prototype: $ ./capp.sh <device01> <device02> <device03> <device04> <time>"
fi
