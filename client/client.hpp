#pragma once
#include "common.hpp"

class ChatClient {
 public:
  // передаем IP сервера и порт
  ChatClient(const std::string& ip, int port);

  // закрываем сокет клиента
  ~ChatClient();

  // метод для подключения (socket + connect)
  bool connect_to_server();

  // основной цикл: ввод имени и отправка сообщений
  void run();

 private:
  // метод для фонового чтения сообщений от сервера (поток)
  void receive_messages();

  int client_fd;          // дескриптор сокета клиента
  int port;               // порт сервера
  std::string server_ip;  // IP сервера
  std::string user_name;  // имя пользователя
};
