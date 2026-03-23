#include "server.hpp"

ChatServer::ChatServer(const std::string& config_path) : server_fd(-1), db(nullptr) {
  // инициализируем систему логирования
  init_logger();
  // загружаем конфиг
  if (!load_config(config_path)) {
    // если файла нет или с ним чтото не так берем дефолты из common.hpp
    this->port = PORT;
    this->db_path = DB_PATH;
    this->log_level = LOG_LEVEL;

    // пока логгер не готов пишем в обычный консольный поток
    logger->warn("Используются настройки по умолчанию из common.hpp");
  }

  logger->info("Экземпляр ChatServer создан. Порт: {}, БД: {}", this->port, this->db_path);
}

ChatServer::~ChatServer() {
  if (server_fd != -1) {
    close(server_fd);
    logger->info("Сокет сервера (FD: {}) закрыт.", server_fd);
  }
}

std::atomic<bool> ChatServer::is_running(true);

bool ChatServer::load_config(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    // Файла нет — сигнализируем об этом, чтобы конструктор взял дефолты
    return false;
  }

  try {
    json data = json::parse(f);

    // Прямое обращение через .at() или [] выбросит исключение, если ключа нет
    // Это гарантирует, что если конфиг ЕСТЬ, то он должен быть ПОЛНЫМ
    this->port = data.at("port").get<int>();
    this->db_path = data.at("db_path").get<std::string>();
    this->log_level = data.at("log_level").get<std::string>();

    return true;
  } catch (const std::exception& e) {
    // Сюда попадем и при ошибке парсинга, и при отсутствии ключа
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

bool ChatServer::start() {
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

// инициализация логера
void ChatServer::init_logger() {
  try {
    // cоздаем стоки (куда писать)
    // _mt означает multi-threaded (потокобезопасно)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink =
        std::make_shared<spdlog::sinks::basic_file_sink_mt>("server.log", false);  // арг. false означает дописывать

    // объединяем их в один объект логгера
    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    logger = std::make_shared<spdlog::logger>("ChatServer", sinks.begin(), sinks.end());

    // настраиваем формат: [Время] [Уровень] Сообщение
    // %^...%$ раскрашивает уровень лога в консоли (красный для ошибок итд)
    logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");

    // устанавливаем уровень фильтрации (пока пишем всё от INFO и выше)
    logger->set_level(spdlog::level::info);

    logger->info("Система логирования инициализирована.");
  } catch (const spdlog::spdlog_ex& ex) {
    std::cerr << "Критическая ошибка инициализации логов: " << ex.what() << std::endl;
  }
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
      if (!msg_json.contains("type") || !msg_json.contains("type") || msg_json["type"].get<std::string>() != "AUTH") {
        throw std::runtime_error("Expected AUTH message");
      }
      if (!msg_json.contains("sender") || msg_json["sender"].get<std::string>().empty()) {
        throw std::runtime_error("Empty or invalid AUTH sender");
      }
      if (!msg_json.contains("timestamp") || !msg_json["timestamp"].is_number()) {
        throw std::runtime_error("Empty or invalid AUTH time when send");
      }
      user_name = msg_json["sender"].get<std::string>();
      msg_json["text"] = " присоединяется к чату.";

      // Логируем вход
      logger->info("Пользователь {} подключился (FD: {})", user_name, client_fd);

      // также выводим имя всем остальным пользователям
      broadcast_message(msg_json.dump(), client_fd);

      // бесконечный цикл ожидания сообщения от клиента до тех пор пока он не выйдет
      while (true) {
        // очищаем старый буффер
        memset(buffer, 0, BUFFER_SIZE);

        // засыпаем в ожидании сообщения от клиента
        bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
          json msg_leave_json;
          msg_leave_json["type"] = "AUTH";
          msg_leave_json["sender"] = user_name;
          msg_leave_json["text"] = " покинул чат.";
          msg_leave_json["timestamp"] = std::time(nullptr);
          broadcast_message(msg_leave_json.dump(), client_fd);
          logger->info("Пользователь {} покинул чат.", user_name);
          break;
        }

        msg_json = json::parse(std::string(buffer, bytes_received));

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
        broadcast_message(msg_json.dump(), client_fd);

        // Логируем текст сообщения
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
