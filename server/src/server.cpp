#include "server.hpp"

std::atomic<bool> ChatServer::is_running(true);

ChatServer::ChatServer(const std::string& config_path) : server_fd(-1), db(nullptr) {
  // загружаем конфиг
  if (!load_config(config_path)) {
    // если файла нет или с ним чтото не так берем дефолты из common.hpp
    this->port = PORT;
    this->db_path = DB_PATH;
    this->log_level = LOG_LEVEL;

    // пока логгер не готов пишем в обычный консольный поток
    logger->warn("Используются настройки по умолчанию из common.hpp");
  }

  // инициализируем систему логирования
  init_logger();

  logger->info("Экземпляр ChatServer создан. Порт: {}, БД: {}", this->port, this->db_path);
}

ChatServer::~ChatServer() {
  if (server_fd != -1) {
    close(server_fd);
    logger->info("Сокет сервера (FD: {}) закрыт.", server_fd);
  }
  if (db) {
    sqlite3_close(db);
    logger->info("Соединение с БД закрыто.");
  }
}

bool ChatServer::load_config(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    return false;
  }

  try {
    json data = json::parse(f);

    // прямое обращение через .at() или [] выбросит исключение, если ключа нет
    this->port = data.at("port").get<int>();
    this->db_path = data.at("db_path").get<std::string>();
    this->log_level = get_log_level(data.at("log_level").get<std::string>());

    return true;
  } catch (const std::exception& e) {
    std::cerr << "[CONFIG ERROR] Ошибка в файле конфигурации: " << e.what() << std::endl;
    return false;
  }
}

void ChatServer::signal_handler(int signal) {
  if (signal == SIGINT) {
    // т.к. логер статический метод, то cout,но можно сделать его глобальным
    std::cout << "\n[SIGNAL] Завершение работы сервера..." << std::endl;
    ChatServer::is_running = false;
  }
}

spdlog::level::level_enum ChatServer::get_log_level(const std::string& level_str) {
  if (level_str == "debug") return spdlog::level::debug;
  if (level_str == "info") return spdlog::level::info;
  if (level_str == "warn") return spdlog::level::warn;
  if (level_str == "error") return spdlog::level::err;
  if (level_str == "off") return spdlog::level::off;
  return spdlog::level::info;
}

bool ChatServer::start() {
  if (!init_db()) {
    logger->error("БД не готова, запуск отменен.");
    return false;
  }
  // создаем сокет(IPv4, TCP, выбор протокола по умолчанию (TCP)) и присваиваем его файловому дескриптору
  server_fd = socket(AF_INET, SOCK_STREAM, 0);

  if (server_fd == -1) {
    logger->error("Не удалось создать сокет: {}", strerror(errno));
    return false;
  }

  // защита от ошибки "Address already in use"
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // настройка адреса (AF_INET - IPv4; INADDR_ANY - константа 0.0.0.0 означающая, что слушай все входящие пакеты;
  // htons(port) - Host-To_Network-Short функция, переворачивающая порядок байтов процессора для сети, если это нужно)
  sockaddr_in address;
  // зануляем всё, включая 8 байт у поля sin_zero
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  // закрепляем за дескриптором (server_fd) этот адрес и порт
  if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
    logger->error("Не удалось привязать сокет к порту {}: {}", port, strerror(errno));
    return false;
  }

  // ставим сокет в состояние "прослушивание", то есть принимает входящие
  if (listen(server_fd, LISTEN_BACKLOG) < 0) {
    logger->error("Ошибка при переходе в режим listen: {}", strerror(errno));
    return false;
  }

  logger->info("Сервер успешно запущен. Слушаем порт {}...", port);
  return true;
}

void ChatServer::run() {
  while (ChatServer::is_running) {
    sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    logger->info("Основной цикл сервера run() запущен. Ожидание подключений...");

    // accept() — блокирующий вызов. Программа замирает здесь,
    // пока кто-то не постучится в порт
    int new_client_fd = accept(server_fd, (sockaddr*)&client_addr, &addrlen);

    if (new_client_fd < 0) {
      // если выключили сервер
      if (!ChatServer::is_running) {
        break;
      }

      // strerror(errno) если accept вернет ошибку, logger запишет не просто «ошибка», а
      // конкретную причину от ОС (например, «Too many open files»).
      logger->error("Ошибка accept: {}", strerror(errno));
      continue;
    }

    // логируем IP-адрес нового клиента (превращаем бинарный IP в строку)
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    logger->info("Новое подключение: {} (FD: {})", client_ip, new_client_fd);

    // сначала добавляем дескриптор в общий список (под защитой мьютекса)
    {
      std::lock_guard<std::mutex> lock(clients_mtx);
      clients.push_back(new_client_fd);
    }

    // создаем отдельный поток для обслуживания этого клиента.
    // передаем: адрес метода, указатель на текущий объект (this) и номер сокета.
    // .detach() позволяет потоку работать независимо от основного цикла, то есть цикл продолжает работу, а поток ушел в
    // поток
    std::thread(&ChatServer::handle_client, this, new_client_fd).detach();
  }
  // когда цикл завершился
  logger->warn("Формируем системный JSON-пакет отключения сервера");
  json shutdown_msg;
  shutdown_msg["type"] = "SYSTEM";
  shutdown_msg["sender"] = "SERVER";
  shutdown_msg["text"] = "Внимание: Сервер завершает работу. Соединение будет закрыто.";
  shutdown_msg["timestamp"] = std::time(nullptr);
  broadcast_message(shutdown_msg.dump(), -1);
  // пауза нужна для того, чтобы сервер обработать данные на сетевой карте,
  // затем отправить их, а уже после сработает деструктор close()
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  logger->info("Оповещение разослано. Переход к очистке ресурсов.");
}

