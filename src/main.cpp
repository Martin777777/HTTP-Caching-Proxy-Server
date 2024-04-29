#include "proxy.h"

int main() {
    proxy http_proxy;
    http_proxy.run_server(12345);
}