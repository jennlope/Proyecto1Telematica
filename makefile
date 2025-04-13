# Makefile

CXX = g++
CXXFLAGS = -Wall -std=c++17 -Iinclude
SRC = src/server.cpp src/http_parser.cpp
OUT = server

all:
	$(CXX) $(CXXFLAGS) -o $(OUT) $(SRC)

clean:
	rm -f $(OUT)