#include"Types.h"
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
    LoggingType LogType =LoggingType::Default;

    if(argc<2){ //no port was given
        port=base_port;    
        BOOST_LOG_TRIVIAL(warning)<<"no arguments were given\n";
    }
    else{
        for(int i=1;i<argc;i++){

            std::string arg=argv[i];

            if(arg =="-L"){
                Leaderflag=true;
                BOOST_LOG_TRIVIAL(debug)<<"Starting this node as a Leader\n";
            }else{                
                int val=atoi(argv[i]);
                port=val;
            }
            //stands for file for putting the logs in a file instead of terminal
            if(arg=="-f" || arg=="-F") {
                LogType=LoggingType::File; //only write to files
                BOOST_LOG_TRIVIAL(info)<<"Logs will go to Node ip.txt \n";
            }else if(arg=="-b"|| arg=="-B"){
                LogType=LoggingType::Both; //write to both Logs and console
                BOOST_LOG_TRIVIAL(info)<<"Logs will go to files and console\n";
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
    
    Server->Start_Server();

    switch(LogType) {
            case LoggingType::Default:
                Server->ChangeLoggingTo(LogType);
                break;
            case LoggingType::File:
                // handle file-only logging
                Server->ChangeLoggingTo(LogType);
                break;
            case LoggingType::Both:
                // handle both file and console logging
                Server->ChangeLoggingTo(LogType);
                break;
    }
    
    for(size_t i=0;i<2;i++){
        Threads.emplace_back([&]{
            IO_ctx.run();
        });
    }


    for(auto& t:Threads) t.join();


    return 0;
}