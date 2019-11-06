#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <set>
#include <queue>
#include <iostream>
#include <thread>
#include <mutex>

using namespace std;

set<int> clients;
std::mutex mutex_Buf_Read;
std::mutex mutex_Buf_Write;
std::queue<std::string> g_BufRead;
std::queue<std::string> g_BufWrite;
std::queue<int> g_BufReadClient;
std::queue<int> g_BufWriteClient;

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

void in_data(int listener)
{
    char buf[1024];
    int bytes_read;
    while (1)
    {
        // Заполняем множество сокетов
        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(listener, &readset);

        for (set<int>::iterator it = clients.begin(); it != clients.end(); it++)
            FD_SET(*it, &readset);

        // Задаём таймаут
        timeval timeout;
        timeout.tv_sec = 120;
        timeout.tv_usec = 0;

        // Ждём события в одном из сокетов
        int mx = max(listener, *max_element(clients.begin(), clients.end()));
        if (select(mx + 1, &readset, NULL, NULL, &timeout) <= 0)
        {
            perror("select");
            exit(3);
        }

        // Определяем тип события и выполняем соответствующие действия
        if (FD_ISSET(listener, &readset))
        {
            // Поступил новый запрос на соединение, используем accept
            int sock = accept(listener, NULL, NULL);
            if (sock < 0)
            {
                perror("accept");
                exit(3);
            }

            clients.insert(sock);
        }

        for (set<int>::iterator it = clients.begin(); it != clients.end(); it++)
        {
            if (FD_ISSET(*it, &readset))
            {
                // Поступили данные от клиента, читаем их
                bytes_read = recv(*it, buf, 1024, 0);

                if (bytes_read <= 0)
                {
                    // Соединение разорвано, удаляем сокет из множества
                    close(*it);
                    clients.erase(*it);
                    continue;
                }
                mutex_Buf_Read.lock();
                g_BufRead.push(buf);
                g_BufReadClient.push(*it);
                mutex_Buf_Read.unlock();
            }
        }
    }
}
void out_data()
{
    std::string result_command;
    int cl;
    int count = 0;
    while (1)
    {
        if (count < 24)
        {
            if (g_BufWrite.empty())
            {
                sleep(5);
                count++;
            }
            else
            {
                mutex_Buf_Write.lock();
                result_command = g_BufWrite.front();
                g_BufWrite.pop();
                cl = g_BufWriteClient.front();
                g_BufWriteClient.pop();
                mutex_Buf_Write.unlock();
                send(cl, result_command.c_str(), 1024, 0);
                count = 0;
            }
        }
        else
        {
            break;
        }
    }
}
void run_command()
{
    int count = 0;
    while (1)
    {
        if (count < 24)
        {
            if (g_BufRead.empty())
            {
                sleep(5);
                count++;
            }
            else
            {
                std::string command;
                int client = 0;
                mutex_Buf_Read.lock();
                command = g_BufRead.front();
                client = g_BufReadClient.front();
                g_BufRead.pop();
                g_BufReadClient.pop();
                mutex_Buf_Read.unlock();
                std::string result_command = exec(command.c_str());
                mutex_Buf_Write.lock();
                g_BufWrite.push(result_command);
                g_BufWriteClient.push(client);
                mutex_Buf_Write.unlock();
                count = 0;
            }
        }
        else
        {
            break;
        }
    }
}

int main()
{
    int listener;
    struct sockaddr_in addr;

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0)
    {
        perror("socket");
        exit(1);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(3425);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(2);
    }

    listen(listener, 20);
    clients.clear();

    /////////////////////////////////////
    std::thread thread_in_data(in_data, listener);
    std::thread thread_run_command(run_command);
    std::thread thread_out_data(out_data);

    if (thread_in_data.joinable())
        thread_in_data.join();
    if (thread_run_command.joinable())
        thread_run_command.join();
    if (thread_out_data.joinable())
        thread_out_data.join();
    /////////////////////////////////////
    std::cout << "close server" << std::endl;
    close(listener); //закрываем сокет сервера
    return 0;
}