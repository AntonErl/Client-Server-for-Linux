#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <cstdio>
#include <cstring>
#include <iostream>

int main(int argc, const char **argv)
{
    struct sockaddr_in addr;                    // структура с адресом
    int sock = socket(AF_INET, SOCK_STREAM, 0); // создание TCP-сокета
    if (sock < 0)
    {
        perror("socket");
        exit(1);
    }

    // Указываем параметры сервера
    addr.sin_family = AF_INET;                     // домены Internet
    addr.sin_port = htons(3425);                   // или любой другой порт...
    addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP-адрес

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        exit(2);
    }

    std::string commands = "start";
    while (commands != "exit")
    {
        std::getline(std::cin, commands);      //Читаем данные из стандарного потока ввода в commands
        unsigned mess_len = commands.length(); // размер данных (передаваемого сообщения)

        if (commands != "exit")
        {

            char buf[1024];
            send(sock, commands.c_str(), mess_len + 1, 0);
            recv(sock, buf, sizeof buf, 0);
            std::cout << buf << std::endl;
        }
    }
    std::cout << "close client" << std::endl;
    close(sock);
    return 0;
}
