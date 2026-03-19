#pragma once

#include <arpa/inet.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <ctime>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;

const int PORT = 7777;
const int BUFFER_SIZE = 4096;
const int LISTEN_BACKLOG = 10;  // максимальная очередь ожидающих соединений
const std::string COMMAND_EXIT = "/exit";