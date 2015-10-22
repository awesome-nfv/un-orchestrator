The name-resolver is a module that provides information for all the possible
implementations for a netowrk function. 

===============================================================================

Libraries required to compile the name-resolver:

* Boost  
      apt-get install libboost-all-dev 

* JSON spirit  
      git clone https://github.com/sirikata/json-spirit
      alternatively, a copy of the library is provided in [un-orchestrator]/contrib/json-spirit.zip

* Libmicrohttpd  
      apt-get install libmicrohttpd-dev

* libxml2  
     apt-get install libxml2-dev
     
===============================================================================
     
Compile the name-resolver

  cd name-resolver  
  ccmake .  
     Here you can select some configuration parameters such as the logging level.  
  make

===============================================================================

Command lines parameters for the un-orchestrator can be retrieved thorugh the
command:

sudo ./node-orchestrator --h
  
  [name-resolver] Usage:   
  sudo ./name-resolver --f file_name  
                                                                                         
Parameters:  
  --f file_name  
        Name of the xml file describing the possible implementations for the network     
        functions.                                                                       
                                                                                         
Options:  
  --h   
        Print this help.                                                                 
                                                                                         
Example:  
  sudo ./name-resolver --f ./config/example.xml
  
Please check config/example.xml to understand the configuration file required by
the name-resolver. This file contains information on all the possible implementaions
for each available network function.
  
===============================================================================

* If you are going to use the name-resolver together with the un-orchestrator, 
you can skip this part *

It is possible to interact with the name-resolver through it own REST server; 
particularly, it listens for HTTP requests on the TCP port 2828. If the required
network function exists, the name-resolver provides the information on such a 
network function in JSON format. 

The following example requires the information for the network function "bridge":

GET /nfs/bridge HTTP/1.1  
A possible answer sent by the name-resolver is the following

{  
    "implementations" : [  
        {  
            "type" : "docker",  
            "uri" : "localhost:5000/example"  
        },  
        {  
            "cores" : "1",  
            "location" : "remote",  
            "type" : "dpdk",  
            "uri" : "https://nf_repository.con/example"  
        },  
        {  
            "cores" : "1",  
            "location" : "local",  
            "type" : "dpdk",  
            "uri" : "/home/nf_repository/dpdk/example"  
        },  
        {  
            "type" : "kvm",  
            "uri" : "/home/nf_repository/kvm/example.qcow2"  
        }  
    ],  
    "name" : "bridge"  
}   
