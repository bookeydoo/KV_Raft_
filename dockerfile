#-------------------Stage 1 : Build-------------------#

#Ubuntu to ensure it works with gcc and boost
FROM ubuntu:24.04 AS Builder

#install dependices
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

#create new folder
WORKDIR /app
COPY ./ProjectFiles /app/ 

#running my program (with debug symbols)
RUN g++ -std=c++23 -Wall -pthread \
    Server.cpp Node.cpp ClientSession.cpp \
    -g -o raft_server \
    -lboost_log -lboost_log_setup \
    -lboost_thread -lboost_filesystem \
    -lboost_date_time -lboost_regex\
    -lboost_chrono -lboost_atomic \
    -lboost_system -lpthread


CMD [ "./raft_server" ]


#-------------------Stage 2 : Run-------------------#


FROM ubuntu:24.04


#Installing only the runtime boost (not build tools!)
RUN apt-get update && apt-get install -y \
    libboost-log1.74.0 \
    libboost-filesystem1.74.0 \
    libboost-thread1.74.0 \
    libboost-date-time1.74.0 \
    libboost-regex1.74.0 \
    gdb \
    && rm -rf /var/lib/apt/lists/*

#working dir
WORKDIR /app 

#copy the compiled binary
COPY --from=Builder /app/raft_server /app/

