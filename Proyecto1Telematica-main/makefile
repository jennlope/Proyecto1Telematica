# Makefile - Compilador

CXX = g++
CXXFLAGS = -Wall -std=c++20 -Iinclude -pthread
SRC = src/server.cpp src/http_parser.cpp src/file_server.cpp
OUT = server

all: $(OUT)

$(OUT): $(SRC) include/http_parser.h include/file_server.h
	$(CXX) $(CXXFLAGS) -o $(OUT) $(SRC)

clean:
	rm -f $(OUT)