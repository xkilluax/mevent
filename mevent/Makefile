CXX       = g++
OS        := $(shell uname)
ifeq ($(OS), Linux)
CXX       = /opt/centos/devtoolset-1.1/root/usr/bin/g++
endif
#CC        = gcc
ifdef DEBUG
CXXFLAGS  = -g -Wall -Wextra -std=c++0x
else
CXXFLAGS  = -O2 -Wall -Wextra -std=c++0x
endif
INCLUDES  = 
LIBS      = -lpthread -lcrypto -lcurl

OBJS = http_server.o \
	   event_loop.o \
	   util.o \
	   request.o \
	   response.o \
	   connection.o \
	   connection_pool.o \
	   base64.o \
	   ternary_search_tree.o \
	   websocket.o \
	   lock_guard.o \
	   http_client.o \
	   event_loop_base.o 

all : examples/chat_room examples/hello_world examples/form_action

examples/chat_room: $(OBJS) examples/chat_room.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)
examples/chat_room.o : examples/chat_room.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

examples/hello_world: $(OBJS) examples/hello_world.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)
examples/hello_world.o : examples/hello_world.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

examples/form_action: $(OBJS) examples/form_action.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)
examples/form_action.o : examples/form_action.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)


http_server.o : http_server.cpp http_server.h
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)
event_loop.o : event_loop.cpp event_loop.h
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)
util.o : util.cpp util.h
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)
request.o : request.cpp request.h
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)
response.o : response.cpp response.h
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)
connection.o : connection.cpp connection.h conn_status.h
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)
connection_pool.o : connection_pool.cpp connection_pool.h
	$(CXX) $(CXXFLAGS) -c $< -o $@  $(INCLUDES)
base64.o : base64.cpp base64.h
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)
ternary_search_tree.o : ternary_search_tree.cpp ternary_search_tree.h
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)
websocket.o : websocket.cpp websocket.h
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)
lock_guard.o : lock_guard.cpp lock_guard.h
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)
http_client.o : http_client.cpp http_client.h
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)
event_loop_base.o : event_loop_base.cpp event_loop_base.h
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)


.PHONY : clean
clean :
	rm -f *.o
	rm -f examples/*.o
	rm -f examples/example1
