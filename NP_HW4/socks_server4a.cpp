#include <array>
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <regex>
#include <map>
#include <sys/types.h>
#include <sys/wait.h>
#include <fstream>
#include <string>
using namespace std;
using namespace boost::asio;

io_service global_io_service;

class ServerSession : public enable_shared_from_this<ServerSession> {
    private:
        enum { max_length = 4096 };
        unsigned int conn_dst_port;
        unsigned int bind_port;
        unsigned char CD;
        array<char, max_length> src_data;
        array<char, max_length> dst_data;
        array<char, max_length> src_response;
        array<char, max_length> dst_response;
        
        ip::tcp::socket src_socket;
        ip::tcp::socket dst_socket;
        ip::tcp::acceptor ftp_acceptor;
        ip::tcp::resolver dst_resolver;
        string dst_ip;
        string dst_ip_arr[4];
        string domain;
        ifstream socks_cfg;
        
    public:
        ServerSession(ip::tcp::socket socket) : src_socket(move(socket)), dst_socket(global_io_service),
                        ftp_acceptor(global_io_service,ip::tcp::endpoint(ip::tcp::v4(), 0)), dst_resolver(global_io_service) {}
        void start() {
            // global_io_service.notify_fork(io_service::fork_prepare);
            // pid_t pid = fork();
            // if (pid == 0) {
            //     global_io_service.notify_fork(io_service::fork_child);
            //     do_request(); 
            // } else {
            //     global_io_service.notify_fork(io_service::fork_parent);
            //     src_socket.close();
            //     start();
            // }
            do_request();
        }

    private:
        void do_request(){
            auto self(shared_from_this());
            src_socket.async_read_some(
                buffer(src_data, max_length),
                [this, self](boost::system::error_code ec, std::size_t length) {
                    if (!ec) {
                        string tmp_buf(src_data.begin(), src_data.begin()+length);
                        unsigned char* buf = (unsigned char *)tmp_buf.c_str(); 
                        unsigned char VN = buf[0] ;
                        CD = buf[1] ;
                        conn_dst_port = buf[2] << 8 | buf[3] ;

                        char tmp_dst_ip[20];
                        sprintf(tmp_dst_ip,"%u.%u.%u.%u",buf[4],buf[5],buf[6],buf[7]);
                        dst_ip = string(tmp_dst_ip);

                        char tmp_part_ip[4];
                        sprintf(tmp_part_ip,"%u",buf[4]);
                        dst_ip_arr[0] = string(tmp_part_ip);
                        sprintf(tmp_part_ip,"%u",buf[5]);
                        dst_ip_arr[1] = string(tmp_part_ip);
                        sprintf(tmp_part_ip,"%u",buf[6]);
                        dst_ip_arr[2] = string(tmp_part_ip);
                        sprintf(tmp_part_ip,"%u",buf[7]);
                        dst_ip_arr[3] = string(tmp_part_ip);

                        char* USER_ID = (char*)buf + 8 ;

                        int pos = tmp_buf.find('\0', 8);
                        printf("pos: %d\n", pos);
                        int pos2 = 0;
                        if(pos != tmp_buf.size()){
                            pos2 = tmp_buf.find('\0', pos + 1);
                        }
                        printf("pos2: %d\n", pos2);
                        if(pos2 != -1){
                            domain = tmp_buf.substr(pos + 1, pos2-pos);
                            printf("Domain: %s\n", domain.c_str());
                        }


                        if (tmp_buf[1] == 1) {
                            
                            int perm  = fire_wall();
                            // int perm = 1;
                            if (perm == 1) {
                                copy(src_data.begin(), src_data.begin()+8, src_response.begin());
                                src_response[0] = 0;
                                src_response[1] = 90;
                                do_send_response(8);
                                cout << "POS2: " << pos2 << endl;
                                if (pos2 != -1){
                                    cout << "SOCK4a" << endl;
                                    do_resolve();
                                    cout << "<S_IP>: " << src_socket.remote_endpoint().address().to_string() << endl;
                                    cout << "<S_PORT>: " << to_string(src_socket.remote_endpoint().port()) << endl;
                                    cout << "<D_IP>: " << dst_ip << endl;
                                    cout << "<D_PORT>: " << conn_dst_port << endl;
                                    cout << "<Command>: CONNECT" << endl;
                                    cout << "<Reply>: Accept" << endl;
                                } else {
                                    cout << "SOCK4" << endl;
                                    cout << "<S_IP>: " << src_socket.remote_endpoint().address().to_string() << endl;
                                    cout << "<S_PORT>: " << to_string(src_socket.remote_endpoint().port()) << endl;
                                    cout << "<D_IP>: " << dst_ip << endl;
                                    cout << "<D_PORT>: " << conn_dst_port << endl;
                                    cout << "<Command>: CONNECT" << endl;
                                    cout << "<Reply>: Accept" << endl;
                                    do_connect();
                                }
                                
                            } else {
                                cout << "<S_IP>: " << src_socket.remote_endpoint().address().to_string() << endl;
                                cout << "<S_PORT>: " << to_string(src_socket.remote_endpoint().port()) << endl;
                                cout << "<D_IP>: " << dst_ip << endl;
                                cout << "<D_PORT>: " << conn_dst_port << endl;
                                cout << "<Command>: CONNECT" << endl;
                                cout << "<Reply>: Reject" << endl;
                                copy(src_data.begin(), src_data.begin()+8, src_response.begin());
                                src_response[0] = 0;
                                src_response[1] = 91;
                                do_send_response(8);
                                src_socket.close();
                            }
                        } else if (tmp_buf[1] == 2) {
                            int pos = tmp_buf.find('\0', 8);
                            printf("pos: %d\n", pos);
                            int pos2 = 0;
                            if(pos != tmp_buf.size()){
                                pos2 = tmp_buf.find('\0', pos + 1);
                            }
                            printf("pos2: %d\n", pos2);
                            if(pos2 != 0){
                                domain = tmp_buf.substr(pos + 1, pos2-pos);
                                printf("Domain: %s\n", domain.c_str());
                                do_ftp_resolve();
                            }
                            cout << "<S_IP>: " << src_socket.remote_endpoint().address().to_string() << endl;
                            cout << "<S_PORT>: " << to_string(src_socket.remote_endpoint().port()) << endl;
                            cout << "<D_IP>: " << dst_ip << endl;
                            cout << "<D_PORT>: " << conn_dst_port << endl;
                            cout << "<Command>: BIND" << endl;
                            
                            bind_port = ftp_acceptor.local_endpoint().port();
                            src_response[0] = 0;
                            src_response[2] = bind_port/256;
                            src_response[3] = bind_port%256;
                            src_response[4] = 0;
                            src_response[5] = 0;
                            src_response[6] = 0;
                            src_response[7] = 0;

                            int perm  = fire_wall();
                            if (perm == 1) {
                                cout << "<Reply>: Accept" << endl;
                                src_response[1] = 90;
                                do_send_response(8);
                                cout << "Do FTP" << endl;
                                do_ftp_accept();
                                cout << "Done FTP" << endl;
                            } else {
                                cout << "<Reply>: Reject" << endl;
                                bind_port = ftp_acceptor.local_endpoint().port();
                                src_response[1] = 91;
                                do_send_response(8);
                                src_socket.close();
                            }
                        }
                    }
                });
        }

