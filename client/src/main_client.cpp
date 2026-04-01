#include <memory>

#include "client.hpp"

int main() {
  // настраиваем структуру sigaction вместо std::signal. Нужно для того чтобы accept мог завершиться
  struct sigaction sa;
  sa.sa_handler = ChatClient::signal_handler;  // наш статический метод
  sigemptyset(&sa.sa_mask);

  // это заставит системные вызовы (например recv() или connect())
  // мгновенно вернуть ошибку при нажатии Ctrl+C, чтобы поток приема завершился.
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, nullptr) == -1) {
    std::cerr << "[CRITICAL] Не удалось настроить обработчик сигналов!" << std::endl;
    return 1;
  }

  try {
    // используем умный указатель вместо обычного объекта на стеке
    auto client = std::make_unique<ChatClient>("config.json");
    // auto client = std::make_unique<ChatClient>("109.120.187.67", PORT);

    if (client->connect_to_server()) {
      client->run();
    } else {
      std::cerr << "[CRITICAL] Сервер недоступен!" << std::endl;
      return 1;
    }
  } catch (const std::exception& e) {
    // что-то пошло не так внутри run(), умный указатель вызовет деструктор
    std::cerr << "[FATAL ERROR] Непредвиденная ошибка клиента: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    // ловим, что не является std::exception
    std::cerr << "[UNKNOWN ERROR] Произошла неизвестная критическая ошибка!" << std::endl;
    return 1;
  }

  return 0;
}
