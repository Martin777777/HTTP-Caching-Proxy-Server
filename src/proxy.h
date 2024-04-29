#ifndef DOCKER_DEPLOY_PROXY_H
#define DOCKER_DEPLOY_PROXY_H

#include "boost_networking.h"
#include "request_cache.h"
#include "logger.h"
#include "parser.h"

class proxy {
private:
    request_cache cache;
    void transfer_data(tcp::socket& source_socket, tcp::socket& target_socket, int id);
    Logger logger;
    parser network_parser;

public:
    void run_server(short unsigned int port);
    void handle_request(tcp::socket request_socket, network::io_context & io_context , int id);
    void handle_get_request(const http::request <http::dynamic_body> & request, tcp::socket & request_socket, network::io_context & io_context, int id);
    void handle_post_request(const http::request <http::dynamic_body> & request, tcp::socket & request_socket, network::io_context & io_context, int id);
    void handle_connect_request(const http::request <http::dynamic_body> request, tcp::socket & request_socket, network::io_context & io_context, int id);
    http::response<http::dynamic_body> forward_request(const http::request<http::dynamic_body>& request, tcp::socket& request_socket, network::io_context & io_context, int id);
    void forward_connect_request(const http::request<http::dynamic_body>& request, tcp::socket & request_socket, network::io_context & io_context, int id);
    http::response<http::dynamic_body> create_error_response(http::status status, const std::string& error_message);
};

#endif
