#ifndef DOCKER_DEPLOY_PARSER_H
#define DOCKER_DEPLOY_PARSER_H

#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <boost/algorithm/string.hpp>
#include "boost_networking.h"

class parser {
public:
    void parse_host_port(const http::request <http::dynamic_body> & request, string & host, string & port);
    void parse_from_key(const string & cacheKey, string & host, string & port, string & target);
    pair<chrono::seconds, bool> parse_max_age(const http::response <http::dynamic_body> & response);
    pair<chrono::steady_clock::time_point, bool> parse_expires(const http::response <http::dynamic_body> & response);
};

#endif
