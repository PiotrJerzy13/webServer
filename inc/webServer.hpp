/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webServer.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: anamieta <anamieta@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/16 21:11:58 by anamieta          #+#    #+#             */
/*   Updated: 2025/02/21 16:45:23 by anamieta         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <unordered_map>
#include <vector>
#include <netinet/in.h>
#include <poll.h>
#include <fcntl.h>
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
#include <filesystem>
#include <csignal>

class webServer {
    public:
        webServer(const std::unordered_multimap<std::string, std::string>& serverConfig,
                    const std::unordered_multimap<std::string, std::vector<std::string>>& locationConfig);
        void start();
    private:
        void handleRequest(int clientSocket);
        void sendResponse(int clientSocket, const std::string& response);
        void setNonBlocking(int socket);

        std::unordered_multimap<std::string, std::string> _serverConfig;
        std::unordered_multimap<std::string, std::vector<std::string>> _locationConfig;
        std::vector<int> _serverSockets;
        std::vector<struct pollfd> _pollfds;
};