#include<iostream>
#include"Node.hpp"
#include"ClientSession.hpp"
#include<boost/asio.hpp>
#include<random>
#include<thread>
#include<fstream>
#include<chrono>
#include<map>
#include<format>

using namespace boost::asio::ip;
using Socket = tcp::socket;

BOOST_LOG_ATTRIBUTE_KEYWORD(channel_attr, "Channel", std::string)

namespace logging = boost::log;
namespace sinks   = boost::log::sinks;
namespace expr    = boost::log::expressions;

int base_port = 4900;





Node::Node(boost::asio::io_context& ctx,int Port)
                :IO_ctx(ctx),port(Port),election_timer(ctx),Heartbeat_timer(ctx)
                ,acceptor(ctx),apiAcceptor(std::make_shared<tcp::acceptor>(ctx,tcp::endpoint(tcp::v4(),port+100)))
                {}

//this is used to access the endpoints of the node sessions (non api)
const std::vector<std::shared_ptr<ClientSession>>& Node::getSessions() const{ 
        return Sessions;    
} 
    
    
void Node::Start_Server(){
    try{   
        BOOST_LOG_TRIVIAL(info)<<"Starting server on port: "<<port<<"\n";
        
        //if couldnt load Config properly we quit
        if(!ConfigLoad()){
            return;
        }

        std::vector<tcp::endpoint>Endpoints=CreateEndpoints();
        std::vector<std::shared_ptr<Socket>>Sockets;
       
        

        for( auto&ep :Endpoints){

            if(ep.port() == port) continue;
            auto socket=std::make_shared<Socket>(this->IO_ctx);
            Sockets.emplace_back(socket);
            auto peer=std::make_shared<Peer>(socket,ep);
            Peers.emplace_back(peer);
            Connect_Peer(peer);
        }
        

        //acceptor acts like a server socket from java
        //for each node(port) we are listening to we need to create a unique acceptor
        //on each unique acceptor object , we run async accept and io_ctx manages them

        auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        BOOST_LOG_TRIVIAL(info)<<"Started Node on Port "<<port<<" at "<<std::ctime(&now)<<"\n";
        

        
        

        auto NodeEndpoint=CreateEndpoint(port);
        acceptor.open(NodeEndpoint.protocol());
        acceptor.set_option(boost::asio::socket_base::reuse_address(true));
        acceptor.bind(NodeEndpoint);
        acceptor.listen();

        do_accept();

            BOOST_LOG_TRIVIAL(info)<<"Listening on "<<acceptor.local_endpoint()<<"\n";
            
            start_election_timer();

        ConnectToApi(apiAcceptor);
        

        }
        catch(const boost::system::system_error& e)
        {
            BOOST_LOG_TRIVIAL(fatal)<<"Failed to start server on port "<<port<<": "<< e.what() << '\n';
        }
    }



