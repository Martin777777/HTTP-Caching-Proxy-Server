#ifndef DOCKER_DEPLOY_BOOST_NETWORKING_H
#define DOCKER_DEPLOY_BOOST_NETWORKING_H

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <string>

namespace network = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = network::ip::tcp;

using namespace std;

#endif
