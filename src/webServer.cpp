/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webServer.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eleni <eleni@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/16 21:12:30 by anamieta          #+#    #+#             */
/*   Updated: 2025/02/19 15:33:42 by eleni            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

// tips from Harsh:
// setsockopt(); SO_REUSEADDR,
// fcntl(); F_SETFL, O_NONBLOCK
// memset to 0 after creating a variable

#include "webServer.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <array>

bool isPortAvailable(int port)
{
    std::array<char, 128> buffer;
    std::string result;
    std::string command = "lsof -i :" + std::to_string(port);

    // Open pipe to file
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to run command" << std::endl;
        return false;
    }

    // Read the output a line at a time - output it.
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    // Close pipe
    pclose(pipe);

    return result.empty();
}

webServer::webServer(const std::unordered_multimap<std::string, std::string>& serverConfig,
                     const std::unordered_multimap<std::string, std::vector<std::string>>& locationConfig)
    : _serverConfig(serverConfig), _locationConfig(locationConfig)
{
    // Initialize server sockets and other configurations
    for (const auto& entry : _serverConfig)
    {
        if (entry.first == "listen")
        {
            int port = std::stoi(entry.second);

            // Check if the port is available
            if (!isPortAvailable(port))
            {
                std::cerr << "Error: Port " << port << " is already in use" << std::endl;
                continue;
            }

            int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
            if (serverSocket < 0)
            {
                std::cerr << "Error: Could not create socket for port " << port << std::endl;
                continue;
            }

            setNonBlocking(serverSocket);

            sockaddr_in serverAddr;
            memset(&serverAddr, 0, sizeof(serverAddr));
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_addr.s_addr = INADDR_ANY;
            serverAddr.sin_port = htons(port);

            if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
            {
                std::cerr << "Error: Could not bind socket for port " << port << std::endl;
                close(serverSocket);
                continue;
            }

            if (listen(serverSocket, 10) < 0)
            {
                std::cerr << "Error: Could not listen on socket for port " << port << std::endl;
                close(serverSocket);
                continue;
            }

            _serverSockets.push_back(serverSocket);
            _pollfds.push_back({serverSocket, POLLIN, 0});
        }
    }

    if (_serverSockets.empty())
    {
        std::cerr << "Error: No valid server sockets created" << std::endl;
        throw std::runtime_error("No valid server sockets created");
    }
}

void webServer::setNonBlocking(int socket)
{
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1)
    {
        std::cerr << "Error: Could not get socket flags" << std::endl;
        return;
    }
    if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        std::cerr << "Error: Could not set socket to non-blocking" << std::endl;
    }
}

void webServer::start()
{
    std::cout << "Server started on configured ports" << std::endl;

    while (true)
    {
        int pollCount = poll(_pollfds.data(), _pollfds.size(), -1);
        if (pollCount < 0)
        {
            std::cerr << "Error: Polling failed" << std::endl;
            return;
        }

        for (size_t i = 0; i < _pollfds.size(); ++i)
        {
            if (_pollfds[i].revents & POLLIN)
            {
                if (std::find(_serverSockets.begin(), _serverSockets.end(), _pollfds[i].fd) != _serverSockets.end())
                {
                    int clientSocket = accept(_pollfds[i].fd, nullptr, nullptr);
                    if (clientSocket < 0)
                    {
                        std::cerr << "Error: Could not accept connection" << std::endl;
                        continue;
                    }

                    setNonBlocking(clientSocket);
                    _pollfds.push_back({clientSocket, POLLIN, 0});
                }
                else
                {
                    handleRequest(_pollfds[i].fd);
                    close(_pollfds[i].fd);
                    _pollfds.erase(_pollfds.begin() + i);
                    --i;
                }
            }
        }
    }
}

void webServer::handleRequest(int clientSocket)
{
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    read(clientSocket, buffer, sizeof(buffer) - 1);

    std::string request(buffer);
    std::cout << "Received request: " << request << std::endl;

    // Parse the request to get the method and path
    std::istringstream requestStream(request);
    std::string method, path, version;
    requestStream >> method >> path >> version;

    if (method == "GET")
    {
        // Get the root directory from the configuration
        std::string rootDir = _serverConfig.find("root")->second;
        if (path.back() == '/')
        {
            path += "index.html";
        }
        std::string filePath = rootDir + path;
        std::cout << "Looking for file: " << filePath << std::endl; // Debugging line

        std::ifstream file(filePath);
        if (file.is_open())
        {
            std::stringstream fileContent;
            fileContent << file.rdbuf();
            file.close();

            std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + fileContent.str();
            sendResponse(clientSocket, response);
        }
        else
        {
            std::cerr << "File not found: " << filePath << std::endl; // Debugging line
            std::string response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n404 Not Found";
            sendResponse(clientSocket, response);
        }
    }
    else
    {
        // Handle other methods or return 405 Method Not Allowed
        std::string response = "HTTP/1.1 405 Method Not Allowed\r\nContent-Type: text/html\r\n\r\n405 Method Not Allowed";
        sendResponse(clientSocket, response);
    }
}

void webServer::sendResponse(int clientSocket, const std::string& response)
{
    write(clientSocket, response.c_str(), response.size());
}