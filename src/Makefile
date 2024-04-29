CXX = g++
CXXFLAGS = -Wall -std=c++11
BOOST_LIBS = -lboost_filesystem -lboost_system
PTHREAD_LIB = -lpthread
TBB_LIBS = -ltbb
SRC = main.cpp proxy.cpp request_cache.cpp parser.cpp logger.cpp
EXECUTABLE = app

all: $(SRC)
	$(CXX) $(CXXFLAGS) -o $(EXECUTABLE) $(SRC) $(BOOST_LIBS) $(PTHREAD_LIB) $(TBB_LIBS)

clean:
	rm -f $(EXECUTABLE)