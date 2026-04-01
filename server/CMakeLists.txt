cmake_minimum_required(VERSION 3.27)
project(ChatServer)

# стандарт C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# указываем путь к заголовкам (твоя папка include)
# мы используем ${CMAKE_CURRENT_SOURCE_DIR}, чтобы пути были абсолютными и надежными
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# находим пакет spdlog (он установит цель spdlog::spdlog)
find_package(spdlog REQUIRED)

# находим библиотеку потоков
find_package(Threads REQUIRED)

# находим библиотеку с бд
find_package(SQLite3 REQUIRED)

# создаем исполняемый файл
# указываем ПРАВИЛЬНЫЕ пути к файлам внутри папки src
add_executable(chat_server 
    src/main_server.cpp 
    src/server.cpp
)

# привязываем библиотеки
target_link_libraries(
    chat_server
PRIVATE
    Threads::Threads
    spdlog::spdlog
    sqlite3
)