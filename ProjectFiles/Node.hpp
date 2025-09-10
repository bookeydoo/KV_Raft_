#pragma once
#include<iostream>
#include<boost/asio.hpp>
#include"ClientSession.hpp"
#include<random>
#include<fstream>
#include<chrono>
#include<map>
#include<boost/log/core.hpp>
#include<boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include<boost/system/error_code.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include<boost/log/utility/setup/console.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include<format>
#include"Types.h"

using namespace boost::asio::ip;
using Socket = tcp::socket;


class ClientSession;


extern int base_port ;

struct Candidate{
     int port;
     int noOfVotes;

    Candidate(int Port, int NoOfVotes)
        : port(Port) , noOfVotes(NoOfVotes){}
};

struct Logstruct{
    std::string value;
    int term;
};
struct Peer {
    std::shared_ptr<Socket> socket;
    tcp::endpoint endpoint;

    Peer(std::shared_ptr<Socket> Socket, tcp::endpoint Endpoint)
        : socket(std::move(Socket)), endpoint(std::move(Endpoint)) {}

};


class Node : public std::enable_shared_from_this<Node>{
private:
    std::fstream LogStream;
    std::fstream ConfigStream;
    tcp::acceptor acceptor;  //non copyable , wont build if emplaced directly in a vec 
    std::shared_ptr<tcp::acceptor> apiAcceptor;
    std::vector<std::shared_ptr<Peer>> Peers; 
    std::vector<Candidate> candidates;
    boost::asio::io_context &IO_ctx;    
    boost::asio::steady_timer election_timer;
    boost::asio::steady_timer Heartbeat_timer;
    std::vector<std::shared_ptr<ClientSession>> Sessions;  //internally have sockets (non copyable) 
    
    //for random timeouts
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist{150,300};


public:
    int port;
    int Curr_Term=0;
    bool isLeader=false;
    bool isFollower=true;
    bool isCandidate=false;
    tcp::endpoint Curr_leader;
    std::map<std::string,Logstruct> Log;
    std::vector<std::pair<std::string,std::string>> Config_EP;
    

    Node(boost::asio::io_context& ctx,int Port);
                    
      //this is used to access the endpoints of the node sessions (non api)
    const std::vector<std::shared_ptr<ClientSession>>& getSessions() const ;
    

    
    void Start_Server();
    bool ConfigLoad(); //used for Hot reloading and dynamically loading new sockets
    
    void do_accept();

    void Connect_Peer(std::shared_ptr<Peer> peer);
    

    void TransmitMsg(std::string &Msg,std::shared_ptr<Socket>sock);

    void BroadcastMsg(const std::string &Msg,std::vector<std::shared_ptr<Socket>>& sockets);
    void BroadcastMsg(const std::string &Msg,const std::vector<std::shared_ptr<ClientSession>>& Sessions)const ;


    //--------------------------------------------------------------------------------------------------------------------------
    //-----------------------Logging functions
    //----------------------------------------------------------------------------------------------------------


    std::string severityColor(boost::log::trivial::severity_level level);

    void initLogging();

    void ChangeLoggingTo(LoggingType LogType);


    //--------------------------------------------------------------------------------------------------------------------------
    //-----------------------Creating stuff for the Node 
    //----------------------------------------------------------------------------------------------------------

    std::vector<std::shared_ptr<Socket>> CreateSockets( boost::asio::io_context &IO_ctx);

    //we can change no. of endpoints depending on number of nodes
    std::vector<tcp::endpoint>CreateEndpoints();

    tcp::endpoint CreateEndpoint(short port);

    tcp::endpoint CreateEndpoint(const std::string& ip, const std::string &port);

    void ConnectToApi(std::shared_ptr<tcp::acceptor> acceptor);

    int generate_random_timeout_ms();

    void start_election_timer();


    
    void Reset_election_timer();


    //-----------------------------------------------------------------------------------------------------------------------
    //---------------------Election functions
    //---------------------------------------------------------------------------------------------------

    std::string RequestVote();

    void handleVoteReq(std::string voteReq);
    
    void start_Election();

    void check_Election_result(const boost::system::error_code& ElectionTimeout);
    
          
    
    void Leaderfunc();



    //------------------------------------------------------------------------------------------------------------
    //--------------------------Operations on logs
    //--------------------------------------------------------------------


    //AppendEntry: key:"",value:"", term:n
    void handleAppendEntry(std::string AppendEntry);
       
    void handlePassAppend(std::string Entry);

    void handleDeleteEntry(std::string DeleteEntry);

    std::optional<Logstruct> getLogEntry(const std::string &key);

    //structure for writes and deletes : op=append,key:"" ,value:"" ,term:n

    std::map<std::string,Logstruct> RestoreState();
    

    bool AddtoLog(bool Write,std::string& key,std::string& value, int& term); //1 for W and 0 for Delete
        


};