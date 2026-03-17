#include "client.hpp"

ChatClient::ChatClient(const std::string& ip, int port) : server_ip(ip), port(port), client_fd(-1) {}

ChatClient::~ChatClient() {
  if (client_fd != -1) {
    close(client_fd);
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
  std::getline(std::cin, user_name);

  // отправляем имя первым пакетом (как договорились в протоколе сервера)
  send(client_fd, user_name.c_str(), user_name.size(), 0);

  // запускаем фоновый поток для приема сообщений
  std::thread receiver_thread(&ChatClient::receive_messages, this);
  // делаем поток независимым
  receiver_thread.detach();

  // основной цикл отправки сообщений
  std::string message;
  while (true) {
    std::cout << "> ";
    std::getline(std::cin, message);

    if (message.empty()) continue;
    if (message == COMMAND_EXIT) break;  // команда для выхода

    // отправляем текст на сервер (а сервер в свою очередь всем другим клиентам)
    send(client_fd, message.c_str(), message.size(), 0);
  }
}

// фоновое прослушивание (метод вывода у клиента)
void ChatClient::receive_messages() {
  char buffer[BUFFER_SIZE];
  while (true) {
    memset(buffer, 0, BUFFER_SIZE);

    // поток засыпает здесь, ожидая данные от сервера
    int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);

    if (bytes_received <= 0) {
      // если сервер закрыл соединение или произошла ошибка
      std::cout << "\n[СИСТЕМА] Соединение с сервером потеряно." << std::endl;
      // можно попробовать переподключиться в дальнейшем
      exit(0);
    }

    // выводим полученное сообщение на экран
    // \n нужен, чтобы сообщение не слиплось с приглашением к вводу "> "
    std::cout << "\r" << std::string(buffer, bytes_received) << std::endl;
    std::cout << "> " << std::flush;
  }
}
