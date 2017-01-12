READE 0.1

For packet-broker application, normally we hash the flow to the same physical port on switch for further analytic processing. According to openflow specification,for "select" group, hashing algorithm is decided by vendor themselves. 
1) In some cases, vendor can't ganrantee the flow-based hash mode. Hereby we introduced "SHASH" mechanism.
2) We could get customized hashing algorithm on-the-fly to archive more balanced traffic. 

shash by xumamen@gmail.com

