#ifndef DOCKER_DEPLOY_REQUEST_CACHE_H
#define DOCKER_DEPLOY_REQUEST_CACHE_H

#include <tbb/concurrent_hash_map.h>
#include <iostream>
#include <string>
#include <chrono>
#include "boost_networking.h"
#include "parser.h"
#include "logger.h"

class request_cache {
private:
    tbb::concurrent_hash_map<string, pair<http::response<http::dynamic_body>, chrono::steady_clock::time_point> > data_map;
    http::response<http::dynamic_body> request_revalidation(const string & target, http::response<http::dynamic_body> & response , int id);
    parser network_parser;
    Logger logger;

public:
    bool put_if_allowed(const string & key, http::response<http::dynamic_body> response  , int id);
    pair<http::response<http::dynamic_body>, bool> get_if_exist(const string & key , int id);
};

#endif
