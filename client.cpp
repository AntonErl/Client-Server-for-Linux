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

std::string exec(const char *cmd)
{
    char buffer[1024];
    std::string result = "";
    FILE *pipe = popen(cmd, "r");
    if (!pipe)
        throw std::runtime_error("popen() failed!");
    try
    {
        while (fgets(buffer, sizeof buffer, pipe) != NULL)
        {
            result += buffer;
        }
    }
    catch (...)
    {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    return result;
}

char *resize(const char *str, unsigned size, unsigned new_size)
{
    char *new_str = new char[new_size];
    int len = 0;
    if (size < new_size)
    {
        len = size;
    }
    else
    {
        len = new_size;
    }
    for (int i = 0; i < len; i++)
    {
        new_str[i] = str[i];
    }
    delete[] str;
    return new_str;
}

char *getline() // Чтение строки с консоли
{
    int ChSize = 40;
    int ChLen = ChSize - 1;
    char *buf = resize(0, 0, ChSize);
    char *str = buf;
    char const *end = str + ChLen;
    size_t len = ChLen;
    char ch = std::getc(stdin);
    while (ch != EOF && ch != '\n')
    {
        *str++ = ch;
        if (str == end)
        {
            buf = resize(buf, len, len + ChSize);
            str = buf + len;
            end = str + ChLen;
            len += ChLen;
        }
        ch = std::getc(stdin);
    }
    *str = 0;
    return buf;
}

unsigned message_len(const char *str)
{
    unsigned i = 0;
    while (str[i] != '\0')
    {
        i++;
    }
    return i;
}

int main(int argc, const char **argv)
{
    std::string commands = "start";
    while (commands != "exit")
    {
        char *message = getline();                //Передаваемы данные
        unsigned mess_len = message_len(message); // размер данных (передаваемого сообщения)
        commands = message;
        if (commands != "exit")
        {

            char buf[1024];

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

            send(sock, message, mess_len + 1, 0);
            //recv(sock, buf, mess_len + 1, 0);
            recv(sock, buf, 1024, 0);
            std::cout << buf << std::endl;
            close(sock);

            /*std::string result_command = exec(message);
            std::cout << "result command" << std::endl;
            std::cout << result_command << std::endl;*/
        }
        delete message;
    }
    std::cout << "close client" << std::endl;
    return 0;
}
