#include "parser.h"

void parser::parse_host_port(const http::request <http::dynamic_body> & request, string & host, string & port) {
    if(request.find("Host") != request.end()) {
        auto host_header = request["Host"].to_string();
        auto colon_position = host_header.find(':');
        if(colon_position != string::npos) {
            host = host_header.substr(0, colon_position);
            port = host_header.substr(colon_position + 1);
        } else {
            host = host_header;
        }
    }
    else {
        throw runtime_error("No Host Name!");
    }
}

void parser::parse_from_key(const string & cacheKey, string & host, string & port, string & target) {
    vector<string> parts;
    boost::split(parts, cacheKey, boost::is_any_of("|"));

    host = parts[0];
    port = parts[1];
    target = parts[2];
}

pair<chrono::seconds, bool> parser::parse_max_age(const http::response <http::dynamic_body> & response) {
    string cache_control = response[http::field::cache_control].to_string();
    size_t pos_s_max_age = cache_control.find("s-maxage=");
    size_t pos_max_age = cache_control.find("max-age=");

    if (pos_s_max_age != std::string::npos) {
        size_t start = pos_s_max_age + 9;
        size_t end = cache_control.find(',', start);
        string s_max_age = cache_control.substr(start, end - start);
        return make_pair(chrono::seconds(std::stoi(s_max_age)), true);
    }
    else if (pos_max_age != std::string::npos) {
        size_t start = pos_max_age + 8;
        size_t end = cache_control.find(',', start);
        string max_age = cache_control.substr(start, end - start);
        return make_pair(chrono::seconds(std::stoi(max_age)), true);
    }

    return make_pair(chrono::seconds(), false);
}

pair<chrono::steady_clock::time_point, bool> parser::parse_expires(const http::response<http::dynamic_body>& response) {
    if (response.find(http::field::expires) != response.end()) {
        string expires_string = response[http::field::expires].to_string();
        tm tm = {};
        istringstream ss(expires_string);
        ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
        if (ss.fail()) {
            return make_pair(chrono::steady_clock::time_point(), false);
        }
        auto time_c = std::mktime(&tm);
        if (time_c == -1) {
            return make_pair(chrono::steady_clock::time_point(), false);
        }

        auto expires_time = chrono::system_clock::from_time_t(time_c);
        auto now_system = chrono::system_clock::now();
        auto duration_until_expires = expires_time - now_system;

        // Convert duration to steady_clock::time_point
        auto now_steady = chrono::steady_clock::now();
        auto expires_steady = now_steady + duration_until_expires;

        return make_pair(expires_steady, true);
    }

    return make_pair(chrono::steady_clock::time_point(), false);
}