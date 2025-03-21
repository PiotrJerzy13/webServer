/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: pwojnaro <pwojnaro@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/07 16:05:11 by pwojnaro          #+#    #+#             */
/*   Updated: 2025/03/21 19:51:00 by pwojnaro         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <optional>
#include <iostream>
#include <cstdio>
#include <array>
#include "SocketManager.hpp"


SocketManager::SocketManager() {}


SocketManager::~SocketManager()
{
    for (auto& socket : _serverSockets)
	{
        close(socket.getFd());
    }
}
/**
 * Creates a new socket for the specified port
 * Sets it to non-blocking mode
 * Binds it to the specified port
 * Sets it to listen mode
 * Adds it to internal socket and poll descriptors collections
*/
void SocketManager::createSocket(int port)
{
    try {
        Socket serverSocket(AF_INET, SOCK_STREAM, 0);
        setNonBlocking(serverSocket.getFd());

        sockaddr_in serverAddr{};
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (bind(serverSocket.getFd(), (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
		{
            std::cerr << "Error: Could not bind socket for port " << port << " (" << strerror(errno) << ")" << std::endl;
            return;
        }

        if (listen(serverSocket.getFd(), 50) < 0)
		{
            std::cerr << "Error: Could not listen on socket for port " << port << " (" << strerror(errno) << ")" << std::endl;
            return;
        }

        _serverSockets.push_back(std::move(serverSocket));

        struct pollfd pfd{};
        pfd.fd = _serverSockets.back().getFd();
        pfd.events = POLLIN;
        _pollFds.push_back(pfd);

        std::cout << "[INFO] Listening on port " << port << std::endl;
    }
	catch (const std::exception& e)
	{
        std::cerr << e.what() << std::endl;
    }
}
/**
 * Accepts a new client connection on the given server socket
 * Sets the new client socket to non-blocking mode
 * Adds it to the poll descriptors with POLLIN events
 * Returns the new client file descriptor
*/

std::optional<int> SocketManager::acceptConnection(int serverFd)
{
    int clientFd = accept(serverFd, nullptr, nullptr);
    if (clientFd < 0)
    {
        std::cerr << "Error: Could not accept connection" << std::endl;
        return std::nullopt;
    }

    setNonBlocking(clientFd);

    auto it = std::find_if(_pollFds.begin(), _pollFds.end(), [clientFd](const pollfd &pfd)
    {
        return pfd.fd == clientFd;
    });
    if (it == _pollFds.end())
    {
        struct pollfd pfd{};
        pfd.fd = clientFd;
        pfd.events = POLLIN;
        _pollFds.push_back(pfd);
    }
    else
    {
        std::cerr << "[WARN] FD already in poll list: " << clientFd << std::endl;
    }

    std::cout << "[INFO] New client connected: " << clientFd << std::endl;
    return clientFd;
}

void SocketManager::closeSocket(int fd)
{
    std::cout << "[INFO] Closing socket: " << fd << std::endl;

    _pollFds.erase( std::remove_if(_pollFds.begin(), _pollFds.end(),
    [fd](const struct pollfd &pfd) { return pfd.fd == fd; }),_pollFds.end()
    );

    close(fd);
}

std::vector<pollfd>& SocketManager::getPollFds()
{
    return _pollFds;
}

std::vector<Socket>& SocketManager::getServerSockets()
{
    return _serverSockets;
}
//set a socket to non-blocking mode using fcntl
void SocketManager::setNonBlocking(int socketFd)
{
    if (fcntl(socketFd, F_SETFL, O_NONBLOCK) == -1)
	{
        std::cerr << "Error: Could not set socket to non-blocking" << std::endl;
    }
}
/**
 * Uses the system's lsof command to check if a port is already in use
 * Returns the command output if the port is in use, or std::nullopt if available
 * dev/null is "black hole"—anything written to it disappears immediately, we dont care about printing anything
 */
std::optional<std::string> SocketManager::isPortAvailable(int port)
{
    std::array<char, 128> buffer;
    std::string result;
    std::string command = "lsof -i :" + std::to_string(port) + " 2>/dev/null";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe)
	{
        std::cerr << "Error: Failed to run command: " << command << std::endl;
        return "Failed to run command";
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
	{
        result += buffer.data();
    }

    int exitStatus = pclose(pipe);
    if (exitStatus == -1)
	{
        std::cerr << "Error: Failed to close pipe" << std::endl;
        return "Failed to close pipe";
    }

    std::cout << "Port " << port << " check result: " << (result.empty() ? "Available" : "In use") << std::endl;

    if (!result.empty())
	{
        std::cout << "lsof output: " << result << std::endl;
        return result;
    }
    return std::nullopt; // Port is available
}
