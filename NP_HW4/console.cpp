#include <array>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace std;
using namespace boost::asio;
io_service global_io_service;

void output_to_shell(string id, string content, bool is_command) {
    boost::replace_all(content,"\r\n","&NewLine;");//Replace all \r\n with &NewLine in content.
    boost::replace_all(content,"\n","&NewLine;");
    boost::replace_all(content, "\\", "\\\\");
    boost::replace_all(content,"\'","\\\'");
    boost::replace_all(content,"<","&lt;");
    boost::replace_all(content,">","&gt;");
    if (is_command) {
        cout << "<script>document.getElementById('" << id << "').innerHTML += '<b>" << content << "</b>';</script>" << endl;
    } else {
        cout << "<script>document.getElementById('" << id << "').innerHTML += '" << content << "';</script>" << endl;
    }
    fflush(stdout);
}

// --------------------------------------------------------------------------------
class Shell_session :public enable_shared_from_this<Shell_session>{
    private:
        enum{max_length = 1024};
        ip::tcp::socket _socket;
        ip::tcp::resolver src_resolver;
        ip::tcp::resolver dst_resolver;
        ip::tcp::resolver::query src_query;
        ip::tcp::resolver::query dst_query;
        string test_case;
        string shell_id;
        string host_id;
        string dest_ip;
        int dest_port;
        array<char, max_length> _data;
        ifstream test_file;
        deadline_timer timer;
    public:
        Shell_session(string host, string port, string in_test_case, string in_shell_id, 
                        string in_host_id, string dst_ip, string dst_port) :
                        _socket(global_io_service), src_resolver(global_io_service), 
                        dst_resolver(global_io_service), src_query(ip::tcp::v4(), host, port), 
                        dst_query(ip::tcp::v4(), dst_ip, dst_port),test_case(in_test_case), 
                        shell_id(in_shell_id), host_id(in_host_id), timer(global_io_service){}
        void start(){
            string test_file_name = "test_case/" + test_case;
            test_file.open(test_file_name);
            // _data.fill('\0');
            do_resolve();
        }

    private:
        void do_resolve() {
            auto self(shared_from_this());
            dst_resolver.async_resolve(dst_query, [this,self](boost::system::error_code ec,ip::tcp::resolver::iterator endpoint_iterator) {
                if (!ec) {
                    dest_ip = endpoint_iterator->endpoint().address().to_string();
                    dest_port = endpoint_iterator->endpoint().port();
                    src_resolver.async_resolve(src_query, [this,self](boost::system::error_code ec,ip::tcp::resolver::iterator endpoint_iterator) {
                        if (!ec) {
                            do_connect(endpoint_iterator);
                        } else {
                            _socket.close();
                        }
                    }); 
                } else {
                    _socket.close();
                }
            }); 
        }

        void do_connect(ip::tcp::resolver::iterator endpoint_iterator){
            auto self(shared_from_this());
            boost::asio::async_connect(_socket, endpoint_iterator, [this, self](boost::system::error_code ec, ip::tcp::resolver::iterator){
                if (!ec) {
                    do_send_request();
                } 
                else {
                    _socket.close();
                }
            }); 
        }

        void do_send_request() {
            auto self(shared_from_this());
            string request = "000000000";
            smatch sm;
            regex pattern("([0-9]*).([0-9]*).([0-9]*).([0-9]*)");
            regex_search (dest_ip,sm,pattern);
            request[0] = 4;
            request[1] = 1;
            request[2] = dest_port/256;
            request[3] = dest_port%256;
            request[4] = stoi(sm[1].str());
            request[5] = stoi(sm[2].str());
            request[6] = stoi(sm[3].str());
            request[7] = stoi(sm[4].str());
            request[8] = 0; 
            _socket.async_send(buffer(request),[this, self](boost::system::error_code ec, std::size_t /* length */) {
                if (!ec) {
                    // do_read_response();
                    do_read();
                } else {
                    _socket.close();
                }
            });
        }

        void do_read_response() {
            //cout<<"<h1>3"<<test_case<<"</h1></br>"<<endl;
            auto self(shared_from_this());
            _socket.async_read_some(
                buffer(_data, max_length),[this, self](boost::system::error_code ec, std::size_t length) {
                    if(!ec){
                        do_read();
                    } else {
                        _socket.close();
                    }
                });
        }

