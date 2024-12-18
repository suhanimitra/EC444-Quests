### Remote Control for Buggy

For controlling the buggy with signals sent from a remote device. 

## Components
- car.c: sketch of code for interpreting WASD inputs. Needs to be
  combined to UDP_client.c or UDP_server.c plus your servo- and
  sensor- driving code.
- remote-control.js: node app code for hosting HTML page that takes WASD
  keyboard input and sends to ESP32 on buggy

## Use of a State Machine
The control model of the car can be in a state machine. So that
interpeting the sensor input and the WASD control input can be
combined to make the right transitions at a given instance and input
conditions.

Consider making a state diagram that considers the operating modes for
your vehicle (idle, driving forward, driving backward, turning, etc.)
with events including (WASD input, detection of walls, detection of
obstacles etc.). If you do this, the code just follows the state
machine.

