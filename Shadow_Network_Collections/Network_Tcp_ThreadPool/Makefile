.PHONY:all

all:tcp_client tcp_server
tcp_client:Tcp_Client.hpp ClientMain.cc
	g++ -o $@ $^ -g -std=c++11
tcp_server:Tcp_Server.hpp Tcp_ThreadPool.hpp ServerMain.cc 
	g++ -o $@ $^ -g -lpthread -std=c++11

.PHONY:clean
clean:
	-rm -f tcp_client tcp_server