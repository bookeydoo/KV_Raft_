
#include<iostream>
#include"Node.hpp"
#include<boost/asio.hpp>
#include<thread>



int main(int argc ,char *argv[]){

    int port=0;

    if(argc<2){ //no port was given
        port=base_port;    
        std::cerr<<"no argument was given\n";
    }
    else{
       int val=atoi(argv[1]);
       port=val;
    }

    boost::asio::io_context IO_ctx;
    std::vector<std::thread> Threads;

    auto Server=std::make_shared<Node>(IO_ctx,port);
    Server->Start_Server();

    for(size_t i=0;i<2;i++){
        Threads.emplace_back([&]{
            IO_ctx.run();
        });
    }


    for(auto& t:Threads) t.join();


    return 0;
}