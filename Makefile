shadow_thread:shadow_thread.cpp
	g++ -o  shadow_thread shadow_thread.cpp -lpthread

.PHONY:clean 
clean:
	rm -f shadow_thread 
