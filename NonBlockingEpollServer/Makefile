.PHONY:all

all:client server

client:Client.hpp ClientMain.cc
	g++ -o $@ $^ -g -std=c++11
server:Server.hpp ServerMain.cc 
	g++ -o $@ $^ -g -std=c++11

.PHONY:clean
clean:
	-rm -f client server