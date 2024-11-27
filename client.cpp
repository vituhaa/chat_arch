#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <thread>

using namespace std;

void receive_messages(int client_socket);

int main()
{
    // создание сокета
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1)
    {
        cerr << "ERROR: cannot create socket" << endl;
        return 1;
    }

    // установка адреса и порта сервера
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);                    // порт 12345
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr); // адрес сервера (localhost)

    // установка соединения с сервером
    if (connect(client_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1)
    {
        cerr << "ERROR: cannot connect to server" << endl;
        close(client_socket);
        return 1;
    }

    cout << "Connected to server." << endl;

    string name;
    cout << "Please enter your name: ";
    getline(cin, name);
    send(client_socket, name.c_str(), name.length(), 0);
    cout << "Now your name is " << name << ". Enter message (type 'exit' to quit): " << endl;

    thread receiver_thread(receive_messages, client_socket);

    // бесконечный цикл для ввода сообщений
    string message;
    while (true)
    {
        getline(cin, message);

        // если пользователь вводит "exit", завершаем соединение
        if (message == "exit")
        {
            break;
        }

        // отправка сообщения серверу
        send(client_socket, message.c_str(), message.length(), 0);
    }

    receiver_thread.detach(); // Ожидание завершения потока

    // закрытие соединения
    close(client_socket);
    return 0;
}

void receive_messages(int client_socket)
{
    char buffer[1024];
    while (true)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0)
        {
            cerr << "Disconnected from server." << endl;
            close(client_socket);
            exit(0);
        }
        buffer[bytesReceived] = '\0';
        cout << buffer << endl;
    }
}
