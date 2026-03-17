#include "server.hpp"

ChatServer::ChatServer(int port) : port(port), server_fd(-1) {}

ChatServer::~ChatServer() {
  if (server_fd != -1) {
    close(server_fd);
    std::cout << "[SERVER] Сокет закрыт, сервер остановлен." << std::endl;
  }
}

bool ChatServer::start() {
  // создаем сокет(IPv4, TCP, выбор протокола по умолчанию (TCP)) и присваиваем его файловому дескриптору
  server_fd = socket(AF_INET, SOCK_STREAM, 0);

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
    std::cerr << "[ERROR] Не удалось привязать сокет к порту " << port << std::endl;
    return false;
  }

  // ставим сокет в состояние "прослушивание", то есть принимает входящие
  if (listen(server_fd, LISTEN_BACKLOG) < 0) {
    std::cerr << "[ERROR] Ошибка при переходе в режим listen" << std::endl;
    return false;
  }

  std::cout << "[SERVER] Успешно запущен. Слушаем порт " << port << "..." << std::endl;
  return true;
}

void ChatServer::run() {
  while (true) {
    sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

    // accept() — блокирующий вызов. Программа замирает здесь,
    // пока кто-то не постучится в порт
    int new_client_fd = accept(server_fd, (sockaddr*)&client_addr, &addrlen);

    if (new_client_fd < 0) {
      std::cerr << "[ERROR] Не удалось принять соединение" << std::endl;
      continue;
    }

    // логируем IP-адрес нового клиента (превращаем бинарный IP в строку)
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    std::cout << "[SERVER] Новое подключение: " << client_ip << " (FD: " << new_client_fd << ")" << std::endl;

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

void ChatServer::handle_client(int client_fd) {
  char buffer[BUFFER_SIZE];

  // recv - поток засыпает до тех пор, пока клиент не пришлет данные и возвращает кол-во байт, которое заняло сообщение
  int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
  /// если клиент отключился сразу, закрываем.

  if (bytes_received > 0) {
    // берем из буфера наше сообщение (имя пользователя) и выводим его в отладку
    std::string user_name(buffer, bytes_received);
    std::cout << "[LOG] " << user_name << " подключился к серверу.\n";

    // также выводим имя всем остальным пользователям
    broadcast_message(user_name + " присоединяется к чату!\n", client_fd);

    // бесконечный цикл ожидания сообщения от клиента до тех пор пока он не выйдет
    while (true) {
      // очищаем старый буффер
      memset(buffer, 0, BUFFER_SIZE);

      // засыпаем в ожидании сообщения от клиента
      bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
      if (bytes_received <= 0) {
        std::cout << "[LOG] " << user_name << " покинул чат." << std::endl;
        break;
      }

      std::string msg = user_name + ": " + std::string(buffer, bytes_received);
      broadcast_message(msg, client_fd);
      std::cout << "[LOG] [" << client_fd << "] " << msg << '\n';
    }
  } else {
    std::cout << "[LOG] Клиент отключился на этапе регистрации (FD: " << client_fd << ")\n";
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
