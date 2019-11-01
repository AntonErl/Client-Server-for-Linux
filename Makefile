all: client server

client: client.cpp
	g++ client.cpp -o client -std=c++11

server: server.cpp
	g++ server.cpp -o server -lpthread -std=c++11

clean:
	rm -f client.o server.o