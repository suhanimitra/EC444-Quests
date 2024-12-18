# Sample Code for Interfacing to Optitrack and Positioning Server
2024-11-09

This repo includes
- C code for a UDP client for connecting to positioning server (node app)
- parseNodeString.c for parsing received positiong data (characters--> values)

The format of the request/receive is:

Request (ID) <br>
Receive (x,z,theta,status) <br>

Where,

- x,z -- coordinates within the Optitrack field of view (0,0 is centered in the space).
- theta -- angle of orientation (0-360, relative to the z direction)
- status -- tracking or not tracking (indicates presence of the rigid body in the field of view)


