#include "client.hpp"

int main() {
  // создаем объект клиента
  // ChatClient client("109.120.187.67", PORT);
  ChatClient client("127.0.0.1", PORT);
  // пытаемся постучаться к серверу
  if (client.connect_to_server()) {
    // запускаем основной цикл (ввод имени, запуск фонового потока и отправка сообщений)
    client.run();
  } else {
    std::cerr << "[CRITICAL] Не удалось подключиться к серверу. "
              << "Проверьте, запущен ли он!" << std::endl;
    return 1;
  }

  return 0;
}
