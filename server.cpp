#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>
#include <fstream>  // Для работы с файлами

using namespace std;
vector<int> users;  // Вектор для хранения сокетов подключённых клиентов
mutex client_sockets_mutex;  // Мьютекс для защиты доступа к списку клиентов
mutex file_mutex;
const string history_filename = "chat_history.txt";  // файл для хранения истории

// Функция для записи сообщения в файл
void save_message_to_file(const string& message) {
    lock_guard<mutex> lock(file_mutex);
    ofstream history_file(history_filename, ios::app);  // открываем файл в ржиме добавления
    if (history_file.is_open()) {
        history_file << message << endl;  // записываем в файл
        history_file.close();  // закрываем файл
    }
}

// функция для отправки истории клиенту
void send_chat_history(int client_socket) {
    lock_guard<mutex> lock(file_mutex);
    ifstream history_file(history_filename);  // открыть файл для чтения
    string line;
    while (getline(history_file, line)) {  // читаем построчно
        send(client_socket, line.c_str(), line.length(), 0);  // отправляем каждую строчку клиенту
        usleep(1000);  // задержка чтобы буфер не переполнялся
    }
    history_file.close();  // закрываем файл
}

// Функция для обработки подключенного клиента
void handle_client(int client_socket) {
    {
        lock_guard<mutex> lock(client_sockets_mutex);
        users.push_back(client_socket);
    }

    char buffer[1024];
    string name;

    // Чтение имени пользователя
    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received > 0) {
        name = string(buffer, bytes_received);
    }

    // отправляем историю новому клиенту
    send_chat_history(client_socket);

    while (true) {
        memset(buffer, 0, sizeof(buffer));  // Очищаем буфер
        bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

        // Проверка на завершение соединения
        if (bytes_received <= 0) {
            cout << "Client disconnected." << endl;
            close(client_socket);
            lock_guard<mutex> lock(client_sockets_mutex);
            users.erase(remove(users.begin(), users.end(), client_socket), users.end());
            break;
        }

        buffer[bytes_received] = '\0';  // Завершаем строку символом '\0'
        string note = "[" + name + "] " + buffer;
        cout << note << endl;

        save_message_to_file(note);  // сохраняем сообщение в файл

        lock_guard<mutex> lock(client_sockets_mutex);
        for (int user_socket : users) {
            if (user_socket != client_socket)
                send(user_socket, note.c_str(), note.length(), 0);
        }
    }
}

int main() {
    // Создание сокета
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        cerr << "ERROR: cannot create socket" << endl;
        return 1;
    }

    // Установка опции SO_REUSEADDR
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "ERROR: setsockopt failed" << endl;
        close(server_socket);
        return 1;
    }

    // Настройка адреса и порта сервера
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);  // Порт 12345
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Привязка сокета к адресу и порту
    if (bind(server_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        cerr << "ERROR: cannot bind socket to address" << endl;
        close(server_socket);
        return 1;
    }
    // Ожидание входящих соединений
    if (listen(server_socket, 5) == -1) {
        cerr << "ERROR: cannot listen on socket" << endl;
        close(server_socket);
        return 1;
    }

    cout << "Server is listening on port 12345..." << endl;

    // Основной цикл сервера
    while (true) {
        // Принятие нового соединения от клиента
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket == -1) {
            cerr << "ERROR: cannot accept incoming connection" << endl;
            continue;  // Пропустить ошибку и продолжить слушать соединения
        }

        // Создание нового потока для обслуживания клиента
        thread client_thread(handle_client, client_socket);
        client_thread.detach();  // Позволяет потоку работать независимо
    }

    // Закрытие сокета сервера
    close(server_socket);
    return 0;
}
