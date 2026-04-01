#pragma once
#include "common.hpp"

class ChatServer {
 public:
  // инициализируем порт и ставим дескриптор в -1 (пока не открыт)
  ChatServer(const std::string& config_path);

  // гарантирует закрытие главного сокета при выключении программы
  ~ChatServer();

  // запрещаем копирование, копирование присваиванием, перемещение и перемещение присваиванием
  ChatServer(const ChatServer&) = delete;
  ChatServer& operator=(const ChatServer&) = delete;
  ChatServer(ChatServer&&) = delete;
  ChatServer& operator=(ChatServer&&) = delete;

  //  метод загрузки конфигов из файла
  bool load_config(const std::string& path);

  // метод для обработки сигналов
  static void signal_handler(int signal);

  // из строки получаем уровень логирования в перечеслении
  spdlog::level::level_enum get_log_level(const std::string& level_str);

  // атомарный флаг гарантирует корректную работу в многопоточной среде
  static std::atomic<bool> is_running;

  // метод для настройки сокета (socket, bind, listen)
  bool start();

  // основной цикл приема новых соединений (accept)
  void run();

 private:
  // поле для логгера
  std::shared_ptr<spdlog::logger> logger;

  void init_logger();  // метод для настройки логов

  // метод исключающий прием/отправку esc последовательностей
  std::string sanitize_text(std::string text);

  // метод для работы с конкретным клиентом в отдельном потоке,
  // мы передаем в него дескриптор, который получили от accept (run)
  void handle_client(int client_fd);

  // метод для рассылки сообщений (в данном случае всем, кроме самого себя)
  void broadcast_message(const std::string& message, int sender_fd);

  // метод создает файл базы если его нет и таблицу
  bool init_db();

  // безопасное сохранение
  void save_message_to_db(const std::string& sender, const std::string& text, int64_t timestamp);

  // вывод последних ... сообщений
  std::vector<json> get_last_messages(const int count);

  int server_fd;                        // дескриптор "слушающего" сокета
  int port;                             // номер порта
  sqlite3* db = nullptr;                // указатель на объект базы данных SQLite
  std::vector<int> clients;             // вектор "жетонов" всех активных клиентов
  std::mutex clients_mtx;               // замок для защиты вектора
  std::string db_path;                  // путь до базы данных
  spdlog::level::level_enum log_level;  // уровень логирования
};
