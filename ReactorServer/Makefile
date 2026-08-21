.PHONY:all

all:client server
client: ClientMain.cc 
	g++ -o $@ $^ -g -std=c++11
server:ServerMain.cc 
	g++ -o $@ $^ -g -std=c++11

.PHONY:clean
clean:
	-rm -f client server