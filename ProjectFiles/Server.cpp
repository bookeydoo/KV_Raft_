
#include<iostream>
#include"Node.hpp"
#include<boost/asio.hpp>
#include<boost/log/trivial.hpp>
#include<boost/system/error_code.hpp>
#include<boost/log/utility/setup/console.hpp>
#include<thread>



int main(int argc ,char *argv[]){


    
    int port=0; 
    bool Leaderflag=false;

    if(argc<2){ //no port was given
        port=base_port;    
        std::cerr<<"no arguments were given\n";
    }
    else{
        for(int i=1;i<argc;i++){

            std::string arg=argv[i];

            if(arg =="-L"){
                Leaderflag=true;
                std::cerr<<"Starting this node as a Leader\n";
            }else{                
                int val=atoi(argv[i]);
                port=val;
            }
        }    
        
    }
    

    boost::asio::io_context IO_ctx;
    std::vector<std::thread> Threads;


    auto Server=std::make_shared<Node>(IO_ctx,port);
    Server->initLogging();

    if(Leaderflag) {  //Feature For testing 
    Server->isLeader=true;
    Server->isCandidate=false;
    Server->isFollower=false;
    }

    BOOST_LOG_TRIVIAL(trace)<<"Trace msg\n";
    BOOST_LOG_TRIVIAL(debug)<<"Trace msg\n";
    BOOST_LOG_TRIVIAL(info)<<"Trace msg\n";
    BOOST_LOG_TRIVIAL(warning)<<"Trace msg\n";
    
    Server->Start_Server();


    
    for(size_t i=0;i<2;i++){
        Threads.emplace_back([&]{
            IO_ctx.run();
        });
    }


    for(auto& t:Threads) t.join();


    return 0;
}