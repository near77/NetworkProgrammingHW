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

using namespace std;
using namespace boost::asio;

io_service global_io_service;

class ServerSession : public enable_shared_from_this<ServerSession> {
    private:
        enum { max_length = 1024 };
        ip::tcp::socket _socket;
        array<char, max_length> _data;

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
                    regex pattern("([A-Z]+) (/([a-z]+.cgi)?(.*)) (.*)\r\nHost: (.*):(.*)");
                    smatch str_match;
                    string cgi;
                    string s(_data.begin(), _data.begin()+length);
                    
                    cout << regex_search (s, str_match, pattern) << endl;
                    if (regex_search (s, str_match, pattern)){
                        env["REQUEST_METHOD"] = str_match[1];
                        env["REQUEST_URI"] = str_match[2];
                        cgi = str_match[3];
                        if (str_match[4].str().size() != 0){
                            env["QUERY_STRING"] = str_match[4].str().substr(1, str_match[4].str().size()-1);
                        }
                        env["SERVER_PROTOCOL"] = str_match[5];
                        env["HTTP_HOST"] = str_match[6];
                        env["SERVER_ADDR"] = _socket.local_endpoint().address().to_string();
                        env["SERVER_PORT"] = to_string(_socket.local_endpoint().port());
                        env["REMOTE_ADDR"] = _socket.remote_endpoint().address().to_string();
                        env["REMOTE_PORT"] = to_string(_socket.remote_endpoint().port());
                        cout << "CGI: " << cgi << endl;
                        cout << "REQUEST METHOD: " <<  str_match[1] << endl;
                        cout << "REQUEST URI: " << str_match[2] << endl;
                        cout << "QUERY STRING: " << str_match[4] << endl;
                        cout << "SERVER PROTOCOL: " << str_match[5] << endl;
                        cout << "HTTP HOST: " << str_match[6] << endl;
                        cout << "SERVER ADDR: " << _socket.local_endpoint().address().to_string() << endl;
                        cout << "SERVER PORT: " << to_string(_socket.local_endpoint().port()) << endl;
                        cout << "REMOTE ADDR: " << _socket.remote_endpoint().address().to_string() << endl;
                        cout << "REMOTE PORT: " << to_string(_socket.remote_endpoint().port()) << endl;
                    }
                    cgi = "./" + cgi;
                    cout << cgi << endl;
                    pid_t pid;
                    global_io_service.notify_fork(io_service::fork_prepare);
                    pid = fork();
                    if (pid == 0) {
                        global_io_service.notify_fork(io_service::fork_child);
                        char* argv[2] = {(char*)cgi.c_str(), NULL};
                        map<string, string>::iterator iterate;
                        for (iterate = env.begin(); iterate != env.end(); iterate++) {
                            setenv(iterate->first.c_str(), iterate->second.c_str(), 1);
                            cout << iterate->first.c_str() << ": " << getenv(iterate->first.c_str()) << endl;
                        }
                        dup2(_socket.native_handle(), STDIN_FILENO);
                        dup2(_socket.native_handle(), STDOUT_FILENO);
                        dup2(_socket.native_handle(), STDERR_FILENO);
                        _socket.close();
                        cout << "HTTP/1.1 200 OK" << endl;
                        cout << "12345" << endl;
                        if (execvp(argv[0],argv) == -1) {
                            cout << "HTTP/1.1 200 OK" << endl;
                            cout << "Content-type: text/html" << endl << endl;
                            cout << "<h1>Hello World</h1>" << endl;
                        }
                    } else {
                        global_io_service.notify_fork(io_service::fork_parent);
                        _socket.close();
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
    signal(SIGCHLD, signal_handle);
    clearenv();
    setenv("PATH", "/usr/bin:.", 1);
    try {
        unsigned short port = atoi(argv[1]);
        httpServer server(port);
        global_io_service.run();
    } catch (exception& e) {
        cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}