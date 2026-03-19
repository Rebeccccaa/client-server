#include "client.hpp"

ChatClient::ChatClient(const std::string& ip, int port) : server_ip(ip), port(port), client_fd(-1) {}

ChatClient::~ChatClient() {
  if (client_fd != -1) {
    close(client_fd);
    client_fd = -1;
  }
}

// метод для подключения (socket + connect)
bool ChatClient::connect_to_server() {
  // создаем сокет (IPv4, TCP)
  client_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (client_fd == -1) {
    std::cerr << "[ERROR] Не удалось создать сокет клиента" << std::endl;
    return false;
  }

  // настраиваем адрес сервера к которому подключаемся
  sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);

  // преобразуем IP из текста в бинарный вид (Presentation to Network)
  if (inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
    std::cerr << "[ERROR] Неверный IP-адрес сервера" << std::endl;
    return false;
  }

  // устанавливаем соединение (connect)
  // программа замирает здесь, пока сервер не сделает accept() или не выйдет таймаут
  if (connect(client_fd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    std::cerr << "[ERROR] Соединение отклонено сервером!" << std::endl;
    return false;
  }

  std::cout << "[CLIENT] Успешно подключено к " << server_ip << std::endl;
  return true;
}

// тут мы регистрируем имя и заходим в беск. цикл, где ожидаем ввода команд от клиента
void ChatClient::run() {
  // регистрируем имя
  std::cout << "Введите имя пользователя: ";
  while (user_name.empty()) std::getline(std::cin, user_name);
  // AUTH пакет
  json auth;
  auth["type"] = "AUTH";
  auth["sender"] = user_name;
  auth["timestamp"] = std::time(nullptr);

  std::string out_auth = auth.dump();
  send(client_fd, out_auth.c_str(), out_auth.size(), 0);

  // запускаем фоновый поток для приема сообщений
  std::thread receiver_thread(&ChatClient::receive_messages, this);
  // делаем поток независимым
  receiver_thread.detach();

  // основной цикл отправки сообщений
  std::string text;
  while (true) {
    std::cout << "> ";
    std::getline(std::cin, text);

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
    send(client_fd, out_chat.c_str(), out_chat.size(), 0);
  }
}

// фоновое прослушивание (метод вывода у клиента)
void ChatClient::receive_messages() {
  char buffer[BUFFER_SIZE];
  while (true) {
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);

    if (bytes_received <= 0) {
      // форматируем время
      std::string time_str = get_and_format_time();

      std::cout << "\n[" << time_str << "] [SERVER] Соединение разорвано." << std::endl;
      exit(0);
    }

    try {
      // распаковываем JSON
      auto json_msg = json::parse(std::string(buffer, bytes_received));

      // достаем и форматируем время
      std::string time_str = get_and_format_time(json_msg.at("timestamp").get<std::time_t>());

      // тоже достаем из json
      std::string type = json_msg.at("type");
      std::string sender = json_msg.at("sender");
      std::string text = json_msg.value("text", "");  // если SYSTEM, текста может не быть

      // возврат каретки \r, чтобы стереть текущее приглашение "> "
      std::cout << "\r";

      if (type == "SYSTEM") {
        std::cout << "[" << time_str << "] *** " << text << " ***" << std::endl;
      } else if (type == "AUTH") {
        std::cout << "[" << time_str << "] Пользователь " << sender << text << std::endl;
      } else {
        std::cout << "[" << time_str << "] " << sender << ": " << text << std::endl;
      }

      // возвращаем приглашение к вводу
      std::cout << "> " << std::flush;
    } catch (const std::exception& e) {
      // если пришел мусор, который не парсится
      std::cerr << get_and_format_time() << "\n[ОШИБКА] Некорректный формат данных от сервера" << std::endl;
    }
  }
}

std::string ChatClient::get_and_format_time() {
  std::time_t ts = time(nullptr);
  std::tm* now = std::localtime(&ts);
  char time_str[10];
  std::strftime(time_str, sizeof(time_str), "%H:%M:%S", now);
  return (std::string)time_str;
}

std::string ChatClient::get_and_format_time(std::time_t time_in_seconds) {
  std::time_t ts = time_in_seconds;
  std::tm* now = std::localtime(&ts);
  char time_str[10];
  std::strftime(time_str, sizeof(time_str), "%H:%M:%S", now);
  return std::string();
}
