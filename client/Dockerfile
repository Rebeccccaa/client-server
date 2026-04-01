# --- ЭТАП 1: СБОРКА (BUILDER) ---
FROM alpine:3.19 AS builder

# Устанавливаем инструменты сборки
# Клиенту sqlite-dev обычно не нужен, если он сам не пишет в БД, 
# но оставим для совместимости с общими хедерами проекта.
RUN apk add --no-cache \
    build-base \
    cmake \
    sqlite-dev \
    spdlog-dev \
    nlohmann-json \
    linux-headers

WORKDIR /src

# Копируем структуру проекта
COPY include/ ./include/
COPY src/ ./src/
COPY CMakeLists.txt .
COPY CMakePresets.json .

# Собираем через пресет release
RUN cmake --preset release && \
    cmake --build --preset release -j$(nproc)

# --- ЭТАП 2: ФИНАЛЬНЫЙ ОБРАЗ (RUNTIME) ---
FROM alpine:3.19

# Библиотеки для работы бинарника
RUN apk add --no-cache \
    libstdc++ \
    spdlog

WORKDIR /app

# Копируем бинарный файл чат-клиента
# Убедись, что имя исполняемого файла в CMake — chat_client
COPY --from=builder /src/build/chat_client .

# Копируем конфиг клиента
COPY config.json .

# Запуск клиента
# Инструкция ENTRYPOINT позволит передавать аргументы, если нужно
ENTRYPOINT ["./chat_client"]
