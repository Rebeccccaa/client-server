#pragma once
#include "common.hpp"

class ChatServer {
 public:
  // инициализируем порт и ставим дескриптор в -1 (пока не открыт)
  ChatServer(int port);

  // гарантирует закрытие главного сокета при выключении программы
  ~ChatServer();

  // метод для обработки сигналов
  static void signal_handler(int signal);

  // атомарный флаг гарантирует корректную работу в многопоточной среде
  static std::atomic<bool> is_running;

  // запрещаем копирование, копирование присваиванием, перемещение и перемещение присваиванием
  ChatServer(const ChatServer&) = delete;
  ChatServer& operator=(const ChatServer&) = delete;
  ChatServer(ChatServer&&) = delete;
  ChatServer& operator=(ChatServer&&) = delete;

  // метод для настройки сокета (socket, bind, listen)
  bool start();

  // основной цикл приема новых соединений (accept)
  void run();

 private:
  // метод для работы с конкретным клиентом в отдельном потоке,
  // мы передаем в него дескриптор, который получили от accept (run)
  void handle_client(int client_fd);

  // метод для рассылки сообщений (в данном случае всем, кроме самого себя)
  void broadcast_message(const std::string& message, int sender_fd);

  int server_fd;             // дескриптор "слушающего" сокета
  int port;                  // номер порта
  std::vector<int> clients;  // вектор "жетонов" всех активных клиентов
  std::mutex clients_mtx;    // замок для защиты вектора
};
