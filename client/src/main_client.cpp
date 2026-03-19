#include <memory>

#include "client.hpp"

int main() {
  try {
    // используем умный указатель вместо обычного объекта на стеке
    auto client = std::make_unique<ChatClient>("127.0.0.1", PORT);
    // auto client = std::make_unique<ChatClient>("109.120.187.67", PORT);

    if (client->connect_to_server()) {
      client->run();
    } else {
      std::cerr << "[CRITICAL] Сервер недоступен!" << std::endl;
      return 1;
    }
  } catch (const std::exception& e) {
    // что-то пошло не так внутри run(), умный указатель вызовет деструктор
    std::cerr << "[FATAL] Произошла ошибка: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
