#include "client.hpp"

#include <netdb.h>  // для докера

// инициализация статического флага
std::atomic<bool> ChatClient::is_running(true);

ChatClient::ChatClient(const std::string& config_path) : client_fd(-1) {
  // загружаем конфиг
  if (!load_config(config_path)) {
    // если файла нет или с ним чтото не так берем дефолты из common.hpp
    this->server_ip = IP_LOCALHOST;
    this->port = PORT;
    this->log_level = LOG_LEVEL;

    // пока логгер не готов пишем в обычный консольный поток
    logger->warn("Используются настройки по умолчанию из common.hpp");
  }
  // настраиваем логгер
  init_logger();

  logger->info("Экземпляр ChatClient создан. Целевой сервер: {}:{}", server_ip, port);
}

ChatClient::~ChatClient() {
  if (client_fd != -1) {
    close(client_fd);
    client_fd = -1;
  }
}

// метод обработки сигналов
void ChatClient::signal_handler(int signal) {
  if (signal == SIGINT) {
    // не используем здесь логгер, так как это статический метод
    // (он не видит поле 'logger' конкретного объекта)
    std::cout << "\n[SIGNAL] Получен сигнал прерывания (Ctrl+C). Выходим..." << std::endl;
    ChatClient::is_running = false;
  }
}

spdlog::level::level_enum ChatClient::get_log_level(const std::string& level_str) {
  if (level_str == "debug") return spdlog::level::debug;
  if (level_str == "info") return spdlog::level::info;
  if (level_str == "warn") return spdlog::level::warn;
  if (level_str == "error") return spdlog::level::err;
  if (level_str == "off") return spdlog::level::off;
  return spdlog::level::info;
}

bool ChatClient::load_config(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open()) return false;

  try {
    json data = json::parse(f);
    this->server_ip = data.at("server_ip").get<std::string>();
    this->port = data.at("port").get<int>();
    this->log_level = get_log_level(data.at("log_level").get<std::string>());

    return true;
  } catch (const std::exception& e) {
    std::cerr << "[CONFIG ERROR] " << e.what() << std::endl;
    return false;
  }
}
/*
// НАШ РЕАЛЬНЫЙ МЕТОД
// метод для подключения (socket + connect)
bool ChatClient::connect_to_server() {
  // создаем сокет (IPv4, TCP)
  client_fd = socket(AF_INET, SOCK_STREAM, 0);

  if (client_fd == -1) {
    logger->error("Не удалось создать сокет клиента: {}", strerror(errno));
    return false;
  }

  // настраиваем структуру сервера к которому подключаемся
  sockaddr_in server_addr;
  std::memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  // преобразуем IP из текста в бинарный вид (Presentation to Network)
  if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
    logger->error("Некорректный IP-адрес сервера: {}", server_ip);
    return false;
  }

  logger->info("Попытка подключения к серверу {}:{}...", server_ip, port);

  // устанавливаем соединение (connect)
  // программа замирает здесь, пока сервер не сделает accept() или не выйдет таймаут
  if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    // eсли сервер выключен, здесь будет "Connection refused"
    logger->error("Ошибка подключения: {}", strerror(errno));
    return false;
  }

  logger->info("Успешно подключено к серверу (FD: {})", client_fd);
  return true;
}
*/

bool ChatClient::connect_to_server() {
  client_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (client_fd == -1) {
    logger->error("Не удалось создать сокет: {}", strerror(errno));
    return false;
  }

  // --- НОВЫЙ БЛОК РАЗРЕШЕНИЯ ИМЕНИ (DNS/Docker) ---
  struct addrinfo hints, *res;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;        // Только IPv4
  hints.ai_socktype = SOCK_STREAM;  // TCP

  // getaddrinfo сам поймет, IP это ("127.0.0.1") или имя ("chat_server")
  int status = getaddrinfo(server_ip.c_str(), std::to_string(port).c_str(), &hints, &res);
  if (status != 0) {
    logger->error("Ошибка разрешения адреса {}: {}", server_ip, gai_strerror(status));
    return false;
  }

  // Пытаемся подключиться по найденному адресу
  if (connect(client_fd, res->ai_addr, res->ai_addrlen) < 0) {
    logger->error("Ошибка подключения к {}:{}: {}", server_ip, port, strerror(errno));
    freeaddrinfo(res);  // Освобождаем память!
    return false;
  }

  freeaddrinfo(res);  // Освобождаем память при успешном подключении
  logger->info("Успешно подключено к серверу!");
  return true;
}

