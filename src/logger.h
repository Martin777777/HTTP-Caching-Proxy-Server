#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <boost/beast/http.hpp>
#include <atomic>

namespace http = boost::beast::http;

class Logger {
public:
    Logger(const std::string& log_file_path = "var/log/erss/proxy.log");
    ~Logger();

    void log_request(unsigned long id, const http::request<http::dynamic_body>& request, const std::string& ip_from);
    void log_response_received(unsigned long id, const http::response<http::dynamic_body>& response, const std::string& server);
    void log_request_from(unsigned long id, const http::request<http::dynamic_body>& request, const std::string& server);
    void log_proxy_response(unsigned long id, const http::response<http::dynamic_body>& response);
    void log_cache_decision(unsigned long id, const std::string& decision);
    void log_cached(unsigned long id, const http::response<http::dynamic_body>& response,  const std::chrono::steady_clock::time_point& expires_at_time_point);
    void log_tunnel_closed(unsigned long id);
    void log_message(unsigned long id, const std::string& message);

private:
    std::ofstream log_file_;
    std::mutex mutex_;
    std::atomic<unsigned long> current_id_;


};

#endif 
