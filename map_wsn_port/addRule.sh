#!/bin/bash

if [ $# -lt 1 ] ; then
   echo -e "Supply rule file argument! Missing!"
   echo -e "Prototype: sudo ./addRule.sh devATTRS.txt"
   exit 0
fi

if [[ $# -gt 1 ]]; then
  echo -e "Warning: Extra arguments present. Will be unused.\nTaking rule file: $1"
fi

echo -e "Note: Run this script only one time to avoid conflicts - All udev rule files will be added with the supplied rule(s)."
echo -e "Taking rule file: $1"

while true; do
    read -p "Do you wish to add this udev rule?(Y/N) : " yn
    case $yn in
        [Yy]* ) bash -c "cat $1 >> /etc/udev/rules.d/*.rules" ; udevadm trigger ; echo -e "Rule added - $1!\nRun ls -l /dev/tty* to review." ; break;;
        [Nn]* ) echo -e "Attempt aborted!" ; exit;;
        * );;
    esac
done
