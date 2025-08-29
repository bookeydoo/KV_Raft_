## How to build

use g++ or clang with pthread flag and add all cpp files and if u are on windows u have to use socket flags (lws2_32 and lmswsock) 

example of my building process on windows :

```
g++ -std=c++20 -O2 -Wall -pthread  Server.cpp  Node.cpp ClientSession.cpp  -o raft_server -lws2_32 -lmswsock
```


## How to use?

You run the program with the base port but for now , you should have it use one of default port(65531 ,65532,65533)

If you run the program without an ip , it will initalize to base_port which is 65531 and will attempt connecting to default ip nodes(65532,65533)

To use the http api , u curl a request like the following :
```curl -X POST localhost:65531/kv -d "key=foo&value=bar"```


## Whats next?
Aiming to add a multistage build process with gcc next to test it better with separate containers ان شاء الله
Gonna try to simplify testing so that you only run the dockercompose and allow u to choose any default ports like a config file.