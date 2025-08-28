## How to build

use g++ or clang with pthread flag and add all cpp files and if u are on windows u have to use socket flags (lws2_32 and lmswsock) 

example of my compiled file :

```
g++ -std=c++20 -O2 -Wall -pthread  Server.cpp  Node.cpp ClientSession.cpp  -o raft_server -lws2_32 -lmswsock
```