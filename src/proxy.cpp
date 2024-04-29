#include "proxy.h"
#include <thread>
#include <iostream>
#include <array>


void proxy::run_server(short unsigned int port) {
    try {
        network::io_context io_context;
        tcp::acceptor acceptor(io_context, {tcp::v4(), port});
        int requestId = 0;

        while (true) {
            tcp::socket socket(io_context);
            requestId++;
            acceptor.accept(socket);
            thread(&proxy::handle_request, this, move(socket), ref(io_context), requestId).detach();
        }

        //Our exception guarantee is No Throw. Every possible exception is carefully caught and handled.
        //handle exception when accepting new connection and creating thread, it should be system error
    } catch (exception &e) {
        cerr << "Exception occurred while running the server:  " << e.what() << "\n";
    }
}

void proxy::handle_request(tcp::socket request_socket, network::io_context &io_context, int id) {
    try {
        beast::flat_buffer buffer;
        http::request <http::dynamic_body> request;
        http::read(request_socket, buffer, request);
        auto remote_endpoint = request_socket.remote_endpoint();
        auto ip_address = remote_endpoint.address().to_string();
        logger.log_request(id, request, ip_address);

        cout << request.target().to_string() << endl;

        switch (request.method()) {
            case http::verb::get:
                handle_get_request(request, request_socket, io_context, id);
                break;
            case http::verb::connect:
                handle_connect_request(request, request_socket, io_context, id);
                break;
            case http::verb::post:
                handle_post_request(request, request_socket, io_context, id);
                break;
                //handle exception if the client request is malformed, send the error reponse to the client
            default:
                auto error_response = create_error_response(http::status::bad_request, "Malformed Request");
                http::write(request_socket, error_response);
                request_socket.shutdown(tcp::socket::shutdown_both);
                request_socket.close();
                break;
        }
    
        //handle exception when resolving the host, and getting the request method, close client socket
    } catch (const std::exception &e) {
        auto error_response = create_error_response(http::status::internal_server_error, "Internal Server Error: The proxy encountered an unexpected condition.");
        http::write(request_socket, error_response);
        request_socket.shutdown(tcp::socket::shutdown_both);
        request_socket.close();
        std::cerr << "Exception occurred while accepting the request from client:  " << e.what() << std::endl;
    }
}

void proxy::handle_get_request(const http::request <http::dynamic_body> &request, tcp::socket &request_socket, network::io_context &io_context, int id) {
    string host;
    string port = "80";
    network_parser.parse_host_port(request, host, port);
    string cached_key = host + "|" + port + "|" + request.target().to_string();
    pair<http::response < http::dynamic_body>, bool > cached_value = cache.get_if_exist(cached_key, id);
    if (cached_value.second) {
        try {
            http::write(request_socket, cached_value.first);
            cout << "returned cache" << endl;
            //handle exception when retriving response from cache and sending to client. gracefully close client socket after sending the error message
        } catch (const std::exception &e) {
            std::cerr << "Failed to send cached response: " << e.what() << std::endl;
            auto error_response = create_error_response(http::status::internal_server_error, "Internal Server Error: The proxy encountered an unexpected condition.");
            http::write(request_socket, error_response);
            request_socket.shutdown(tcp::socket::shutdown_both);
            request_socket.close();
        }
        return;
    }
    
    http::response <http::dynamic_body> response = forward_request(request, request_socket, io_context, id);
    cache.put_if_allowed(cached_key, response, id);
}

void proxy::handle_post_request(const http::request <http::dynamic_body> &request, tcp::socket &request_socket, network::io_context &io_context, int id) {
    forward_request(request, request_socket, io_context, id);
}

void proxy::handle_connect_request(const http::request <http::dynamic_body> request, tcp::socket &request_socket, network::io_context &io_context, int id) {
    string target_host;
    string target_port = "443";

    network_parser.parse_host_port(request, target_host, target_port);

    try {

        tcp::resolver resolver(io_context);
        auto const results = resolver.resolve(target_host, target_port);
        auto server_socket = make_shared<tcp::socket>(io_context);
        network::connect(*server_socket, results.begin(), results.end());
        logger.log_request_from(id, request, target_host);

        http::response <http::dynamic_body> response{http::status::ok, request.version()};
        response.set(http::field::connection, "close");
        response.prepare_payload();

        logger.log_proxy_response(id, response);
        http::write(request_socket, response);

        auto request_socket_shared = make_shared<tcp::socket>(move(request_socket));

        std::thread client_to_server_thread([this, request_socket_shared, server_socket, id]() {
            this->transfer_data(*request_socket_shared, *server_socket, id);
        });
        std::thread server_to_client_thread([this, server_socket, request_socket_shared, id]() {
            this->transfer_data(*server_socket, *request_socket_shared, id);
        });

        client_to_server_thread.detach();
        server_to_client_thread.detach();
        logger.log_tunnel_closed(id);

        //the exception occurs during the connction, send error message to client
    } catch (const boost::system::system_error &e) {
        //the response is corruted or the connection is closed
        if (e.code() == boost::asio::error::eof) {
            auto error_response = create_error_response(http::status::bad_gateway, "Corrupted Response or Connection Closed");
            http::write(request_socket, error_response);
            //the connection is refused due to some issue
        } else if (e.code() == boost::asio::error::connection_refused) {
            auto error_response = create_error_response(http::status::bad_gateway, "Connection Refused by Upstream Server");
            http::write(request_socket, error_response);
            //the internal server may have issue
        } else {
            auto error_response = create_error_response(http::status::internal_server_error, "Internal Server Error");
            http::write(request_socket, error_response);
        }
        request_socket.shutdown(tcp::socket::shutdown_both);
        request_socket.close();


        //it should be a system error, print it out and gracefully close client socket after sending the error message
    } catch (const std::exception &e) {
        auto error_response = create_error_response(http::status::internal_server_error, "Internal Server Error: The proxy encountered an unexpected condition.");
        http::write(request_socket, error_response);
        request_socket.shutdown(tcp::socket::shutdown_both);
        request_socket.close();
        std::cerr << "Exception occurred while forwarding connect request : " << e.what() << std::endl;
    }
}