        void do_resolve() {
            auto self(shared_from_this());
            ip::tcp::resolver::query dst_query(ip::tcp::v4(), domain, to_string(conn_dst_port));
            dst_resolver.async_resolve(dst_query, [this,self](boost::system::error_code ec,ip::tcp::resolver::iterator endpoint_iterator) {
                if (!ec) {
                    cout << endpoint_iterator->endpoint().address().to_string() << endl;
                    dst_ip = endpoint_iterator->endpoint().address().to_string();
                    do_connect();
                } 
            }); 
        }

        void do_ftp_resolve() {
            auto self(shared_from_this());
            ip::tcp::resolver::query dst_query(ip::tcp::v4(), domain, to_string(conn_dst_port));
            dst_resolver.async_resolve(dst_query, [this,self](boost::system::error_code ec,ip::tcp::resolver::iterator endpoint_iterator) {
                if (!ec) {
                    cout << endpoint_iterator->endpoint().address().to_string() << endl;
                    dst_ip = endpoint_iterator->endpoint().address().to_string();
                } 
            }); 
        }


        int fire_wall(){
            int permission = 1;
            
            socks_cfg.open("socks.conf");
            string cfg_line;
            regex pattern("permit (b|c) (\\*|[0-9]*).(\\*|[0-9]*).(\\*|[0-9]*).(\\*|[0-9]*)");
            
            while(getline(socks_cfg, cfg_line)){
                permission = 1;
                int flag[4] = {0};
                smatch match;
                regex_search (cfg_line, match, pattern);
                cout << match[1].str() << " " << match[2].str() << " " << match[3].str() << " " << match[4].str() << " " << match[5].str() << endl;
                
                if ((CD == 1 && match[1].str().compare("c") == 0)|| (CD == 2 && match[1].str().compare("b") == 0)) {
                    for(int i = 0 ; i < 4; i++){
                        if(match[i+2].str().compare("*") == 0 || dst_ip_arr[i].compare(match[i+2].str()) == 0){
                            continue;
                        } else {
                            permission = 0;
                            break;
                        }
                    }
                } else {
                    permission = 0;
                }
                if (permission) {
                    break;
                }
            }
            socks_cfg.close();
            return permission;
        }

