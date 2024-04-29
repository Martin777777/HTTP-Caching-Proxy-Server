#include "request_cache.h"

bool request_cache::put_if_allowed(const string & key, http::response<http::dynamic_body> response, int id) {
    if (response.result() != http::status::ok) {
        return false;
    }
    
    if (response[http::field::cache_control].find("no-store") != std::string::npos) {
        logger.log_cache_decision(id, "not cacheable because NOT STORE");
        return false;
    }
    if (response[http::field::cache_control].find("private") != std::string::npos) {
        logger.log_cache_decision(id, "not cacheable because PRIVATE");
        return false;
    }

    tbb::concurrent_hash_map<string, pair<http::response<http::dynamic_body>, chrono::steady_clock::time_point> >::accessor accessor;

    if (response[http::field::cache_control].find("no-cache") != std::string::npos) {
        data_map.insert(accessor, key);
        accessor->second = std::make_pair(response, std::chrono::steady_clock::now());
        logger.log_cache_decision(id, "cached, but requires re-validation");
        return true;
    }

    pair<chrono::seconds, bool> max_age = network_parser.parse_max_age(response);
    if (max_age.second) {
        data_map.insert(accessor, key);
        accessor->second = std::make_pair(response, std::chrono::steady_clock::now() + max_age.first);
        logger.log_cached(id, response,std::chrono::steady_clock::now() + max_age.first );
        return true;
    }

    pair<chrono::steady_clock::time_point, bool> expires = network_parser.parse_expires(response);
    if (expires.second) {
        data_map.insert(accessor, key);
        accessor->second = std::make_pair(response, expires.first);
        logger.log_cached(id, response, expires.first );
        return true;
    }

    logger.log_cache_decision(id, "not cacheable because of LACK OF DIRECTIVES");
    return false;
}

http::response<http::dynamic_body> request_cache::request_revalidation(const string & key, http::response<http::dynamic_body> & response  , int id) {
    string host;
    string port;
    string target;

    network_parser.parse_from_key(key, host, port, target);

    string etag = response[http::field::etag].to_string();
    string last_modified = response[http::field::last_modified].to_string();

    try {
        network::io_context io_context;
        tcp::resolver resolver(io_context);
        auto const results = resolver.resolve(host, port);
        tcp::socket socket(io_context);

        network::connect(socket, results.begin(), results.end());

        http::request<http::dynamic_body> request{http::verb::get, target, 11};
        request.set(http::field::host, host);
        request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        if (!etag.empty()) {
            request.set(http::field::if_none_match, etag);
        }
        if (!last_modified.empty()) {
            request.set(http::field::if_modified_since, last_modified);
        }

        http::write(socket, request);

        beast::flat_buffer buffer;

        http::response<http::dynamic_body> revalidation_response;
        http::read(socket, buffer, revalidation_response);

        beast::error_code ec;
        socket.shutdown(tcp::socket::shutdown_both, ec);

        return revalidation_response;
    } catch(std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        http::response<http::dynamic_body> error_response{http::status::internal_server_error, 11};
        beast::ostream(error_response.body()) << "Error";
        error_response.prepare_payload();
        return error_response;
    }
}

pair<http::response<http::dynamic_body>, bool> request_cache::get_if_exist(const string & key, int id) {
    tbb::concurrent_hash_map<string, pair<http::response<http::dynamic_body>, chrono::steady_clock::time_point> >::accessor accessor;

    if (data_map.find(accessor, key)) {
        pair<http::response<http::dynamic_body>, chrono::steady_clock::time_point> & cached_value = accessor->second;
        accessor.release();
        if (chrono::steady_clock::now() < cached_value.second) {
            logger.log_cache_decision(id, "in cache, valid");
            return make_pair(cached_value.first, true);
        }
        else {
            //logger.log_cache_decision(id, "in cache, but expired at");
            logger.log_cache_decision(id, "in cache, but need validation");
            http::response<http::dynamic_body> revalidation_response = request_revalidation(key, cached_value.first , id);
            if (revalidation_response.result() == http::status::not_modified) {
                pair<chrono::seconds, bool> max_age = network_parser.parse_max_age(revalidation_response);
                if (max_age.second) {
                    cached_value.second = chrono::steady_clock::now() + max_age.first;
                } else {
                    pair<chrono::steady_clock::time_point, bool> expires = network_parser.parse_expires(revalidation_response);
                    if (expires.second) {
                        cached_value.second = expires.first;
                    } else {
                        cached_value.second = chrono::steady_clock::now() + chrono::seconds(60);
                    }
                }
                return make_pair(cached_value.first, true);
            } else if (revalidation_response.result() == http::status::ok) {
                put_if_allowed(key, revalidation_response, id);
                return make_pair(revalidation_response, true);
            }
            data_map.erase(key);
        }
    }

    logger.log_cache_decision(id, "not in cache");
    return make_pair(http::response<http::dynamic_body>(), false);
}
