#include <array>
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <string>
#include <regex>
#include <stdlib.h>
#include <map>
#include <boost/algorithm/string.hpp>
#include <fstream>

using namespace std;
using namespace boost::asio;

io_service global_io_service;
string host_menu;


class client;
class Shell_session :public enable_shared_from_this<Shell_session>{
    private:
        enum{max_length = 1024};
        shared_ptr<client> client_ptr;
        ip::tcp::socket _socket;
        ip::tcp::socket &web_socket;
        ip::tcp::resolver _resolver;
        ip::tcp::resolver::query query;
        string output_content;
        string test_case;
        string shell_id;
        string host_id;
        string ip;
        string port;
        array<char, max_length> _data;
        ifstream test_file;
    public:
        Shell_session(string in_host, string in_port, string in_test_case, string in_shell_id, string in_host_id , ip::tcp::socket &in_socket, shared_ptr<client> in_ptr) :
                        _socket(global_io_service), _resolver(global_io_service), query(ip::tcp::v4(), in_host, in_port), 
                        test_case(in_test_case), shell_id(in_shell_id), host_id(in_host_id), ip(in_host), port(in_port),
                        web_socket(in_socket), client_ptr(in_ptr){}
        void start(){
            string ip_port_str = ip + ":" + port;
            output_to_shell(host_id, ip_port_str, false);
            string test_file_name = "test_case/" + test_case;
            test_file.open(test_file_name);
            do_resolve();
        }

