.PHONY:all
all:tcp_server tcp_client

tcp_server:TcpServer.hpp TcpThread.hpp ServerMain.cc
	g++ -o $@ $^ -g -lpthread -std=c++11
tcp_client:TcpClient.hpp ClientMain.cc
	g++ -o $@ $^ -g -std=c++11

.PHONY:clean
clean:
	-rm -f tcp_client tcp_server
