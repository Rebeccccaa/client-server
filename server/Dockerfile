# --- ЭТАП 1: СБОРКА (BUILDER) ---
# Используем фиксированную стабильную версию Alpine
FROM alpine:3.19 AS builder

# МЕНЯЕМ ЗЕРКАЛО (добавь эту строку):
# RUN sed -i 's/dl-cdn.alpinelinux.org/mirror.yandex.ru/g' /etc/apk/repositories

# 1. Устанавливаем системные зависимости для сборки
# build-base: g++, make, libc-dev
# linux-headers: заголовочные файлы ядра для работы с сокетами
RUN apk add --no-cache \
    build-base \
    cmake \
    sqlite-dev \
    spdlog-dev \
    nlohmann-json \
    linux-headers

# 2. Указываем рабочую директорию
WORKDIR /src

# 3. Копируем исходный код и пресеты
# Сначала копируем только папки, чтобы сохранить структуру
COPY include/ ./include/
COPY src/ ./src/
COPY CMakeLists.txt .
COPY CMakePresets.json .

# 4. Конфигурируем и собираем проект через пресет "release"
# Мы используем пресет, чтобы настройки в Docker и локально были идентичны
RUN cmake --preset release && \
    cmake --build --preset release -j$(nproc)

# --- ЭТАП 2: ФИНАЛЬНЫЙ ОБРАЗ (RUNTIME) ---
FROM alpine:3.19

# RUN sed -i 's/dl-cdn.alpinelinux.org/mirror.yandex.ru/g' /etc/apk/repositories

# 5. Устанавливаем только библиотеки времени выполнения (Shared Libs)
# Это делает образ максимально легким (около 15-20 МБ)
RUN apk add --no-cache \
    libstdc++ \
    sqlite-libs \
    spdlog

WORKDIR /app

# 6. Копируем только готовый бинарный файл из папки сборки пресета
# В твоем пресете это "${sourceDir}/build"
COPY --from=builder /src/build/chat_server .

# 7. Копируем файл конфигурации
COPY config.json .

# 8. Информируем Docker о порте (должен совпадать с портом в config.json)
EXPOSE 9000

# 9. Запуск сервера
# Используем форму массива для правильной обработки сигналов (Ctrl+C)
ENTRYPOINT ["./chat_server"]
