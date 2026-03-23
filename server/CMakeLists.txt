cmake_minimum_required(VERSION 3.28.3)
project(ChatServer)

# Стандарт C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Указываем путь к заголовкам (твоя папка include)
# Мы используем ${CMAKE_CURRENT_SOURCE_DIR}, чтобы пути были абсолютными и надежными
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 2. Находим пакет spdlog (он установит цель spdlog::spdlog)
find_package(spdlog REQUIRED)

# 3. Находим библиотеку потоков
find_package(Threads REQUIRED)

# 4. Создаем исполняемый файл. 
# Указываем ПРАВИЛЬНЫЕ пути к файлам внутри папки src
add_executable(chat_server 
    src/main_server.cpp 
    src/server.cpp
)

# 5. Привязываем библиотеки
# Добавляем sqlite3 для будущего этапа
target_link_libraries(chat_server 
    Threads::Threads 
    sqlite3
    spdlog::spdlog
)
