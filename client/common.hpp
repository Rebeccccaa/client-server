#pragma once

#include <arpa/inet.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

const int PORT = 7777;
const int BUFFER_SIZE = 1024;
const int LISTEN_BACKLOG = 10;  // Максимальная очередь ожидающих соединений
const std::string COMMAND_EXIT = "/exit";
