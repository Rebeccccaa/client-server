#pragma once

#include <arpa/inet.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;

const std::string CFG_PATH = "config.json";

const int PORT = 7777;
const int BUFFER_SIZE = 4096;
const int LISTEN_BACKLOG = 10;
const std::string COMMAND_EXIT = "/exit";
inline std::string DB_PATH = "chat_history.db";
inline std::string LOG_LEVEL = "info";
