SRCS = src/main.cpp local/Arduino.cpp
OBJS = build/main.o build/Arduino.o
HEADERS = $(wildcard src/*.h)
LOCAL_HEADERS = $(wildcard local/*.h)
CXXFLAGS = -isystem local -Isrc -std=c++11 -Wall -Wextra -Wno-builtin-declaration-mismatch

build/main: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

build/Arduino.o: local/Arduino.cpp $(LOCAL_HEADERS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

build/%.o: src/%.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

test-compile: build/main

test-run: build/main
	./build/main