void ChatServer::broadcast_message(const std::string& message, int sender_fd) {
  // закрываем "замок" перед тем, как читать список клиентов
  std::lock_guard<std::mutex> lock(clients_mtx);

  // _fd - это файловый дескриптор, другими словами это номер инструмента внутри процесса в линуксе
  // Проходимся по всему списку дескрипторов (номеров инструментов)
  for (int client_fd : clients) {
    if (client_fd != sender_fd) {
      // Параметры: (куда отправить, что отправить, сколько байт, флаги)
      send(client_fd, message.c_str(), message.size(), 0);
    }
  }
}  // замок открывается

// создаем файл бд и таблицу
bool ChatServer::init_db() {
  // открываем базу из конфига
  if (sqlite3_open(this->db_path.c_str(), &db) != SQLITE_OK) {
    logger->error("Не удалось открыть SQLite БД: {}", sqlite3_errmsg(db));
    return false;
  }

  // SQL запрос на создание таблицы
  const char* sql =
      "CREATE TABLE IF NOT EXISTS messages ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "sender TEXT NOT NULL,"
      "message TEXT NOT NULL,"
      "timestamp INTEGER NOT NULL);";

  char* err_msg = nullptr;
  if (sqlite3_exec(db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
    logger->error("Ошибка создания таблицы БД: {}", err_msg);
    sqlite3_free(err_msg);
    return false;
  }

  logger->info("База данных успешно инициализирована: {}", this->db_path);
  return true;
}

// для защиты от скл инъекций используем стандарт Prepared Statements
void ChatServer::save_message_to_db(const std::string& sender, const std::string& text, int64_t timestamp) {
  const char* sql = "INSERT INTO messages (sender, message, timestamp) VALUES (?, ?, ?);";
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    logger->error("Ошибка подготовки SQL: {}", sqlite3_errmsg(db));
    return;
  }

  // привязываем данные к знакам вопроса (безопасно)
  sqlite3_bind_text(stmt, 1, sender.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, text.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 3, timestamp);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    logger->error("Ошибка записи сообщения в БД: {}", sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
}

std::vector<json> ChatServer::get_last_messages(int count) {
  std::vector<json> history;
  // Берем последние N сообщений, сортируем по ID или времени
  const char* sql =
      "SELECT sender, message, timestamp FROM messages "
      "ORDER BY id DESC LIMIT ?;";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    logger->error("Ошибка подготовки истории: {}", sqlite3_errmsg(db));
    return history;
  }

  sqlite3_bind_int(stmt, 1, count);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    json msg;
    msg["type"] = "CHAT";  // Клиент подумает, что это обычное сообщение
    msg["sender"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    msg["text"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    msg["timestamp"] = sqlite3_column_int64(stmt, 2);
    history.push_back(msg);
  }

  sqlite3_finalize(stmt);

  // Так как мы брали DESC (с конца), история перевернута.
  // Разворачиваем обратно, чтобы старые были сверху, новые снизу.
  std::reverse(history.begin(), history.end());
  return history;
}

// инициализация логера
void ChatServer::init_logger() {
  try {
    // cоздаем стоки (куда писать)
    // _mt означает multi-threaded (потокобезопасно)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("server.log", false);  // арг. false означает дописывать

    // объединяем их в один объект логгера
    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    logger = std::make_shared<spdlog::logger>("ChatServer", sinks.begin(), sinks.end());

    // настраиваем формат: [Время] [Уровень] Сообщение
    // %^...%$ раскрашивает уровень лога в консоли (красный для ошибок итд)
    logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");

    // устанавливаем уровень фильтрации (пока пишем всё от INFO и выше)
    logger->set_level(this->log_level);

    logger->info("Система логирования инициализирована.");
  } catch (const spdlog::spdlog_ex& ex) {
    std::cerr << "Критическая ошибка инициализации логов: " << ex.what() << std::endl;
  }
}

std::string ChatServer::sanitize_text(std::string text) {
  std::string sanitized;
  for (char c : text) {
    if (c == 27) {
      sanitized += "^[";
    } else {
      sanitized += c;
    }
  }
  return sanitized;
}

void ChatServer::handle_client(int client_fd) {
  char buffer[BUFFER_SIZE];
  std::string user_name = "Unknown";

  // recv - поток засыпает до тех пор, пока клиент не пришлет данные и возвращает кол-во байт, которое заняло
  // сообщение
  int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
  /// если клиент отключился сразу, закрываем.

  if (bytes_received > 0) {
    try {
      auto msg_json = json::parse(std::string(buffer, bytes_received));
      if (!msg_json.contains("type") || !msg_json.contains("type") || msg_json["type"].get<std::string>() != "SYSTEM") {
        throw std::runtime_error("Expected SYSTEM message");
      }
      if (!msg_json.contains("sender") || msg_json["sender"].get<std::string>().empty()) {
        throw std::runtime_error("Empty or invalid SYSTEM sender");
      }
      if (!msg_json.contains("timestamp") || !msg_json["timestamp"].is_number()) {
        throw std::runtime_error("Empty or invalid SYSTEM time when send");
      }
      msg_json["sender"] = sanitize_text(msg_json["sender"].get<std::string>());
      user_name = msg_json["sender"].get<std::string>();
      msg_json["text"] = " присоединяется к чату.";

      // логируем вход
      logger->info("Пользователь {} подключился (FD: {})", user_name, client_fd);

      // также выводим имя всем остальным пользователям
      broadcast_message(msg_json.dump(), client_fd);

      // 1. Получаем историю из БД
      auto history = get_last_messages(10);

      // 2. Отправляем лично этому клиенту (не всем!)
      if (!history.empty()) {
        logger->info("Отправка истории ({} сообщ.) для FD: {}", history.size(), client_fd);
        for (const auto& msg : history) {
          std::string history_packet = msg.dump();
          send(client_fd, history_packet.c_str(), history_packet.size(), 0);
          // Маленькая пауза, чтобы пакеты не "склеились" в сокете (TCP нюанс)
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
      }

      // бесконечный цикл ожидания сообщения от клиента до тех пор пока он не выйдет
      while (ChatServer::is_running) {
        // очищаем старый буффер
        std::memset(buffer, 0, BUFFER_SIZE);

        // если сервер выключается (флаг стал false), то выходим немедленно
        if (!ChatServer::is_running) break;

        // засыпаем в ожидании сообщения от клиента
        bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
          json msg_leave_json;
          msg_leave_json["type"] = "SYSTEM";
          msg_leave_json["sender"] = user_name;
          msg_leave_json["text"] = " покинул чат.";
          msg_leave_json["timestamp"] = std::time(nullptr);
          broadcast_message(msg_leave_json.dump(), client_fd);
          logger->info("Пользователь {} покинул чат.", user_name);
          break;
        }

        msg_json = json::parse(std::string(buffer, bytes_received));

        // проверяем, не прислал ли клиент "прощальный пакет"
        if (msg_json.contains("type") && msg_json["type"].get<std::string>() == "EXIT") {
          logger->info("Пользователь {} инициировал выход (EXIT).", user_name);

          json leave_msg;
          leave_msg["type"] = "SYSTEM";
          leave_msg["sender"] = "SERVER";
          leave_msg["text"] = user_name + " покинул чат.";
          leave_msg["timestamp"] = std::time(nullptr);

          broadcast_message(leave_msg.dump(), client_fd);

          break;
        }

        if (!msg_json.contains("type") || !msg_json.contains("type") || msg_json["type"].get<std::string>() != "CHAT") {
          throw std::runtime_error("Expected CHAT message");
        }
        if (!msg_json.contains("sender") || msg_json["sender"].get<std::string>().empty()) {
          throw std::runtime_error("Empty or invalid CHAT sender");
        }
        if (!msg_json.contains("text") || msg_json["text"].get<std::string>().empty()) {
          throw std::runtime_error("Empty or invalid text CHAT message");
        }
        if (!msg_json.contains("timestamp") || !msg_json["timestamp"].is_number()) {
          throw std::runtime_error("Empty or invalid CHAT time when send");
        }

        msg_json["text"] = sanitize_text(msg_json["text"].get<std::string>());
        save_message_to_db(user_name, msg_json["text"], msg_json["timestamp"]);

        broadcast_message(msg_json.dump(), client_fd);
        logger->info("[{}] {}: {}", client_fd, user_name, msg_json["text"].get<std::string>());
      }
    } catch (const json::parse_error& e) {
      logger->error("Ошибка парсинга JSON от FD {}: {}", client_fd, e.what());
    } catch (const std::exception& e) {
      logger->error("Ошибка протокола для {}: {}", user_name, e.what());
    }
  } else {
    logger->warn("Клиент отключился на этапе регистрации (FD: {})", client_fd);
  }
  // УБОРКА ЗА СОБОЙ
  // когда цикл прервался (клиент вышел), нам нужно почистить список.
  {
    // обязательно закрываем "замок", так как меняем общий вектор.
    std::lock_guard<std::mutex> lock(clients_mtx);

    // находим и удаляем дескриптор (номер инструмента) этого клиента.
    clients.erase(std::remove(clients.begin(), clients.end(), client_fd), clients.end());
  }

  // важнейший шаг: возвращаем "жетон" (дескриптор) операционной системе.
  close(client_fd);
}