// тут мы регистрируем имя и заходим в беск. цикл, где ожидаем ввода команд от клиента
void ChatClient::run() {
  logger->info("Запуск основного цикла взаимодействия run(). Ожидание ввода имени...");

  // регистрируем имя
  std::cout << "Введите имя пользователя: ";
  while (user_name.empty() && ChatClient::is_running) {
    if (!std::getline(std::cin, user_name)) break;
  }

  // если во время ввода имени нажали ctrl+c выходим сразу
  if (!ChatClient::is_running) return;

  // SYSTEM пакет
  json system;
  system["type"] = "SYSTEM";
  system["sender"] = user_name;
  system["timestamp"] = std::time(nullptr);

  std::string out_system = system.dump();
  if (send(client_fd, out_system.c_str(), out_system.size(), 0) < 0) {
    logger->error("Не удалось отправить SYSTEM пакет: {}", strerror(errno));
    return;
  }
  user_name = sanitize_text(user_name);
  logger->info("Пакет SYSTEM успешно отправлен. Пользователь: {}", user_name);

  // запускаем фоновый поток для приема сообщений
  std::thread receiver_thread(&ChatClient::receive_messages, this);
  // делаем поток независимым
  receiver_thread.detach();

  // основной цикл отправки сообщений
  std::string text;
  while (ChatClient::is_running) {
    std::cout << "> " << std::flush;
    if (!std::getline(std::cin, text)) break;  // если поток ввода закрыт ctrl+d
    if (text.empty() || text == "\n") continue;
    if (text == COMMAND_EXIT) break;  // команда для выхода
    // CHAT пакет
    json chat;
    chat["type"] = "CHAT";
    chat["sender"] = user_name;
    chat["text"] = text;
    chat["timestamp"] = std::time(nullptr);
    // отправляем текст на сервер (а сервер в свою очередь всем другим клиентам)
    std::string out_chat = chat.dump();
    if (send(client_fd, out_chat.c_str(), out_chat.size(), 0) < 0) {
      logger->error("Ошибка при отправке сообщения: {}", strerror(errno));
      break;
    }
  }
  // отправляем прощальный пакет
  if (client_fd != -1) {
    logger->warn("Выхода из чата...");

    json exit_msg;
    exit_msg["type"] = "EXIT";  // специальный тип для вежливого отключения
    exit_msg["sender"] = user_name;
    exit_msg["timestamp"] = std::time(nullptr);

    std::string out = exit_msg.dump();
    send(client_fd, out.c_str(), out.size(), 0);

    // даем 100мс, чтобы пакет успел уйти в сеть перед вызовом close() в деструкторе
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logger->info("Прощальный пакет EXIT отправлен.");
  }
}

std::string ChatClient::sanitize_text(std::string text) {
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

void ChatClient::init_logger() {
  try {
    // консольный сток (цветной)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    // файловый сток
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("client.log", false);

    // объединяем их
    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    logger = std::make_shared<spdlog::logger>("ChatClient", sinks.begin(), sinks.end());

    // настраиваем формат времени и уровень логирования
    logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
    logger->set_level(this->log_level);

    logger->info("Логгер клиента успешно инициализирован.");
  } catch (const spdlog::spdlog_ex& ex) {
    std::cerr << "Критическая ошибка логгера клиента: " << ex.what() << std::endl;
  }
}

// фоновое прослушивание (метод вывода у клиента)
void ChatClient::receive_messages() {
  char buffer[BUFFER_SIZE];
  while (ChatClient::is_running) {
    std::memset(buffer, 0, BUFFER_SIZE);
    int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);

    // если случился сигнал ctrl+c, то recv вернет -1
    if (!ChatClient::is_running) break;

    if (bytes_received <= 0) {
      // форматируем время
      logger->warn("Потеряно соединение с сервером.");
      ChatClient::is_running = false;  // Сигнализируем основному потоку о выходе
      break;
    }

    try {
      // распаковываем JSON
      auto json_msg = json::parse(std::string(buffer, bytes_received));

      // достаем и форматируем время
      std::string time_str = get_and_format_time(json_msg.at("timestamp").get<std::time_t>());

      // тоже достаем из json
      std::string type = json_msg.at("type");
      std::string sender = sanitize_text(json_msg.at("sender"));
      std::string text = sanitize_text(json_msg.value("text", ""));  // если SYSTEM, текста может не быть

      // \033[2K — очистить всю строку, \r — прыгнуть в начало
      std::cout << "\033[2K\r" << std::flush;

      if (type == "SYSTEM") {
        logger->info("[SYSTEM] {}", text);
        std::cout << "[" << time_str << "] *** " << text << " ***" << std::endl;
      } else if (type == "SYSTEM") {
        std::cout << "[" << time_str << "] Пользователь " << sender << text << std::endl;
      } else {
        std::cout << "[" << time_str << "] " << sender << ": " << text << std::endl;
      }

      // возвращаем приглашение к вводу
      if (ChatClient::is_running) {
        std::cout << "> " << std::flush;
      }
    } catch (const std::exception& e) {
      // если пришел мусор, который не парсится
      logger->error("Ошибка парсинга входящего JSON: {}", e.what());
    }
  }
}

std::string ChatClient::get_and_format_time(std::time_t time_in_seconds) {
  std::time_t ts = time_in_seconds;
  std::tm* now = std::localtime(&ts);
  char time_str[10];
  std::strftime(time_str, sizeof(time_str), "%H:%M:%S", now);
  return std::string();
}