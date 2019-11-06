# Client-Server-for-Linux
Client-Server for Linux.

server_one_thread.cpp:
In the client the Linux command is entered and the server executes it and sends the result.

server_two_thread.cpp:
In the client the Linux command is entered and the server executes it and sends the result. There are two threads on the server. One receives the data and sends the result. Another processes the commands.

server_three_thread.cpp:
In the client the Linux command is entered and the server executes it and sends the result. There are three threads on the server. The first takes data. The second sends the result. The third processes the commands.
