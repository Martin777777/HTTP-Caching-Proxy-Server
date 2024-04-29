#include "logger.h"

//var/log/erss/proxy.log

Logger::Logger(const std::string& log_file_path) : log_file_(log_file_path, std::ios::app), current_id_(0) {
    if (!log_file_.is_open()) {
        throw std::runtime_error("Unable to open log file: " + log_file_path);
    }
    log_file_.flush();
}


Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void Logger::log_request(unsigned long id, const http::request<http::dynamic_body>& request, const std::string& ip_from) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::time_t time_now = std::time(nullptr);
    std::tm* utc_time = std::gmtime(&time_now);
    std::string time_str = std::asctime(utc_time);
    if (!time_str.empty() && time_str.back() == '\n') {
    time_str.pop_back();  
    }

    std::string request_line = request.method_string().to_string() + " " + request.target().to_string() + " " + "HTTP/" + std::to_string(request.version() / 10) + "." + std::to_string(request.version() % 10);

    log_file_ << id << ": \"" << request_line << "\" from " << ip_from << " @ " << time_str << std::endl;
    log_file_.flush();
}





void Logger::log_request_from(unsigned long id, const http::request<http::dynamic_body>& request, const std::string& server) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string method = request.method_string().to_string();
    std::string target = request.target().to_string();
    int version_major = request.version() / 10;
    int version_minor = request.version() % 10;
    std::string request_line = method + " " + target + " HTTP/" + std::to_string(version_major) + "." + std::to_string(version_minor);

    log_file_ << id << ": Requesting \"" << request_line << "\" from " << server << std::endl;
    log_file_.flush();
}



void Logger::log_response_received(unsigned long id, const http::response<http::dynamic_body>& response, const std::string& server) {
    std::lock_guard<std::mutex> lock(mutex_);

    int version_major = response.version() / 10;
    int version_minor = response.version() % 10;
    std::string status_code = std::to_string(response.result_int());
    std::string reason = response.reason().to_string();
    std::string response_line = "HTTP/" + std::to_string(version_major) + "." + std::to_string(version_minor) + " " + status_code + " " + reason;

    log_file_ << id << ": Received \"" << response_line << "\" from " << server << std::endl;
    log_file_.flush();
}

void Logger::log_proxy_response(unsigned long id, const http::response<http::dynamic_body>& response) {
    std::lock_guard<std::mutex> lock(mutex_);
    int version_major = response.version() / 10;
    int version_minor = response.version() % 10;
    std::string status_code = std::to_string(response.result_int());
    std::string reason_phrase = response.reason().to_string();
    std::string response_line = "HTTP/" + std::to_string(version_major) + "." + std::to_string(version_minor) + " " + status_code + " " + reason_phrase;
    log_file_ << id << ": Responding \"" << response_line << "\"" << std::endl;
    log_file_.flush();
}



void Logger::log_cache_decision(unsigned long id, const std::string& decision) {
    std::lock_guard<std::mutex> lock(mutex_);
    log_file_ << id <<": "<< decision << std::endl;
    log_file_.flush();
}


void Logger::log_cached(unsigned long id, const http::response<http::dynamic_body>& response, const std::chrono::steady_clock::time_point& expires_at_time_point) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto expires_at_system = std::chrono::system_clock::now() + std::chrono::duration_cast<std::chrono::system_clock::duration>(expires_at_time_point - std::chrono::steady_clock::now());
    auto expires_at_t = std::chrono::system_clock::to_time_t(expires_at_system);
    std::tm* utc_time = std::gmtime(&expires_at_t);
    std::string expires_at = std::asctime(utc_time); 
    if (!expires_at.empty() && expires_at.back() == '\n') {
        expires_at.pop_back(); 
    }

    log_file_ << id << ": cached, expires at " + expires_at << std::endl;
    log_file_.flush();
}




void Logger::log_tunnel_closed(unsigned long id) {
    std::lock_guard<std::mutex> lock(mutex_);
    log_file_ << id <<": Tunnel closed" << std::endl;
    log_file_.flush();
}

void Logger::log_message(unsigned long id, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    log_file_ << id <<": " <<  message << std::endl;
    log_file_.flush();
}
