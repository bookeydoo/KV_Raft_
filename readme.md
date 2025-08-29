# KV_Raft_


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
You run each node with a port for now , you should have it use one of the default ports(65531 ,65532,65533).

If you run the program without an ip , it will initalize to base_port which is 65531 and will attempt connecting to default ip nodes(65532,65533)

To use the http api , you curl a request like the following :
```curl -X POST localhost:65531/kv -d "key=foo&value=bar"```


## Whats next?
- Aiming to add a multistage build process with gcc next to test it better with separate containers ان شاء الله
- Gonna try to simplify testing so that you only run the dockercompose and allow you to choose any default ports like a config file.
- TODO: allow the user through the api to simulate node failures to properly test the nodes
