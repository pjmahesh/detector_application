/***Root access required**/

Linux udev rule configuration to detect WSN and asign a labeled port:
> udev rules in debian located at : /etc/udev/rules.d/
> Hardware device description of node's USB to TTL interface: WSN01
> Requested port to be allocated: Next available ttyUSBx with a label ttyWSN
  So, WSN will be accesible both on ttyUSBx and ttyWSN (PFA image)

Setup:

1. Rule is writen inside devATTRS.txt
2. Copy the rule manually and reload rules to rule file in udev directory (or)
   Run the script addRule.sh with root permission.
3. Script prootype: sudo ./addRule.sh devATTRS.txt