bool Node::ConfigLoad(){


    
    /*Config for the config should be like :
    Ip="",port=""
    Ip="",port=""
    */


    std::string Filename="Config.txt";
    ConfigStream=std::fstream(Filename,std::ios::in);

    if(!ConfigStream.is_open()){
        BOOST_LOG_TRIVIAL(fatal)<<"Critical Error:\n Couldnt access or open Config.txt, Make sure its in the same folder as .exe and thats its accesible\n";
        return false; //CRITICAL ERROR
    }

    std::string Line;
    while (std::getline(ConfigStream, Line)) {
        if (Line.empty()) continue; // skip blank lines

        std::string ip, Port;
        std::stringstream ss(Line);
        std::string token;

        while (std::getline(ss, token, ',')) {
            auto pos = token.find('=');
            if (pos == std::string::npos) continue;

            std::string value = token.substr(pos + 1);

            // strip surrounding whitespace
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // remove quotes
            if (!value.empty() && value.front() == '"') value.erase(0, 1);
            if (!value.empty() && value.back() == '"') value.pop_back();

            if (token.find("Ip") != std::string::npos) {
                ip = value;
            } else if (token.find("port") != std::string::npos) {
                Port = value;
            }
        }

        
            //skip our node if found
            //In a proper build that works on actual servers it should check the ip addr but since i am testing on local np
        if(std::stoi(Port) == port)  continue; 

        if (!ip.empty() && !Port.empty()) {
            auto candidate=std::make_pair(ip,Port);
            auto it=std::find(Config_EP.begin(),Config_EP.end(),candidate);
            if(it==Config_EP.end())
                Config_EP.emplace_back(ip, Port);
        }
    }

    BOOST_LOG_TRIVIAL(trace) << "Loaded " << Config_EP.size() << " config entries.\n";
    for (auto &c : Config_EP) {
        BOOST_LOG_TRIVIAL(trace) << "Ip: " << c.first << ", Port: " << c.second << "\n";
    }

    return true;
}

    
void Node::do_accept(){

        auto self =shared_from_this();

        acceptor.async_accept(
            [self](const boost::system::error_code ec, Socket peer_socket ){
                
                if(!ec) {

                    
                    auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());                
                    BOOST_LOG_TRIVIAL(info)<<"Accepted connection on port "
                                <<peer_socket.remote_endpoint().port()
                                <<" at "<<std::ctime(&now)<<"\n";


                    auto Shared_socket=std::make_shared<Socket>(std::move(peer_socket));
                    auto buf=std::make_shared<boost::asio::streambuf>();
                    //do operations here
                    auto Session=std::make_shared<ClientSession>(Shared_socket,buf,self);
                    self->Sessions.emplace_back(Session);

                    Session->start(self->isLeader);
                }
                else {
                    BOOST_LOG_TRIVIAL(error)<<"Accept error: "<<ec.what()<<"\n";
                }
                // To continue listening, restart the async_accept operation.
                self->do_accept();
            }
        );
    }

void Node::Connect_Peer(std::shared_ptr<Peer> peer){
        peer->socket->close();

        peer->socket->async_connect(peer->endpoint,
        [this,peer](boost::system::error_code ec){
            if(!ec){
                
                BOOST_LOG_TRIVIAL(trace)<< "connected to "<<peer->endpoint.port()<<"\n";
                //function to write specific text to socket
                auto buf =std::make_shared<boost::asio::streambuf>();
                auto session=std::make_shared<ClientSession>(peer->socket,buf,shared_from_this());
                Sessions.emplace_back(session);
                session->start(isLeader);

            }else{
                BOOST_LOG_TRIVIAL(warning)<<"failed to connect!!\n"<<ec.message()<<" retrying \n";

                //Retrying
                
                auto timer=std::make_shared<boost::asio::steady_timer>(
                    peer->socket->get_executor()
                );
                timer->expires_after(std::chrono::seconds(2));
                timer->async_wait([this,&peer,timer](auto){
                    //debug
                    BOOST_LOG_TRIVIAL(trace)<<"attempting connection"<<"\n";
                    Connect_Peer(peer);
                });
             }
        });
    }
    

void Node::TransmitMsg(std::string &Msg,std::shared_ptr<Socket> sock){
    if(!Msg.empty() && Msg.back() != '\n')  Msg.push_back('\n');

    if(sock && sock->is_open()){
            boost::asio::async_write(
                *sock,
                boost::asio::buffer(Msg),
                [Msg,sock](boost::system::error_code  ec, size_t bytes_transferred){
                    if(ec){
                        BOOST_LOG_TRIVIAL(error)<<"Failed to send"<<ec.message()<<"\n";
                    }
                    else{
                        BOOST_LOG_TRIVIAL(trace)<<"Sent:"<<bytes_transferred
                                 <<"bytes\n" ;
                    }
                }
            );
        }
    }
    

void Node::BroadcastMsg(const std::string &Msg,std::vector<std::shared_ptr<Socket>>& sockets){
        auto msg=std::make_shared<std::string>(Msg);
        if(!msg.get()->empty() && msg.get()->back() != '\n' )  msg.get()->push_back('\n');
        for(auto& socket: sockets){
            if(socket && socket->is_open()){
                boost::asio::async_write(
                *socket,
                boost::asio::buffer(*msg),
                [msg,socket](boost::system::error_code  ec, size_t bytes_transferred){
                    if(ec){
                        BOOST_LOG_TRIVIAL(error)<<"Failed to send"<<ec.message()<<"\n";
                    }
                    else{
                        BOOST_LOG_TRIVIAL(trace)<<"Sent:"<<bytes_transferred
                                 <<"bytes\n" ;
                    }
                });
            }
        }
    }


