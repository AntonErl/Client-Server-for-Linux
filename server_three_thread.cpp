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

struct Message
{
    int client = 0;
    std::string data = "";
    Message(int client, std::string data)
    {
        this->client = client;
        this->data = data;
    }
};

std::set<int> clients;
std::mutex mutex_Buf_Read;
std::mutex mutex_Buf_Write;
std::queue<Message> g_BufRead;
std::queue<Message> g_BufWrite;

std::string exec(const char *cmd)
{
    char buffer[1024];
    std::string result = "";
    FILE *pipe = popen(cmd, "r");
    if (!pipe)
        throw std::runtime_error("popen() failed!");

    while (fgets(buffer, sizeof buffer, pipe) != NULL)
    {
        result += buffer;
    }

    pclose(pipe);
    return result;
}

void in_data(int listener)
{
    bool flag = true; //управляющая переменная для while
    char buf[1024];
    int bytes_read;
    while (flag)
    {
        // Заполняем множество сокетов
        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(listener, &readset);

        for (int client : clients)
        {
            FD_SET(client, &readset);
        }

        // Задаём таймаут
        timeval timeout;
        timeout.tv_sec = 120;
        timeout.tv_usec = 0;

        // Ждём события в одном из сокетов
        int mx = std::max(listener, *std::max_element(clients.begin(), clients.end()));
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

        for (int client : clients)
        {
            if (FD_ISSET(client, &readset))
            {
                // Поступили данные от клиента, читаем их
                bytes_read = recv(client, buf, sizeof buf, 0);

                if (bytes_read <= 0)
                {
                    // Соединение разорвано, удаляем сокет из множества
                    close(client);
                    clients.erase(client);
                    continue;
                }
                mutex_Buf_Read.lock();
                g_BufRead.push(Message(client, buf));
                mutex_Buf_Read.unlock();
            }
        }
    }
    for (int client : clients)
    {
        close(client);
    }
    close(listener);
}
void out_data()
{
    bool flag = true; //управляющая переменная для while
    std::string result_command;
    Message result(0, "NuN");
    int client;
    int count = 0;
    while (flag)
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
                result = g_BufWrite.front();
                client = result.client;
                result_command = result.data;
                g_BufWrite.pop();
                mutex_Buf_Write.unlock();
                send(client, result_command.c_str(), 1024, 0);
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
    bool flag = true; //управляющая переменная для while
    int count = 0;
    while (flag)
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
                Message result(0, "NuN");
                std::string command;
                int client = 0;
                mutex_Buf_Read.lock();
                result = g_BufRead.front();
                client = result.client;
                command = result.data;
                g_BufRead.pop();
                mutex_Buf_Read.unlock();
                std::string result_command = exec(command.c_str());
                mutex_Buf_Write.lock();
                g_BufWrite.push(Message(client, result_command));
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
    int max_size_queue = 20;

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

    listen(listener, max_size_queue);
    clients.clear();

    std::thread thread_in_data(in_data, listener);
    std::thread thread_run_command(run_command);
    std::thread thread_out_data(out_data);

    if (thread_in_data.joinable())
        thread_in_data.join();
    if (thread_run_command.joinable())
        thread_run_command.join();
    if (thread_out_data.joinable())
        thread_out_data.join();

    std::cout << "close server" << std::endl;
    close(listener); //закрываем сокет сервера
    return 0;
}