        void do_ftp_accept(){
            auto self(shared_from_this());
            ftp_acceptor.async_accept(dst_socket, [this,self](boost::system::error_code ec) {
                if(!ec){
                    src_response[0] = 0;
                    src_response[1] = 90;
                    src_response[2] = bind_port/256;
                    src_response[3] = bind_port%256;
                    src_response[4] = 0;
                    src_response[5] = 0;
                    src_response[6] = 0;
                    src_response[7] = 0;
                    do_send_response(8);
                    do_read_src();
                    do_read_dst();
                }else{
                    cout << "FTP error" << endl;
                }
            });
        }

        void do_connect(){
            ip::tcp::endpoint endpoint(ip::address::from_string(dst_ip), conn_dst_port);
            auto self(shared_from_this());
            dst_socket.async_connect(
                endpoint,
                [this, self](boost::system::error_code ec) {
                    if (!ec){
                        cout << "do_connect" << endl;
                        do_read_src();
                        do_read_dst();
                    }
                });
        }

        void do_send_response(size_t length){
            auto self(shared_from_this());
            async_write(
                src_socket,
                buffer(src_response,length),
                [this, self](boost::system::error_code ec, std::size_t /* length */) {
                    if (!ec){
                        // pass
                    }
                });
        }

        void do_read_src() {  
            auto self(shared_from_this());
            src_socket.async_read_some(
                buffer(src_data, max_length),
                [this, self](boost::system::error_code ec, std::size_t length) {
                    if (!ec) {
                        copy(src_data.begin(),src_data.end(),dst_response.begin());
                        do_write_dst(length);
                    } else {
                        src_socket.close();
                        dst_socket.close();
                    }
                });
        }

        void do_write_src(size_t length) {
            auto self(shared_from_this());
            async_write(
                src_socket,
                buffer(src_response,length),
                [this, self](boost::system::error_code ec, std::size_t /* length */) {
                    if (!ec){
                        do_read_dst();
                    }
                });
        }

        void do_read_dst(){
            auto self(shared_from_this());
            dst_socket.async_read_some(
                buffer(dst_data, max_length),
                [this, self](boost::system::error_code ec, std::size_t  length ) {
                    if (!ec){
                        copy(dst_data.begin(), dst_data.end(), src_response.begin());
                        do_write_src(length);
                    }else{
                        dst_socket.close();
                        src_socket.close();
                    }
                });
        }
        void do_write_dst(size_t length) {
            auto self(shared_from_this());
            async_write(
                dst_socket,
                buffer(dst_response,length),
                [this, self](boost::system::error_code ec, std::size_t /* length */) {
                    if (!ec){
                        do_read_src();
                    }
                });
        }
};

class httpServer {
    private:
        ip::tcp::acceptor _acceptor;
        ip::tcp::socket _socket;

    public:
        httpServer(short port)
            :   _acceptor(global_io_service, ip::tcp::endpoint(ip::tcp::v4(), port)),
                _socket(global_io_service) {
                    do_accept();
                }

    private:
        void do_accept() {
            _acceptor.async_accept(_socket, [this](boost::system::error_code ec) {
                if (!ec) {
                    global_io_service.notify_fork(io_service::fork_prepare);
                    if(fork() == 0){
                        global_io_service.notify_fork(io_service::fork_child);
                        _acceptor.close();
                        make_shared<ServerSession>(move(_socket))->start();
                    } else {
                        global_io_service.notify_fork(io_service::fork_parent);
                        _socket.close();
                        do_accept();
                    }
                }
            });
        }
};
void signal_handle(int signum){
    int status;
    while(waitpid(-1,&status,WNOHANG)>0){
    }
}

int main(int argc, char* const argv[]) {
    if (argc != 2) {
        cerr << "Usage:" << argv[0] << " [port]" << endl;
        return 1;
    }
    try {
        unsigned short port = atoi(argv[1]);
        httpServer server(port);
        global_io_service.run();
    } catch (exception& e) {
        cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}