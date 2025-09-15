
#-------------------This is the Debug container so its heavy-------------------#
#-------------------Stage 1 : Build-------------------#

#Ubuntu to ensure it works with gcc and boost
FROM ubuntu:24.04 AS Builder

ARG BUILD_TYPE=release

#install dependices
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

#create new folder
WORKDIR /app
COPY ./ProjectFiles /app/ProjectFiles
COPY ./CMakeLists.txt /app/

#building my program 
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
RUN cmake --build build -- -j${nproc}


CMD [ "./raft_server" ]


#-------------------Stage 2 : Run-------------------#


FROM ubuntu:24.04 

ARG BUILD_TYPE=release

#Installing only the runtime boost (not build tools!)
RUN apt-get update && apt-get install -y --no-install-recommends\
    libboost-log1.74.0 \
    libboost-filesystem1.74.0 \
    libboost-thread1.74.0 \
    libboost-date-time1.74.0 \
    libboost-regex1.74.0 \
    && rm -rf /var/lib/apt/lists/*

RUN if [${BUILD_TYPE}="Debug"]; then apt-get update && apt-get install -y gdb && rm -rf /var/lib/apt/lists/*; fi
#working dir
WORKDIR /app 

#copy the compiled binary
COPY --from=Builder /app/build/raft_server /app/

