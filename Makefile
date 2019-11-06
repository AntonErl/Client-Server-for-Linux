all: client server_one_thread server_two_thread server_three_thread

client: client.cpp
	g++ client.cpp -o client -std=c++14

server_one_thread: server_one_thread.cpp
	g++ server_one_thread.cpp -o server_one_thread -lpthread -std=c++14

server_two_thread: server_two_thread.cpp
	g++ server_two_thread.cpp -o server_two_thread -lpthread -std=c++14

server_three_thread: server_three_thread.cpp
	g++ server_three_thread.cpp -o server_three_thread -lpthread -std=c++14

clean:
	rm -f client.o server_one_thread.o server_two_thread.o server_three_thread.o