    private:
        void do_resolve() {
            auto self(shared_from_this());
            _resolver.async_resolve(query, [this,self](boost::system::error_code ec,ip::tcp::resolver::iterator endpoint_iterator) {
                if (!ec) {
                    boost::asio::async_connect(_socket, endpoint_iterator, [this, self](boost::system::error_code ec, ip::tcp::resolver::iterator){
                        if (!ec) {
                            do_read();
                        } else {
                            _socket.close();
                        }
                    }); 
                } else {
                    _socket.close();
                }
            }); 
        }
        void do_read() {
            auto self(shared_from_this());
            _socket.async_read_some(
                buffer(_data, max_length),[this, self](boost::system::error_code ec, std::size_t length) {
                    if (!ec) {
                        string buf(_data.begin(),_data.begin()+length);
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
        void output_to_shell(string id, string content, bool is_command) {
            boost::replace_all(content,"\r\n","&NewLine;");//Replace all \r\n with &NewLine in content.
            boost::replace_all(content,"\n","&NewLine;");
            boost::replace_all(content, "\\", "\\\\");
            boost::replace_all(content,"\'","\\\'");
            boost::replace_all(content,"<","&lt;");
            boost::replace_all(content,">","&gt;");

            if (is_command) {
                output_content = "<script>document.getElementById('" + id + "').innerHTML += '<b>" + content + "</b>';</script>";
            } else {
                output_content = "<script>document.getElementById('" + id + "').innerHTML += '" + content + "';</script>";
            }
            auto shared_string = make_shared<string>(output_content);
            auto self(shared_from_this());
            web_socket.async_send(buffer(*shared_string), [this,self,shared_string](boost::system::error_code ec, std::size_t /* length */){
                if (!ec) {
                    cout << "Send Success." << endl;
                }
            });
        }
};
class ServerSession;

class client : public enable_shared_from_this<client>{
    private:
        string ip;
        string port;
        string test_case;
        string shell_id;
        string host_id;
        ip::tcp::socket &web_socket;
        shared_ptr<ServerSession> http_server_ptr; 
    public:
        client(string in_ip, string in_port, string in_test_case,int id, ip::tcp::socket &in_socket, shared_ptr<ServerSession> in_ptr):
        web_socket(in_socket), http_server_ptr(in_ptr){
            ip = in_ip;
            port = in_port;
            test_case = in_test_case;
            shell_id = "s"+to_string(id);
            host_id = "h"+to_string(id);
        }
        void start(){
            make_shared<Shell_session>(ip, port, test_case, shell_id, host_id, web_socket, shared_from_this())->start();
        }
};

class ServerSession : public enable_shared_from_this<ServerSession> {
    private:
        enum { max_length = 1024 };
        ip::tcp::socket _socket;
        array<char, max_length> _data;
        string html_content;
    public:
        ServerSession(ip::tcp::socket socket) : _socket(move(socket)) {}

    void start() { 
        do_read(); 
    }

    private:
        map<string,string> env;
        void do_read() {
            auto self(shared_from_this());
            _socket.async_read_some(buffer(_data, max_length),
                [this, self](boost::system::error_code ec, size_t length) {
                    string s(_data.begin(), _data.begin()+length);
                    do_command(s);
                });
        }
        void do_command(string input_str){
            smatch str_match;
            map<string, string> env;
            regex pattern("([A-Z]+) (/([a-z]+.cgi)?(.*)) (.*)\r\nHost: (.*):(.*)");
            if (regex_search(input_str, str_match, pattern)){
                string cgi = str_match[3].str();
                cgi = "./" + cgi;
                if (cgi.compare("./panel.cgi") == 0){
                    cout << "panel" << endl;
                    html_content = "HTTP/1.1 200 OK\r\n";
                    html_content += "Content-type: text/html\r\n\r\n";
                    html_content += 
                        R"(<!DOCTYPE html>
                        <html lang="en">
                            <head>
                                <title>NP Project 3 Panel</title>
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
                                    href="https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png"
                                />
                                <style>
                                    * {
                                        font-family: 'Source Code Pro', monospace;
                                    }
                                </style>
                            </head>
                            <body class="bg-secondary pt-5">
                    
                                <form action="console.cgi" method="GET">
                                    <table class="table mx-auto bg-light" style="width: inherit">
                                        <thead class="thead-dark">
                                            <tr>
                                                <th scope="col">#</th>
                                                <th scope="col">Host</th>
                                                <th scope="col">Port</th>
                                                <th scope="col">Input File</th>
                                            </tr>
                                        </thead>
                                        <tbody>)";
                    for(int i = 0; i < 5; i++){
                        string tmp = R"(<tr>
                                            <th scope="row" class="align-middle">Session )";
                        html_content += tmp;
                        tmp = to_string(i + 1) + R"(</th>
                                            <td>
                                                <div class="input-group">
                                                    <select name="h)";
                        html_content += tmp;
                        html_content += to_string(i);
                        tmp = R"(" class="custom-select">
                                                        <option></option>)";
                        html_content += tmp;
                        html_content += host_menu;
                        tmp = R"(
                                                    </select>
                                                    <div class="input-group-append">
                                                        <span class="input-group-text">.cs.nctu.edu.tw</span>
                                                    </div>
                                                </div>
                                            </td>
                                            <td>
                                                <input name="p)";
                        html_content += tmp;
                        html_content += to_string(i);
                        tmp = R"(" type="text" class="form-control" size="5" />
                                            </td>
                                            <td>
                                                <select name="f)";
                        html_content += tmp;
                        html_content += to_string(i);
                        tmp = R"(" class="custom-select">
                                                    <option></option>
                                                    <option value="t1.txt">t1.txt</option>
                                                    <option value="t2.txt">t2.txt</option>
                                                    <option value="t3.txt">t3.txt</option>
                                                    <option value="t4.txt">t4.txt</option>
                                                    <option value="t5.txt">t5.txt</option>
                                                    <option value="t6.txt">t6.txt</option>
                                                    <option value="t7.txt">t7.txt</option>
                                                    <option value="t8.txt">t8.txt</option>
                                                    <option value="t9.txt">t9.txt</option>
                                                    <option value="t10.txt">t10.txt</option>
                                                </select>
                                            </td>
                                        </tr>)";
                        html_content += tmp;
                    }   
                    string tmp = R"(<tr>
                                                <td colspan="3"></td>
                                                <td>
                                                    <button type="submit" class="btn btn-info btn-block">Run</button>
                                                </td>
                                            </tr>
                                        </tbody>
                                    </table>
                                </form>
                            </body>
                        </html>)";
                    html_content += tmp;
                    send_command();
                } else {
                    html_content = "HTTP/1.1 200 OK\r\n";
                    html_content += "Content-type: text/html\r\n\r\n";
                    html_content += 
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
                    send_command();
                    string query = str_match[4].str();
                    smatch q_match;
                    regex pattern("((?:\\?|&)\\w+=)([^&]+)");
                    int id=0;
                    vector<client> clients;
                    while (regex_search (query, str_match, pattern)) {
                        string ip = str_match[2].str();
                        query = str_match.suffix().str();

                        regex_search (query, str_match, pattern);
                        string port = str_match[2].str();
                        query = str_match.suffix().str();

                        regex_search (query, str_match, pattern);
                        string test_case = str_match[2].str();
                        query = str_match.suffix().str();
                        make_shared<client>(ip, port, test_case, id, _socket, shared_from_this())->start();
                        id++;
                    }
                }
            }
        }
        void send_command(){
            auto self(shared_from_this());
            _socket.async_send(buffer(html_content), [this, self](boost::system::error_code ec, std::size_t /* length */) {
                if (ec) {
                    cout << "Send Command Error." << endl;
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
                if (!ec) make_shared<ServerSession>(move(_socket))->start();
                do_accept();
            });
        }
};



int main(int argc, char* const argv[]) {
	if (argc != 2) {
		std::cerr << "Usage:" << argv[0] << " [port]" << endl;
		return 1;
	}
	host_menu = "";
	for (int i = 0; i < 12; i++) {
		host_menu += "<option value = \"nplinux"+ to_string(i + 1) + ".cs.nctu.edu.tw\">nplinux" + to_string(i + 1) + "</option>";
	}
	try {
		short port = atoi(argv[1]);
		httpServer server(port);
		global_io_service.run();
	}
	catch (exception& e) {
		cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}

