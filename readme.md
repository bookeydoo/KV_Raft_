# KV_Raft_ (WIP)



## Description
This project is a simple implementation of the Raft consensus algorithm.
It provides a distributed key-value store with :
- HTTP API for GET and POST to add to the network 
- Persists state in case of Node failures(fault tolerance)
- Leader Election between nodes


## How to build

use g++ or clang with pthread flag and add all cpp files and if you are on windows you have to use socket flags (lws2_32 and lmswsock) 

example of my building process on windows :

```
g++ -std=c++20 -O2 -Wall -pthread  Server.cpp  Node.cpp ClientSession.cpp  -o raft_server -lws2_32 -lmswsock
```


## How to use?

Each program instance is a single Node instance.
You run each node with a port for now and place a Config file in the same directory as the exe or it won't run.

If you run the program without an ip , it will initalize to base_port 4900 but won't connect without a Config file: Config.txt (case sensitive)

Structure of Config.txt:

```
Ip="127.0.0.1",port="4900"
Ip="127.0.0.1",port="9001"
Ip="127.0.0.1",port="9002"
Ip="127.0.0.1",port="9003"
````

Also added Leader flag for testing: -L (WIP)

To use the http api , you curl a request like the following :
```curl -X POST localhost:65531/kv -d "key=foo&value=bar"```


## Whats next?
- Aiming to add a multistage build process with gcc next to test it better with separate containers ان شاء الله
- Gonna try to simplify testing so that you only run the dockercompose and allow you to choose any default ports like a config file.
- TODO: allow the user through the api to simulate node failures to properly test the nodes
- snapshotting and log compaction (using a form of zipping and truncation)
- maybe attempt an LSM-tree style storage
- HOT config reload to add/remove peers dynamically
- when a new node joins, keys/snapshots get redistributed

## TODO 
Modify Config.txt parsing to parse a port for the apiSession acceptor 
Look into using boost::log instead of std::cerr for better logging 