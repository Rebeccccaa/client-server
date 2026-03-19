#include <memory>

#include "server.hpp"

int main() {
  try {
    // если внутри try случится ошибка, деструктор ChatServer
    // вызовется автоматически и корректно закроет сокет.
    auto server = std::make_unique<ChatServer>(PORT);

    // настройка сетевых ресурсов
    if (server->start()) {
      // запуск бесконечного цикла
      server->run();

    } else {
      std::cerr << "[CRITICAL] Ошибка при старте сервера на порту " << PORT << std::endl;
      return 1;
    }
  } catch (const std::exception& e) {
    std::cerr << "[FATAL ERROR] Непредвиденная ошибка сервера: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    // ловим, что не является std::exception
    std::cerr << "[UNKNOWN ERROR] Произошла неизвестная критическая ошибка!" << std::endl;
    return 1;
  }

  return 0;
}
