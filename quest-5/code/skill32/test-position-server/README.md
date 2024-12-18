# Test Position Server
2024-11-09

This program is used to test the positioning server (a node.js app).

To run, load onto a laptop running on the local subnet (same as
positioning server). The test program requests positioning data for
"Robot 10" and returns (x,z,theta,status) where,

- x,z -- coordinates within the Optitrack field of view (0,0 is centered in the space).
- theta -- angle of orientation (0-360, relative to the z direction)
- status -- tracking or not tracking (indicates presence of the rigid body in the field of view)
