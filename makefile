CXX = g++
CXXFLAGS = -g -Wall -lm -pg

.PHONY: all clean

all: list chat threads reactor proactor #adj_m deque

# adj_m: p1_using_adj_matrix.o
# 	$(CXX) $(CXXFLAGS) $^ -o $@

# deque: p1_using_deque.o
# 	$(CXX) $(CXXFLAGS) $^ -o $@
reactor:server_using_reactor.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

proactor:server_using_proactor.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@	

chat: server_chat.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

threads: server_threads.o
	$(CXX) $(CXXFLAGS) $^ -o $@

list: p1_using_list.o
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -lgcov -c $< -o $@

clean:
	rm -f *.o  list threads chat reactor proactor