#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <algorithm>
#include <set>
#include <queue>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <csignal>
#include <signal.h>
#include <errno.h>
#include <string.h>
 

struct Message
{
    int client = 0;
    std::string data = "";
    Message(int client, std::string data)
    {
        this->client = client;
        this->data = data;
    }
    Message()
    {
    }
};

volatile bool done = false; //управляющая переменная для while
bool read_notified = false;
bool write_notified = false;

std::set<int> clients;

std::condition_variable read_cond_var;
std::condition_variable write_cond_var;
std::mutex mutex_Buf_Read;
std::mutex mutex_Buf_Write;
std::queue<Message> g_BufRead;
std::queue<Message> g_BufWrite;

std::string exec(const char *cmd)
{
    std::string result = "";

    char buffer[1024];

    FILE *pipe = popen(cmd, "r");
    if (!pipe)
    {
        std::string errors = strerror(errno);
        result = "popen() failed! /n" + errors;
    }
    while (fgets(buffer, sizeof buffer, pipe) != NULL)
    {
        result += buffer;
    }

    pclose(pipe);
    return result;
}

void signalHandler(int signum)
{
    done = true;
}

void in_data(int listener)
{
    char buf[1024];
    int bytes_read;

    while (!done)
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
        timespec timeout;
        timeout.tv_sec = 1;
        timeout.tv_nsec = 0;

        // Ждём события в одном из сокетов

        sigset_t sigmask, empty_mask;
        struct sigaction sa;
        sigemptyset(&sigmask);
        sigaddset(&sigmask, SIGINT);
        if (sigprocmask(SIG_BLOCK, &sigmask, NULL) == -1)
        {
            perror("sigprocmask");
            exit(EXIT_FAILURE);
        }
        sa.sa_flags = 0;
        sa.sa_handler = signalHandler;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGINT, &sa, NULL) == -1)
        {
            perror("sigaction");
            exit(EXIT_FAILURE);
        }
        sigemptyset(&empty_mask);

        int mx = std::max(listener, *std::max_element(clients.begin(), clients.end()));
        int r = pselect(mx + 1, &readset, NULL, NULL, &timeout, &empty_mask);
        if (r == 0)
        {
            continue;
        }
        if (r < 0)
        {
            break;
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
                std::unique_lock<std::mutex> read_lock(mutex_Buf_Read);
                g_BufRead.push(Message(client, buf));
                read_notified = true;
                read_cond_var.notify_one();
            }
        }
    }

    read_notified = true;
    read_cond_var.notify_one();

    write_notified = true;
    write_cond_var.notify_one();
}
void out_data()
{

    Message result;
    while (!done)
    {
        std::unique_lock<std::mutex> write_lock(mutex_Buf_Write);
        while (!write_notified)
        {
            write_cond_var.wait(write_lock);
        }
        while (!g_BufWrite.empty())
        {
            result = g_BufWrite.front();
            send(result.client, result.data.c_str(), 1024, 0);
            g_BufWrite.pop();
        }
        write_notified = false;
    }
}
void run_command()
{
    while (!done)
    {
        std::unique_lock<std::mutex> read_lock(mutex_Buf_Read);
        while (!read_notified)
        {
            read_cond_var.wait(read_lock);
        }
        while (!g_BufRead.empty())
        {
            Message result;
            std::string command;
            int client = 0;
            result = g_BufRead.front();
            client = result.client;
            command = result.data;
            g_BufRead.pop();
            std::cout << "Run command: " << command << std::endl;
            std::string result_command = exec(command.c_str());
            std::unique_lock<std::mutex> write_lock(mutex_Buf_Write);
            g_BufWrite.push(Message(client, result_command));
            write_notified = true;
            write_cond_var.notify_one();
        }
        read_notified = false;
    }
}

int main()
{
    std::signal(SIGINT, signalHandler);

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

    std::thread thread_in_data(in_data, listener);
    std::thread thread_run_command(run_command);
    std::thread thread_out_data(out_data);

    if (thread_in_data.joinable())
        thread_in_data.join();
    if (thread_run_command.joinable())
        thread_run_command.join();
    if (thread_out_data.joinable())
        thread_out_data.join();

    for (int client : clients)
    {
        close(client);
    }
    close(listener); //закрываем сокет сервера
    std::cout << "close server" << std::endl;
    return 0;
}