#pragma once
#include "common.hpp"

class ChatClient {
 public:
  // передаем IP сервера и порт
  ChatClient(const std::string& ip, int port);

  // закрываем сокет клиента
  ~ChatClient();

  // запрещаем копирование, копирование присваиванием, перемещение и перемещение присваиванием
  ChatClient(const ChatClient&) = delete;
  ChatClient& operator=(const ChatClient&) = delete;
  ChatClient(ChatClient&&) = delete;
  ChatClient& operator=(ChatClient&&) = delete;

  // метод для подключения (socket + connect)
  bool connect_to_server();

  // основной цикл: ввод имени и отправка сообщений
  void run();

 private:
  // метод для фонового чтения сообщений от сервера (поток)
  void receive_messages();

  std::string get_and_format_time();
  std::string get_and_format_time(std::time_t);

  int client_fd;          // дескриптор сокета клиента
  int port;               // порт сервера
  std::string server_ip;  // IP сервера
  std::string user_name;  // имя пользователя
};
