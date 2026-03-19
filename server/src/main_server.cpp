#include "server.hpp"

int main() {
  // создаем экземпляр нашего сервера
  ChatServer server(PORT);

  // инициализируем сокеты (socket, bind, listen)
  if (server.start()) {
    // запускаем бесконечный цикл приема новых клиентов
    server.run();

  } else {
    std::cerr << "[CRITICAL] Не удалось запустить сервер. Проверьте порт!" << std::endl;
    return 1;
  }

  return 0;
}
