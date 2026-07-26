.PHONY:all
all:udp_server udp_client

udp_server:server.cc Server.hpp
	g++ -o $@ $^ -g -std=c++11
udp_client:client.cc Client.hpp
	g++ -o $@ $^ -g -std=c++11

.PHONY:clean
clean:
	-rm -f udp_client udp_server