        void do_read() {
            auto self(shared_from_this());
            _socket.async_read_some(
                buffer(_data),[this, self](boost::system::error_code ec, std::size_t length) {
                    if (!ec) {
                        // string buf(_data.begin(),_data.begin()+length);
                        string buf(_data.data());
                        output_to_shell(shell_id, buf, false);
                        if (buf.find("% ") != string::npos){
                            do_send_cmd();
                        } else {
                            do_read();
                        }
                    } else {
                        _socket.close();
                    }
                });
            
        }
        

        void do_send_cmd() {
            auto self(shared_from_this());
            string tmp;
            getline(test_file,tmp);
            tmp +="\n";
            output_to_shell(shell_id, tmp, true);
            _socket.async_send(buffer(tmp),[this, self](boost::system::error_code ec, std::size_t /* length */) {
                if (!ec) {
                    do_read();
                } else {
                    _socket.close();
                }
            });
        }
};

// --------------------------------------------------------------------------------
class client{
    private:
        string ip;
        string port;
        string test_case;
        string shell_id;
        string host_id;
        string socket_ip;
        string socket_port;

    public:
        client(string in_ip, string in_port, string in_test_case, int id, string sock_ip, string sock_port){
            ip = in_ip;
            port = in_port;
            test_case = in_test_case;
            shell_id = "s"+to_string(id);
            host_id = "h"+to_string(id);
            socket_ip = sock_ip;
            socket_port = sock_port;
        }
        void start(){
            string tmp = ip+":"+port;
            output_to_shell(host_id, tmp, false);
            make_shared<Shell_session>(socket_ip, socket_port, test_case, shell_id, host_id, ip, port)->start();
        }
};



int main(){
    char* query_string = getenv("QUERY_STRING");
    string query(query_string);
    query = "?" + query;
    smatch str_match;
    regex patterns("((?:\\?|&)\\w+=)([^&]+)");
    regex sock_patterns("(.*)&sh=(.*)&sp=(.*)");

    cout << "HTTP/1.1 200 OK"<<endl;
    cout << "Content-type: text/html" << endl << endl;
    string html = 
        R"(<!DOCTYPE html>
        <html lang="en">
        <head>
            <meta charset="UTF-8" />
            <title>NP Project 3 Console</title>
            <link
            rel="stylesheet"
            href="https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css"
            integrity="sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO"
            crossorigin="anonymous"
            />
            <link
            href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
            rel="stylesheet"
            />
            <link
            rel="icon"
            type="image/png"
            href="https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png"
            />
            <style>
            * {
                font-family: 'Source Code Pro', monospace;
                font-size: 1rem !important;
            }
            body {
                background-color: #212529;
            }
            pre {
                color: #cccccc;
            }
            b {
                color: #ffffff;
            }
            </style>
        </head>
        <body>
            <table class="table table-dark table-bordered">
            <thead>
                <tr>
                    <th><pre id="h0" class="mb-0"></pre></th>
                    <th><pre id="h1" class="mb-0"></pre></th>
                    <th><pre id="h2" class="mb-0"></pre></th>
                    <th><pre id="h3" class="mb-0"></pre></th>
                    <th><pre id="h4" class="mb-0"></pre></th>
                </tr>
            </thead>
            <tbody>
                <tr>
                    <td><pre id="s0" class="mb-0"></pre></td>
                    <td><pre id="s1" class="mb-0"></pre></td>
                    <td><pre id="s2" class="mb-0"></pre></td>
                    <td><pre id="s3" class="mb-0"></pre></td>
                    <td><pre id="s4" class="mb-0"></pre></td>
                </tr>
            </tbody>
            </table>
        </body>
        </html>)";
    cout << html;

    regex_search(query, str_match, sock_patterns);
    string sock_ip = str_match[2].str();
    string sock_port = str_match[3].str();
    query = str_match[1].str();
    int id = 0;
    vector<client> clients;
    while (regex_search (query, str_match, patterns)) {
        string ip = str_match[2].str();
        query = str_match.suffix().str();

        regex_search (query, str_match, patterns);
        string port = str_match[2].str();
        query = str_match.suffix().str();

        regex_search (query, str_match, patterns);
        string test_case = str_match[2].str();
        query = str_match.suffix().str();
        client tmp(ip, port, test_case, id, sock_ip, sock_port);
        clients.push_back(tmp);
        id++;
    }
    try {
        for(int i=0;i<clients.size();i++){
            clients[i].start();
        }
        global_io_service.run();
    } catch (exception& e) {
        cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}