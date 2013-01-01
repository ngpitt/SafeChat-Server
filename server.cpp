/*
  Copyright Xphysics 2012. All Rights Reserved.

  SafeChat-Server is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation, either version 3 of the License, or (at your option) any
  later version.

  SafeChat-Server is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

  <http://www.gnu.org/licenses/>
 */

#include "server.h"

Server::Server(int argc, char *argv[]) {

    std::string string;
    std::ifstream config_file;

    try {
        _config_path = std::string(getenv("HOME")) + "/.safechat-server";
        config_file.open(_config_path.c_str());
        if (config_file) {
            while (std::getline(config_file, string))
                if (string.substr(0, 5) == "port=")
                    _port = atoi(string.substr(5).c_str());
                else if (string.substr(0, 12) == "max_sockets=")
                    _max_sockets = atoi(string.substr(12).c_str());
            config_file.close();
        }
        for (int i = 1; i < argc; i++) {
            string = argv[i];
            if (string == "-p" && i + 1 < argc)
                _port = atoi(argv[++i]);
            else if (string == "-s" && i + 1 < argc)
                _max_sockets = atoi(argv[++i]);
            else if (string == "-v") {
                std::cout << "SafeChat-Server version " << __version << "\n";
                exit(EXIT_SUCCESS);
            } else
                throw std::runtime_error("unknown argument '" + std::string(argv[i]) + "'");
        }
        if (_port < 1 || _port > 65535)
            throw std::runtime_error("invalid port number");
        if (_max_sockets < 1)
            throw std::runtime_error("invalid number of max sockets");
    } catch (const std::exception &exception) {
        std::cout << "SafeChat-Server (version " << __version << ") - (c) 2012 Nicholas Pitt \nhttps://www.xphysics.net/\n\n    -p <port> Specifies the port the server binds to\n    -s <numb> Specifies the maximum number of sockets the server opens\n    -v Displays the version\n\n" << std::flush;
        std::cerr << "SafeChat-Server: " << exception.what() << ".\n";
        exit(EXIT_FAILURE);
    }
}

Server::~Server() {

    std::ofstream config_file(_config_path.c_str());

    try {
        pthread_kill(_cleaner, SIGTERM);
        close(_socket);
        if (!config_file)
            throw std::runtime_error("can't write " + _config_path);
        config_file << "Config file for SafeChat-Server\n\nport=" << _port << "\nmax_sockets=" << _max_sockets;
        config_file.close();
    } catch (const std::exception &exception) {
        std::cerr << "Error: " << exception.what() << ".\n";
    }
}

void Server::start() {

    int new_socket;
    std::pair < Socket::socket_t::iterator, bool> pair;
    socklen_t addr_size = sizeof (sockaddr_in);
    sockaddr_in addr;

    try {
        _socket = socket(AF_INET, SOCK_STREAM, 0);
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(_port);
        if (bind(_socket, (sockaddr *) & addr, sizeof addr))
            throw std::runtime_error("can't bind to port " + _port);
        pthread_create(&_cleaner, NULL, &Server::cleaner, this);
        listen(_socket, 3);
        while (true) {
            new_socket = accept(_socket, (sockaddr *) & addr, &addr_size);
            pair = _sockets.insert(std::make_pair(new_socket, new Socket(new_socket, _sockets.size() >= (unsigned) _max_sockets ? true : false, &_sockets, &_hosts)));
            pthread_create(&pair.first->second->_listener, NULL, &Socket::listener, pair.first->second);
        }
    } catch (const std::exception &exception) {
        std::cerr << "Error: " << exception.what() << ".\n";
        exit(EXIT_FAILURE);
    }
}

void *Server::cleaner() {

    Socket::socket_t::iterator socket;

    signal(SIGTERM, thread_handler);
    while (true) {
        sleep(1);
        socket = _sockets.begin();
        while (socket != _sockets.end())
            if (socket->second->_terminated) {
                delete socket->second;
                _sockets.erase(socket++);
            } else if (difftime(time(NULL), socket->second->_time) > __time_out) {
                socket->second->log("Connection timed out, listener terminated");
                pthread_kill(socket->second->_listener, SIGTERM);
                socket->second->terminate();
                delete socket->second;
                _sockets.erase(socket++);
            } else
                socket++;
    }
    return NULL;
}
