# Code Readme

This code folder contains a folder named "Fitcat" and within that folder are two subfolders named "Rasberry-pi" and "udp_client".
The Rasberry-pi folder contains another subfolder named "node" which contains the node and canvas code in order to view the port with each of the cat tracking data the node_server.js is the primary file to be run. 

The udp_client folder contains the code to be flashed on the esp-32. This code tracks the acceleration with the accelerometer and then comunicates with the Raspberry Pi.

Both udp_client and node_server.js must be running at the same time with the correct IP addresses to have a working system.

## Reminders
  - This code is adapted from the previous quest (I2C_Accel) and integrated with code from the skills for this quest, chatgpt.com cited in line of the code, and written code.
