#pragma once
#include "common.hpp"

class ChatClient {
 public:
  // передаем IP сервера и порт
  ChatClient(const std::string& config_path);

  // закрываем сокет клиента
  ~ChatClient();

  // запрещаем копирование, копирование присваиванием, перемещение и перемещение присваиванием
  ChatClient(const ChatClient&) = delete;
  ChatClient& operator=(const ChatClient&) = delete;
  ChatClient(ChatClient&&) = delete;
  ChatClient& operator=(ChatClient&&) = delete;

  // аналогично серверу, создаем статический флаг для того, чтобы можно было корректно из любого места завершить работу
  static std::atomic<bool> is_running;     // атомарный флаг гарантирует корректную работу в многопоточной среде
  static void signal_handler(int signal);  // метод для обработки сигналов

  // из строки получаем уровень логирования в перечеслении
  spdlog::level::level_enum get_log_level(const std::string& level_str);

  // подгружаем данные из конфига
  bool load_config(const std::string& path);

  // метод для подключения (socket + connect)
  bool connect_to_server();

  // основной цикл: ввод имени и отправка сообщений
  void run();

  // метод исключающий прием/отправку esc последовательностей
  std::string sanitize_text(std::string text);

 private:
  // поле для логгера
  std::shared_ptr<spdlog::logger> logger;

  void init_logger();  // метод для настройки логов

  // метод для фонового чтения сообщений от сервера (поток)
  void receive_messages();

  std::string get_and_format_time();
  std::string get_and_format_time(std::time_t);

  int client_fd;                        // дескриптор сокета клиента
  int port;                             // порт сервера
  std::string server_ip;                // IP сервера
  std::string user_name;                // имя пользователя
  spdlog::level::level_enum log_level;  // уровень логирования
};
