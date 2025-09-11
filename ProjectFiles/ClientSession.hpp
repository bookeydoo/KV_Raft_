
#pragma once
#include<boost/asio.hpp>
#include<chrono>
#include<memory>
#include<map>

using namespace boost::asio::ip;
using Socket = tcp::socket;


class Node;

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    explicit ClientSession(std::shared_ptr<Socket> Socket,
                           std::shared_ptr<boost::asio::streambuf> Buffer,
                           std::shared_ptr<Node> Parent);
    
    void start(bool isLeader);
    void do_read(bool isLeader ) ;
    std::shared_ptr<Socket> get_socket();
    
protected:
    std::shared_ptr<Socket> socket;
    std::shared_ptr<boost::asio::streambuf> buffer;
    std::weak_ptr<Node> Parent_node; // store as weak_ptr



};


class ApiSession: public ClientSession {
public:

    ApiSession(std::shared_ptr<Socket> Socket,
               std::shared_ptr<boost::asio::streambuf> Buffer,
               std::shared_ptr<Node> Parent)
    : ClientSession(std::move(Socket) ,std::move(Buffer),(std::move(Parent))){}


    void parseHttpRequest(const std::string & Raw_req, std::string& method,std::string& path,
        std::map<std::string,std::string>& headers, std::string & body);

    void Handle_Req();

    void process_Req(const std::string &method,
                                const std::string &path,
                                const std::map<std::string, std::string> &headers,
                                const std::string &body);
};
