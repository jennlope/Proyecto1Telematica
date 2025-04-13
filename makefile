# Makefile - Compilador

CXX = g++
CXXFLAGS = -Wall -std=c++20 -Iinclude
SRC = src/server.cpp src/http_parser.cpp src/file_server.cpp
OUT = server

all:
	$(CXX) $(CXXFLAGS) -o $(OUT) $(SRC)

clean:
	rm -f $(OUT)