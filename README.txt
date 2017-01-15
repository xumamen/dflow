Simple way to implement systemtric hash by using openflow. 

1) According to openflow specification,  hashing mechanism of "select group"  is decided by vendor itself. Sometimes the hashing mechanism provided by vendor can't meet customer's requirement.

2) Shash could use all the field in openflow matching term to distribute the traffic to egress interfaces.
