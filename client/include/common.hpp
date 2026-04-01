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

const int PORT = 9000;
const int BUFFER_SIZE = 4096;
const std::string COMMAND_EXIT = "/exit";
inline std::string IP_LOCALHOST = "127.0.0.1";
inline spdlog::level::level_enum LOG_LEVEL = spdlog::level::info;
