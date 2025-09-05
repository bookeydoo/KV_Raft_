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



int base_port = 4900;



Node::Node(boost::asio::io_context& ctx,int Port)
                :IO_ctx(ctx),port(Port),election_timer(ctx),Heartbeat_timer(ctx),acceptor(ctx)
                {
                        std::stringstream filename;
                        filename<<port<<".txt";
                        LogStream=std::fstream(filename.str(),std::ios::out | std::ios::in| std::ios::app);
                        
                        if(!LogStream.is_open()){
                            std::cerr<<"Couldn't Open file or Create \n";
                        }

                    }
//this is used to access the endpoints of the node sessions (non api)
    const std::vector<std::shared_ptr<ClientSession>>& Node::getSessions() const{ 
        return Sessions;    
    } 
    
    
void Node::Start_Server(){
        try
        {
            std::cerr<<"Starting server on port: "<<port<<"\n";
            std::vector<tcp::endpoint>Endpoints=CreateEndpoints(port);
            
            std::vector<std::shared_ptr<Socket>> Sockets= CreateSockets(this->IO_ctx);
            

            for(size_t i=0;i<2;i++){
                auto buf=std::make_shared<boost::asio::streambuf>();

                Peers.emplace_back(Peer{Sockets[i],Endpoints[i]});
            }
            

            //acceptor acts like a server socket from java
            //for each node(port) we are listening to we need to create a unique acceptor
            //on each unique acceptor object , we run async accept and io_ctx manages them

            auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::cerr<<"Started Node on Port "<<port<<" at "<<std::ctime(&now)<<"\n";
            

            
            

            auto NodeEndpoint=CreateEndpoint(port);
            acceptor.open(NodeEndpoint.protocol());
            acceptor.set_option(boost::asio::socket_base::reuse_address(true));
            acceptor.bind(NodeEndpoint);
            acceptor.listen();

            do_accept();

            std::cerr<<"Listening on "<<acceptor.local_endpoint()<<"\n";
            

            for(auto& peer:Peers){
                auto p=std::make_shared<Peer>(peer);
                Connect_Peer(p);
            }

            

        }
        catch(const boost::system::system_error& e)
        {
            std::cerr <<"Failed to start server on port "<<port<<": "<< e.what() << '\n';
        }
        

    }

    
    void Node::do_accept(){

        auto self =shared_from_this();

        acceptor.async_accept(
            [self](const boost::system::error_code ec, Socket peer_socket ){
                
                if(!ec) {

                    
                    auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());                
                    std::cerr<<"Accepted connection on port "
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
                    std::cerr<<"Accept error: "<<ec.what()<<"\n";
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
                
                std::cerr<< "connected to "<<peer->endpoint.port()<<"\n";
                //function to write specific text to socket
                auto buf =std::make_shared<boost::asio::streambuf>();
                auto session=std::make_shared<ClientSession>(peer->socket,buf,shared_from_this());
                Sessions.emplace_back(session);
                session->start(isLeader);

            }else{
                std::cerr<<"failed to connect!!"<<ec.message()<<" retrying \n";

                //Retrying
                
                auto timer=std::make_shared<boost::asio::steady_timer>(
                    peer->socket->get_executor()
                );
                timer->expires_after(std::chrono::seconds(2));
                timer->async_wait([this,&peer,timer](auto){
                    //debug
                    std::cerr<<"attempting connection"<<"\n";
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
                        std::cerr<<"Failed to send"<<ec.message()<<"\n";
                    }
                    else{
                        std::cerr<<"Sent:"<<bytes_transferred
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
                        std::cerr<<"Failed to send"<<ec.message()<<"\n";
                    }
                    else{
                        std::cerr<<"Sent:"<<bytes_transferred
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
                        std::cerr<<"Failed to send"<<ec.message()<<"\n";
                    }
                    else{
                        std::cerr<<"Sent:"<<bytes_transferred
                                 <<"bytes\n" ;
                    }
                });
            }
        }
    }



    //--------------------------------------------------------------------------------------------------------------------------
    //-----------------------Creating stuff for the Node 
    //----------------------------------------------------------------------------------------------------------

    std::vector<std::shared_ptr<Socket>>Node::CreateSockets( boost::asio::io_context &IO_ctx){
        std::vector<std::shared_ptr<Socket>> SocketVec;
        for(size_t i=0;i<3;i++){
            auto socket=std::make_shared<Socket>(IO_ctx);
            SocketVec.emplace_back(std::move(socket));
        }

        return SocketVec;
    }

    tcp::endpoint Node::CreateEndpoint(short port){
        return tcp::endpoint(tcp::v4(), base_port);
    }

    //we can change no. of endpoints depending on number of nodes
    std::vector<tcp::endpoint> Node::CreateEndpoints(short port){
        std::vector<tcp::endpoint> Endpoints;
        for(int i=0;i<2;i++){

            if(port == base_port+i) continue;

            Endpoints.emplace_back( tcp::endpoint(boost::asio::ip::address_v4::loopback(), base_port+i));
        }
        return Endpoints;    
    }


    int Node::generate_random_timeout_ms(){
        return dist(rng);
    }

    void Node::start_election_timer(){
        auto timeout=std::chrono::milliseconds(generate_random_timeout_ms());
        std::cerr<<"Election timeout set to: "<<timeout<<"\n";

        election_timer.expires_after(timeout);

        //capture self to keep node alive during async wait 
        //in theory this should add election timer wait to the io_ctx so it will managed by the asio runtime 
        auto self=shared_from_this();
        election_timer.async_wait([self](boost::system::error_code ec){
            if(!ec){
                std::cerr<<"Election timer expired so starting Election\n";
                self->start_Election();
            }else if(ec==boost::asio::error::operation_aborted){
                std::cerr<<"Election timer reset\n";
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
            std::cerr<<"already a leader, No election needed\n";
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
        
        std::cerr<<"Election ended\n";
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
            std::cerr << "Election won! Becoming the leader." << std::endl;
            this->isLeader = true;
            this->isCandidate = false;
            election_timer.cancel();
            Leaderfunc();
        } 
        else {
        std::cerr << "Election timed out. Starting a new election." << std::endl;
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
                std::cerr<<"Heartbeat timer expired so starting Election\n";
                self->Leaderfunc();
            }else if(ec==boost::asio::error::operation_aborted){
                std::cerr<<"Heartbeat timer reset? i think this shouldn't happen\n";
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
            std::cerr<<"Started restoring state\n";

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
                    std::cerr<<"successfully deleted item"<<key<<"\n";
                }
                

            }

            std::cerr<<"Succesfully was able to Restore state\n";
            return Log;
        }

        std::cerr<<"Critical Error: failed to open Log\n";
        return {};
    }

    bool Node::AddtoLog(bool Write,std::string& key,std::string& value, int& term){ //1 for W and 0 for Delete
        if(LogStream && LogStream.is_open()){
            std::cerr<<"Started Persisting state\n";
            std::string Stringbuf;
            if(Write){
                LogStream<<"op=append,key:\""<<key<<"\",value:\""<<value<<"\",term:"<<term<<"\n";
            }
            if(!Write){
                LogStream<<"op=delete,key:\""<<key<<"\",value:\""<<value<<"\",term:"<<term<<"\n";
            }

            std::cerr<<"Success\n";
            return 1;
        }
        std::cerr<<"Failed to write\n";
        return 0;
    }


