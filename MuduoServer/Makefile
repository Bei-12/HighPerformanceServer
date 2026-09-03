.PHONY:all

all:client server stress
client: ClientMain.cc 
	g++ -o $@ $^ -g  -std=c++11
server:ServerMain.cc 
	g++ -o $@ $^ -g -std=c++11 -pthread
stress:StressMain.cc
	g++ -o $@ $^ -O2 -std=c++11

.PHONY:clean
clean:
	-rm -f client server stress