void Node::BroadcastMsg(const std::string &Msg,const std::vector<std::shared_ptr<ClientSession>>& Sessions) const{
        auto msg=std::make_shared<std::string>(Msg);

        for(auto& session: Sessions){
            auto socket_ptr=session->get_socket();
            if(socket_ptr && socket_ptr->is_open()){
                boost::asio::async_write(
                *socket_ptr,
                boost::asio::buffer(*msg),
                [msg,socket_ptr](boost::system::error_code  ec, size_t bytes_transferred){
                    if(ec){
                        BOOST_LOG_TRIVIAL(error)<<"Failed to send"<<ec.message()<<"\n";
                    }
                    else{
                        BOOST_LOG_TRIVIAL(trace)<<"Sent:"<<bytes_transferred
                                 <<"bytes\n" ;
                    }
                });
            }
        }
    }

    //--------------------------------------------------------------------------------------------------------------------------
    //-----------------------Logging functions
    //----------------------------------------------------------------------------------------------------------

    std::string Node::severityColor(boost::log::trivial::severity_level level){
        switch(level){
            case boost::log::trivial::trace   : return "\033[32m"; // green
            case boost::log::trivial::debug   : return "\033[35m"; // magenta 
            case boost::log::trivial::info    : return "\033[34m"; // blue
            case boost::log::trivial::warning : return "\033[33m"; // yellow
            case boost::log::trivial::error   : return "\033[31m"; // red
            case boost::log::trivial::fatal   : return "\033[41m"; // red background
            default: return "\033[37m";
        }
    }

    
    boost::shared_ptr<sinks::synchronous_sink<sinks::text_ostream_backend>> console_sink;
    boost::shared_ptr<sinks::synchronous_sink<sinks::text_ostream_backend>> file_sink;

