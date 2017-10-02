spsc:	catch.cpp spsc.cpp
	$(CXX) -std=c++11 -Wall -Wextra -O2 $^ -pthread -o $@

clean:
	rm -f spsc

check:	spsc
	./spsc
