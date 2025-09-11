#include"Node.hpp"
#include"ClientSession.hpp"

using namespace boost::asio::ip;
using Socket = tcp::socket;


class Node;


   ClientSession::ClientSession(std::shared_ptr<Socket> Socket,
                             std::shared_ptr<boost::asio::streambuf> Buffer,
                             std::shared_ptr<Node> Parent)
        : socket(std::move(Socket)), buffer(std::move(Buffer)), Parent_node(Parent) {}
    

    void ClientSession::start(bool isLeader) {
        do_read(isLeader); // Start the first asynchronous read operation 
    }

    std::shared_ptr<Socket> ClientSession::get_socket() { return socket; }  

    void ClientSession::do_read(bool isLeader ) {
        // Use a shared pointer to the session to keep it alive
        auto self(shared_from_this());
        bool Leaderstate=isLeader;

        boost::asio::async_read_until(*socket,*buffer, '\n',
            [this, self,Leaderstate](boost::system::error_code ec, size_t bytes_transferred) {
                if (!ec) {
                    std::istream is(buffer.get());
                    std::string line;
                    std::getline(is, line);

                    auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                    auto parent=Parent_node.lock();

                    if(line.find("AppendEntries") != std::string::npos){
                       
                        if(!parent || parent->isLeader){
                            //leader shouldn't accept append from non_leaders
                            return ;
                        }
                        //Leader heartbeat 
                        parent->Reset_election_timer();

                        size_t term_Pos=line.find("term:")+5;
                        std::string term_str=line.substr(term_Pos);
                        int term=std::stoi(term_str);

                        if(term <parent->Curr_Term){
                            std::string Msg=std::format("AppendEntries_Fail leader: {}:{},term:{}",
                            parent->Curr_leader.address().to_string()
                            ,parent->Curr_leader.port()
                            ,parent->Curr_Term);
                            parent->TransmitMsg(Msg,self->socket);
                        }

                        if(term>parent->Curr_Term){
                            parent->Curr_Term=term;
                            parent->isFollower=true;
                            parent->isCandidate=false;
                            parent->isLeader=false;
                            parent->Curr_leader=this->get_socket()->local_endpoint();
                        }
                        
                        
                        std::string AppendEntry=line;
                        if(!AppendEntry.empty()){
                            //append
                            parent->handleAppendEntry(AppendEntry);
                        }
                    }

                    // std::format("PassAppend:key:{},value:{}",key,value);
                    if(line.find("PassAppend")!= std::string::npos){
                        parent->handlePassAppend(line);
                    }

                    if(line.find("DeleteEntries")!= std::string::npos){
                        parent->Reset_election_timer();

                        std::string DeleteEntry=line;
                           if(!DeleteEntry.empty()){
                            //append
                            parent->handleDeleteEntry(DeleteEntry);
                        }

                    }
                    if(line.starts_with("REQUEST-VOTE")){

                         size_t term_Pos=line.find("term:")+5;
                        std::string term_str=line.substr(term_Pos);
                        int term=std::stoi(term_str);

                        if(term>parent->Curr_Term){
                            parent->Curr_Term=term;
                            parent->isFollower=true;
                            parent->isCandidate=false;
                            parent->isLeader=false;
                        }
                        
                        parent->handleVoteReq(line);
                    }


                
                    std::cerr << "Received: " << line << " from "
                              << socket->remote_endpoint()<< "at"
                              << std::ctime(&now) << "\n";

                    // Continue reading from the same client
                    do_read(Leaderstate);
                } else {
                    std::cerr << "Read error: " << ec.message() << "\n";
                    // Error occurred, the session can be closed or handle the error
                }
            });
    }




 