http::response <http::dynamic_body> proxy::forward_request(const http::request <http::dynamic_body> &request, tcp::socket &request_socket, network::io_context &io_context, int id) {
    try {
        boost::beast::flat_buffer buffer;

        string port = "80";
        string host;

        network_parser.parse_host_port(request, host, port);

        tcp::resolver resolver(io_context);
        auto const results = resolver.resolve(host, port);
        tcp::socket socket(io_context);
        network::connect(socket, results.begin(), results.end());

        //request from server
        logger.log_request_from(id, request, host);
        http::write(socket, request);

        //receive from server
        http::response <http::dynamic_body> response;
        http::read(socket, buffer, response);
        logger.log_response_received(id, response, host);

        //response to client
        logger.log_proxy_response(id, response);
        http::write(request_socket, response);

        return response;

         //the exception occurs during the connction, send error message to client
    } catch (boost::system::system_error &e) {
            //the response is corruted or the connection is closed
        if (e.code() == boost::asio::error::eof) {
            auto error_response = create_error_response(http::status::bad_gateway, "Corrupted Response or Connection Closed");
            http::write(request_socket, error_response);
             //the connection is refused due to some issue
        } else if (e.code() == boost::asio::error::connection_refused) {
            auto error_response = create_error_response(http::status::bad_gateway, "Connection Refused by Upstream Server");
            http::write(request_socket, error_response);
             //the internal server may have issue
        } else {
            auto error_response = create_error_response(http::status::internal_server_error, "Internal Server Error");
            http::write(request_socket, error_response);
        }
        http::response <http::dynamic_body> error_response{http::status::internal_server_error, 11};
        beast::ostream(error_response.body()) << "Internal Server Error: The proxy encountered an unexpected condition";
        error_response.prepare_payload();
        request_socket.shutdown(tcp::socket::shutdown_both);
        request_socket.close();
        return error_response;

    } catch (exception &e) {
        //it should be a system error, print it out and gracefully close client socket after sending the error message
        std::cerr << "Exception occurred while forwarding request: : " << e.what() << std::endl;
        http::response <http::dynamic_body> error_response{http::status::internal_server_error, 11};
        beast::ostream(error_response.body()) << "Internal Server Error: The proxy encountered an unexpected condition";
        error_response.prepare_payload();
        http::write(request_socket, error_response);
        request_socket.shutdown(tcp::socket::shutdown_both);
        request_socket.close();
        return error_response;

    }

    
}

http::response <http::dynamic_body> proxy::create_error_response(http::status status, const std::string &error_message) {
    http::response <http::dynamic_body> error_response{status, 11};
    beast::ostream(error_response.body()) << error_message;
    error_response.prepare_payload();
    return error_response;
}

void proxy::transfer_data(tcp::socket &source_socket, tcp::socket &target_socket, int id) {
    try {
        const size_t buffer_size = 4096;
        char data[buffer_size];
        boost::system::error_code ec;
        size_t bytes_transferred;

        while (true) {
            bytes_transferred = source_socket.read_some(network::buffer(data), ec);
            if (ec) {
                if (ec == network::error::eof) {
                    target_socket.shutdown(tcp::socket::shutdown_both);

                } else {
                    cerr << "Error during data transfer: " << ec.message() << endl;
                    auto error_response = create_error_response(http::status::internal_server_error, "Internal Server Error During Reading Data");
                    http::write(target_socket, error_response);
                }
                break;
            }

            target_socket.write_some(network::buffer(data, bytes_transferred), ec);
            if (ec) {
                cerr << "Error writing data: " << ec.message() << endl;
                auto error_response = create_error_response(http::status::internal_server_error, "Internal Server Error During Sending Data");
                http::write(source_socket, error_response);
                break;
            }
        }
        //gracefully close client socket after sending the error message
    } catch (const std::exception &e) {
        cerr << "Exception during data transfer: " << e.what() << endl;
        auto error_response = create_error_response(http::status::internal_server_error, "Internal Server Error: The proxy encountered an unexpected condition.");
        http::write(source_socket, error_response);
        source_socket.shutdown(tcp::socket::shutdown_both);
        source_socket.close();
        http::write(target_socket, error_response);
        target_socket.shutdown(tcp::socket::shutdown_both);
        target_socket.close();
    }
}