void Node::initLogging() {

    logging::add_common_attributes();

    //console sink
    auto backend_console=boost::make_shared<sinks::text_ostream_backend>();
    backend_console->add_stream(boost::shared_ptr<std::ostream>(&std::clog, boost::null_deleter()));
    console_sink=boost::make_shared<sinks::synchronous_sink<sinks::text_ostream_backend>>(backend_console);

    console_sink->set_formatter([this](const boost::log::record_view& rec,
                           boost::log::formatting_ostream& strm) {
        auto lvl = rec[boost::log::trivial::severity];
        std::string color = severityColor(lvl.get());  // call static method
        strm << color
             << "[" << lvl << "] "
             << rec[boost::log::expressions::smessage]
             << "\033[0m"; // reset
    });

    
    // creating file with port as name for both file sinks
    std::stringstream filename;
    filename<<port<<".log";

    auto file=boost::make_shared<std::ofstream>(filename.str());

    if(!file->is_open())
        BOOST_LOG_TRIVIAL(fatal)<<"Failed to open log file:"<<filename.str()<<"\n";


    //file sink for all logs not saving state
    auto backend_updates=boost::make_shared<sinks::text_ostream_backend>();
    backend_updates->add_stream(file);
    backend_updates->auto_flush(true); //debugging


    file_sink=boost::make_shared<sinks::synchronous_sink<sinks::text_ostream_backend>>(backend_updates);

    //-----------------------------------------
    //file sink for saving state
    //-----------------------------------------
    
    
    auto updates_sink=boost::make_shared<sinks::synchronous_sink<sinks::text_ostream_backend>>(backend_updates);
    updates_sink->set_filter(channel_attr == "updates");
    updates_sink->set_formatter([](const boost::log::record_view& rec,boost::log::formatting_ostream& strm) {
    
        if(auto msg=rec[boost::log::expressions::smessage]){
            strm<<msg.get();
        }
    });
    logging::core::get()->add_sink(updates_sink);


    //initalize the default sink
    ChangeLoggingTo(LoggingType::Default); //default 

}


    void Node::ChangeLoggingTo(LoggingType logtype){

        auto core=logging::core::get();
        core->remove_sink(console_sink);
        core->remove_sink(file_sink);
        
        if(logtype== LoggingType::Default){ //default:console
            core->add_sink(console_sink);
        }
        if(logtype==LoggingType::File){ //file only
            core->add_sink(file_sink);
        }
        if(logtype==LoggingType::Both){ //Both
            core->add_sink(console_sink);
            core->add_sink(file_sink);
        }
    }



    //--------------------------------------------------------------------------------------------------------------------------
    //-----------------------Creating stuff for the Node 
    //----------------------------------------------------------------------------------------------------------

    //we can change no. of endpoints depending on number of nodes
    std::vector<tcp::endpoint> Node::CreateEndpoints(){
        std::vector<tcp::endpoint> Endpoints;
        for(auto& Entry: Config_EP){

            //skip our node if found
            if(std::stoi(Entry.second) == port) 
             continue; 

            tcp::endpoint ep=CreateEndpoint(Entry.first,Entry.second);
            BOOST_LOG_TRIVIAL(trace)<<"Created an Endpoint"<<ep.address().to_string()<<":"<<ep.port()<<"\n";
            Endpoints.emplace_back(ep);
        }
        return Endpoints;    
    }

    
    tcp::endpoint Node::CreateEndpoint(short port){
        return tcp::endpoint(tcp::v4(), port);
    }

    
    tcp::endpoint Node::CreateEndpoint(const std::string& ip, const std::string &portStr){

        boost::system::error_code ec;
        auto address=boost::asio::ip::make_address(ip,ec);

        if(ec){
            throw std::runtime_error("Invalid Ip address"+ip);
        }

        unsigned short port=static_cast<unsigned short>(stoi(portStr));

        return tcp::endpoint(address,port);
    }


    void Node::ConnectToApi(std::shared_ptr<tcp::acceptor> acceptor){

        auto socket = std::make_shared<Socket>(IO_ctx);
        auto self = shared_from_this();

        acceptor->async_accept(*socket,
        [self,acceptor,socket,this](const boost::system::error_code& ec){

            if(!ec){
                auto buffer=std::make_shared<boost::asio::streambuf>();
                auto api_session=std::make_shared<ApiSession>(socket,buffer,self);

                api_session->start(self->isLeader);
            }
            ConnectToApi(acceptor);
        });
    }
    


    int Node::generate_random_timeout_ms(){
        return dist(rng);
    }

    void Node::start_election_timer(){
        auto timeout=std::chrono::milliseconds(generate_random_timeout_ms());
        BOOST_LOG_TRIVIAL(debug)<<"Election timeout set to: "<<timeout<<"\n";

        election_timer.expires_after(timeout);

        //capture self to keep node alive during async wait 
        //in theory this should add election timer wait to the io_ctx so it will managed by the asio runtime 
        auto self=shared_from_this();
        election_timer.async_wait([self](boost::system::error_code ec){
            if(!ec){
                BOOST_LOG_TRIVIAL(debug)<<"Election timer expired so starting Election\n";
                self->start_Election();
            }else if(ec==boost::asio::error::operation_aborted){
                BOOST_LOG_TRIVIAL(debug)<<"Election timer reset\n";
            }
        });

    }


    
    void Node::Reset_election_timer(){
        //reset the election timeout
        election_timer.cancel();
        start_election_timer();
    }


    //-----------------------------------------------------------------------------------------------------------------------
    //---------------------Election stuff
    //---------------------------------------------------------------------------------------------------

    std::string Node::RequestVote(){
        int lastIndex = this->Log.empty() ? 0 : this->Log.size() - 1;
        std::string Request=std::format(
            "REQUEST-VOTE {} {} {} ",this->Curr_Term,"Candidateid",lastIndex);
        return Request;
    }

    void Node::handleVoteReq(std::string voteReq){
        //structure for vote request: 
        //Voted for:

         if(voteReq.find("Voted for")==0){
            std::string Vote;
                
            size_t vote_start=voteReq.find("Voted for:\"")+12;
            size_t vote_end=voteReq.find("\"",vote_start);

            Vote=voteReq.substr(vote_start,vote_end-vote_start);
            int Voteport=std::stoi(Vote);

            for(auto &candidate: candidates){
                if(Voteport == candidate.port)
                    candidate.noOfVotes++;
            }
        }
    }
    
    void Node::start_Election(){
        
        if(this->isLeader ){
            BOOST_LOG_TRIVIAL(debug)<<"already a leader, No election needed\n";
            return ;
        }

        this->Curr_Term++;
        this->isCandidate=true;
        //voting for itself
        candidates.clear();
        candidates.emplace_back(Candidate{port,1});

        //then send requests : my request vote function outputs a constant string sent to all the sockets 
        std::string Requestbuffer=RequestVote();

        for(const auto& Session : Sessions){
            this->TransmitMsg(Requestbuffer,Session->get_socket());
        }

        auto timer=std::make_shared<boost::asio::steady_timer>(IO_ctx);
        timer->expires_after(std::chrono::milliseconds(200));
        timer->async_wait([this,timer](const boost::system::error_code & ElectionTimeout){
                    this->check_Election_result(ElectionTimeout);
        });  
        
        BOOST_LOG_TRIVIAL(info)<<"Election ended\n";
    }

    void Node::check_Election_result(const boost::system::error_code& ElectionTimeout){
        
       
        if(!isCandidate)
            return;

        if(ElectionTimeout == boost::asio::error::operation_aborted){
            //If the timer was cancelled , you got a heartbeat from the leader(leader has been decided)
            //so time to the process and start listening 
            return; 
        }
        if (this->isCandidate && this->candidates.size() > (Peers.size() + 1) / 2) {
            BOOST_LOG_TRIVIAL(info) << "Election won! Becoming the leader.\n";
            this->isLeader = true;
            this->isCandidate = false;
            election_timer.cancel();
            Leaderfunc();
        } 
        else{
            BOOST_LOG_TRIVIAL(info) << "Election timed out. Starting a new election.\n";
            // Start a new election with a new term
            this->start_Election();
        }
    }

    
          
    
    void Node::Leaderfunc(){

        std::string Heartbeat= "AppendEntries";

        for(const auto& Session: Sessions){
            TransmitMsg(Heartbeat,Session->get_socket());
        }
        auto timeout=std::chrono::milliseconds(150);
    
        Heartbeat_timer.expires_after(timeout);

        //capture self to keep node alive during async wait 
        //in theory this should add election timer wait to the io_ctx so it will managed by the asio runtime 
        auto self=shared_from_this();
        Heartbeat_timer.async_wait([self](boost::system::error_code ec){
            if(!ec){
                BOOST_LOG_TRIVIAL(info)<<"Heartbeat timer expired so starting Election\n";
                self->Leaderfunc();
            }else if(ec==boost::asio::error::operation_aborted){
                BOOST_LOG_TRIVIAL(warning)<<"Heartbeat timer reset? i think this shouldn't happen\n";
            }
        });


    }




    //------------------------------------------------------------------------------------------------------------
    //--------------------------Operations on logs
    //--------------------------------------------------------------------


    //AppendEntry: key:"",value:"", term:n
    void Node::handleAppendEntry(std::string AppendEntry){
        std::string key,value;
        int term;

        size_t key_start=AppendEntry.find("key:\"")+5;
        size_t key_end=AppendEntry.find("\"",key_start);

        key=AppendEntry.substr(key_start,key_end-key_start);
        size_t value_start=AppendEntry.find("value:\"")+7;
        size_t value_end=AppendEntry.find("\"",value_start);

        value=AppendEntry.substr(value_start,value_end-value_start);

        size_t term_Pos=AppendEntry.find("term:")+5;
        std::string term_str=AppendEntry.substr(term_Pos);
        term=std::stoi(term_str);
                    
        Log.emplace(key,Logstruct{value,term});
        AddtoLog(1,key,value,term);
    }

     void Node::handlePassAppend(std::string Entry){
        std::string key,value;
        size_t key_start_pos = Entry.find("key:")+4;
        size_t key_end_pos = Entry.find(",value:", key_start_pos);

        key = Entry.substr(key_start_pos, key_end_pos - key_start_pos);

        size_t value_start_pos=key_end_pos+7; //,value:

        value=Entry.substr(value_start_pos);

        Log.emplace(key,Logstruct(value,Curr_Term));
        AddtoLog(1,key,value,Curr_Term);
    }

    void Node::handleDeleteEntry(std::string DeleteEntry){
        std::string key,value;
        int term;

        size_t key_start=DeleteEntry.find("key:\"")+5;
        size_t key_end=DeleteEntry.find("\"",key_start);

        key=DeleteEntry.substr(key_start,key_end-key_start);
        size_t value_start=DeleteEntry.find("value:\"")+7;
        size_t value_end=DeleteEntry.find("\"",value_start);

        value=DeleteEntry.substr(value_start,value_end-value_start);

        size_t term_Pos=DeleteEntry.find("term:")+5;
        std::string term_str=DeleteEntry.substr(term_Pos);
        term=std::stoi(term_str);
                    
        Log.emplace(key,Logstruct{value,term});
        AddtoLog(0,key,value,term);
    }
    std::optional<Logstruct> Node::getLogEntry(const std::string &key){
        auto it = Log.find(key);
        if (it != Log.end()) return it->second;
        return std::nullopt;
    }

    //structure for writes and deletes : op=append,key:"" ,value:"" ,term:n

    std::map<std::string,Logstruct> Node::RestoreState(){
        if(LogStream && LogStream.is_open()){
            BOOST_LOG_TRIVIAL(info)<<"Started restoring state\n";

            std::string Stringbuf;
            std::map<std::string,Logstruct> Log;

            while(std::getline(LogStream,Stringbuf)){
                if(Stringbuf.find("op=append")==0){
                    std::string key,value;
                    int term;
                    
                    size_t key_start=Stringbuf.find("key:\"")+5;
                    size_t key_end=Stringbuf.find("\"",key_start);

                    key=Stringbuf.substr(key_start,key_end-key_start);

                    
                    size_t value_start=Stringbuf.find("value:\"")+7;
                    size_t value_end=Stringbuf.find("\"",value_start);

                    value=Stringbuf.substr(value_start,value_end-value_start);


                    size_t term_Pos=Stringbuf.find("term:")+5;
                    std::string term_str=Stringbuf.substr(term_Pos);
                    term=std::stoi(term_str);
                    
                    Log.emplace(key,Logstruct{value,term});
                    
                }
                if (Stringbuf.find("op=delete")==0){
                   //do instant delete at first and do tombstones in a later version
                    std::string key;

                    size_t key_start=Stringbuf.find("key:\"")+5;
                    size_t key_end=Stringbuf.find("\"",key_start);

                    key=Stringbuf.substr(key_start,key_end-key_start);

                    Log.erase(key);
                    BOOST_LOG_TRIVIAL(trace)<<"successfully deleted item"<<key<<"\n";
                }
                

            }

            BOOST_LOG_TRIVIAL(info)<<"Succesfully was able to Restore state\n";
            return Log;
        }

        BOOST_LOG_TRIVIAL(fatal)<<"Critical Error: failed to open Log\n";
        return {};
    }

    bool Node::AddtoLog(bool Write,std::string& key,std::string& value, int& term){ //1 for W and 0 for Delete
        if(LogStream && LogStream.is_open()){
            BOOST_LOG_TRIVIAL(info)<<"Started Persisting state\n";
            std::string Stringbuf;
            if(Write){
                 BOOST_LOG(updatelogger) <<"op=append,key:\""<<key<<"\",value:\""<<value<<"\",term:"<<term<<"\n";
            }
            if(!Write){
                 BOOST_LOG(updatelogger) <<"op=delete,key:\""<<key<<"\",value:\""<<value<<"\",term:"<<term<<"\n";
            }

            BOOST_LOG_TRIVIAL(trace)<<"Success\n";
            return 1;
        }
        BOOST_LOG_TRIVIAL(error)<<"Failed to write\n";
        return 0;
    }