void ApiSession::parseHttpRequest(const std::string & Raw_req, std::string& method,std::string& path,
        std::map<std::string,std::string>& headers, std::string & body){
        
        std::istringstream stream(Raw_req);
        std::string line;

        if(std::getline(stream,line)){
            std::istringstream request_line(line);
            request_line>>method>>path;
        }

        //parsing headers  
        //ex:  host: localhost:8080\r\n

        while(std::getline(stream,line)&& line !="\r"){
            auto colon= line.find(":");
            if(colon !=std::string::npos){ //npos means there was an error finding keyword or the string ended
                std::string key=line.substr(0,colon);
                std::string value=line.substr(colon+1);
                //trim space+ \r
                key.erase(key.find_last_not_of(" \r\n")+1);
                value.erase(0,value.find_first_not_of(" "));
                value.erase(value.find_last_not_of(" \r\n")+1);

                // normalize headers to lowercase
                 std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c){ return std::tolower(c); });

                headers[key]=value;
            }
        }

        // Body (everything left in the stream)
        
        std::ostringstream body_stream;
        body_stream << stream.rdbuf();
        body = body_stream.str();
        // trim \r\n at the end
        if (!body.empty() && body.back() == '\n') body.pop_back();
        if (!body.empty() && body.back() == '\r') body.pop_back();

}


void ApiSession::Handle_Req(){

    auto self = shared_from_this();

    // Step 1: read headers
    boost::asio::async_read_until(*socket, *buffer, "\r\n\r\n",
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                BOOST_LOG_TRIVIAL(error) << "Header read error: " << ec.message();
                return;
            }

            
            std::string raw_headers(bytes_transferred,'\0');
            buffer->sgetn(&raw_headers[0], bytes_transferred);

            std::string method, path, body;
            std::map<std::string, std::string> headers;
            parseHttpRequest(raw_headers, method, path, headers, body);


            

            std::size_t content_length = 0;
            if (headers.contains("Content-Length")) {
                try {
                    content_length = std::stoul(headers["Content-Length"]);
                } catch (...) {
                    BOOST_LOG_TRIVIAL(error) << "Invalid Content-Length";
                    return;
                }
            }

            std::size_t already_have = body.size();
            if (content_length > already_have) {
                // Read remaining body
                boost::asio::async_read(*socket, *buffer,
                    boost::asio::transfer_exactly(content_length - already_have),
                    [this, self, method, path, headers, body](boost::system::error_code ec, std::size_t) mutable {
                        if (!ec) {
                            std::istream body_stream(buffer.get());
                            std::string extra_body(
                                (std::istreambuf_iterator<char>(body_stream)),
                                std::istreambuf_iterator<char>());
                            body += extra_body;

                            process_Req(method, path, headers, body);
                        } else {
                            BOOST_LOG_TRIVIAL(error) << "Body read error: " << ec.message();
                        }
                    });
            } else {
                // no body (or fully received already)
                process_Req(method, path, headers, body);
            }
        });
}


void ApiSession::process_Req(const std::string &method,
                                const std::string &path,
                                const std::map<std::string, std::string> &headers,
                                const std::string &body) {
    auto parent = Parent_node.lock();
    if (!parent) return;

    if (method == "POST" && path == "/kv") {
        // parse body for key=value
        std::string key, value;
        size_t key_pos   = body.find("key=");
        size_t value_pos = body.find("value=");
        if (key_pos != std::string::npos && value_pos != std::string::npos) {
            key = body.substr(key_pos + 4, body.find('&') - (key_pos + 4));
            value = body.substr(value_pos + 6);
            if (parent->isLeader) {
                parent->Log.emplace(key, Logstruct(value, parent->Curr_Term));
                std::string msg = std::format("AppendEntries key:{} value:{} term:{}",
                                              key, value, parent->Curr_Term);
                parent->BroadcastMsg(msg, parent->getSessions());
            } else {
                auto sessions = parent->getSessions();
                std::string msg = std::format("PassAppend key:{} value:{}", key, value);
                for (auto &session : sessions) {
                    if (parent->Curr_leader == session->get_socket()->remote_endpoint()) {
                        parent->TransmitMsg(msg, session->get_socket());
                    }
                }
            }
        }
    }
    else if(method=="POST" && path.rfind("/admin/restart")){
        BOOST_LOG_TRIVIAL(info)<<"Restart Requested via API\n";
        parent->restartNode();
    }
    else if (method == "GET" && path.rfind("/kv", 0) == 0) {
        auto query_pos = path.find('?');
        if (query_pos != std::string::npos) {
            std::string query = path.substr(query_pos + 1);
            size_t eq = query.find('=');
            if (eq != std::string::npos && query.substr(0, eq) == "key") {
                std::string key = query.substr(eq + 1);
                auto val = parent->getLogEntry(key);
                std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n" + val->value;
                parent->TransmitMsg(response, get_socket());
            }
        }
    }
}





