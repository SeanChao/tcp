#include "eventloop.hh"
#define LAB_IMPL _  // Comment to use kernal TCP
#ifdef LAB_IMPL
#include "tcp_sponge_socket.hh"
#else
#include "socket.hh"
#endif
#include "util.hh"

#include <cstdlib>
#include <iostream>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

using namespace std;

void get_URL(const string &host, const string &path) {
    // connect to the "http" service on the computer whose name is in
    // the "host" string, then request the URL path given in the "path" string.
#ifdef LAB_IMPL
    FullStackSocket socket = FullStackSocket();
#else
    TCPSocket socket = TCPSocket();
#endif
    cerr << "\033[;32mGET\033[;0m " << host << path << endl;

    socket.connect(Address(host, "http"));
    const std::string request = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\n\r\n";
    socket.write(request);
    socket.shutdown(SHUT_WR);

    // Then you'll need to print out everything the server sends back,
    // (not just one call to read() -- everything) until you reach
    // the "eof" (end of file).
    cout << "wait for server responce.\n";
    auto i = 0;
    while (!socket.eof()) {
        cout << socket.read();
        i++;
    }
    cerr << "read " << i << " times.\n";

#ifdef LAB_IMPL
    // include fd close
    socket.wait_until_closed();
#else
    socket.close();
#endif

    // spdlog::shutdown();
}

int main(int argc, char *argv[]) {
    try {
        if (argc <= 0) {
            abort();  // For sticklers: don't try to access argv[0] if argc <= 0.
        }

        // The program takes two command-line arguments: the hostname and "path" part of the URL.
        // Print the usage message unless there are these two arguments (plus the program name
        // itself, so arg count = 3 in total).
        if (argc != 3) {
            cerr << "Usage: " << argv[0] << " HOST PATH\n";
            cerr << "\tExample: " << argv[0] << " stanford.edu /class/cs144\n";
            return EXIT_FAILURE;
        }

        // Get the command-line arguments.
        const string host = argv[1];
        const string path = argv[2];

        // spdlog::set_default_logger(spdlog::basic_logger_mt("webget", "/tmp/tcplab.log"));
        // spdlog::info("Hello!");
        // spdlog::flush_every(std::chrono::seconds(1));

        // Call the student-written function.
        get_URL(host, path);
    } catch (const exception &e) {
        cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
