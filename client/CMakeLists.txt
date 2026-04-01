cmake_minimum_required(VERSION 3.27)
project(ChatClient)

# Стандарт C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Указываем путь к заголовкам (твоя папка include)
# Мы используем ${CMAKE_CURRENT_SOURCE_DIR}, чтобы пути были абсолютными и надежными
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 2. Находим библиотеку потоков
find_package(Threads REQUIRED)

# находим библ. логов
find_package(spdlog REQUIRED)

# 3. Создаем исполняемый файл. 
# Указываем ПРАВИЛЬНЫЕ пути к файлам внутри папки src
add_executable(chat_client 
    src/main_client.cpp 
    src/client.cpp
)

# 4. Привязываем библиотеки
# Добавляем sqlite3 для будущего этапа
target_link_libraries(chat_client 
    Threads::Threads 
    spdlog::spdlog
    sqlite3
)
