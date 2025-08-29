#pragma once
#include<iostream>
#include<boost/asio.hpp>
#include<random>
#include<thread>
#include<fstream>
#include<chrono>
#include<map>
#include<format>

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

    Peer(Peer&& other) noexcept
        : socket(std::move(other.socket)), endpoint(std::move(other.endpoint)) {}

    Peer& operator=(Peer&& other) noexcept {
        if (this != &other) {
            socket = std::move(other.socket);
            endpoint = std::move(other.endpoint);
        }
        return *this;
    }
};


class Node : public std::enable_shared_from_this<Node>{
private:
    std::fstream LogStream;
    std::vector<Peer> Peers; 
    std::vector<Candidate> candidates;
    boost::asio::io_context &IO_ctx;    
    boost::asio::steady_timer election_timer;
    boost::asio::steady_timer Heartbeat_timer;
    std::vector<std::shared_ptr<ClientSession>> Sessions;  //internally have sockets (non copyable) 
    std::vector<std::shared_ptr<tcp::acceptor>> acceptors;  //non copyable , wont build if emplaced directly in a vec 
    std::map<std::string,Logstruct> Log;
    
    //for random timeouts
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist{150,300};


public:
    int port;
    int Curr_Term=0;
    bool isLeader=false;
    bool isFollower=true;
    bool isCandidate=false;


    Node(boost::asio::io_context& ctx,int Port);
                    
    
    
    void Start_Server();
    
    void do_accept(tcp::acceptor& acceptor);

    void Connect_Peer(Peer& peer);
    

    void TransmitMsg(std::string &Msg,std::shared_ptr<Socket>sock);

    void BroadcastMsg(std::string &Msg,std::vector<std::shared_ptr<Socket>>& sockets);
    void BroadcastMsg(std::string &Msg,std::vector<std::shared_ptr<ClientSession>>& Sessions);


    //--------------------------------------------------------------------------------------------------------------------------
    //-----------------------Creating stuff for the Node 
    //----------------------------------------------------------------------------------------------------------

    std::vector<std::shared_ptr<Socket>> CreateSockets( boost::asio::io_context &IO_ctx);

    //we can change no. of endpoints depending on number of nodes
    std::vector<tcp::endpoint> CreateEndpoints(short port);


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
       
    

    void handleDeleteEntry(std::string DeleteEntry);

    std::optional<Logstruct> getLogEntry(const std::string &key);

    //structure for writes and deletes : op=append,key:"" ,value:"" ,term:n

    std::map<std::string,Logstruct> RestoreState();
    

    bool AddtoLog(bool Write,std::string& key,std::string& value, int& term); //1 for W and 0 for Delete
        


};