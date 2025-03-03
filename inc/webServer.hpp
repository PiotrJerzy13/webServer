/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webServer.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: pwojnaro <pwojnaro@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/16 21:11:58 by anamieta          #+#    #+#             */
/*   Updated: 2025/03/03 14:46:18 by pwojnaro         ###   ########.fr       */
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
#include <sys/stat.h>

class webServer {
    public:
        webServer(const std::unordered_multimap<std::string, std::string>& serverConfig,
                    const std::unordered_multimap<std::string, std::vector<std::string>>& locationConfig);
        void start();
    private:
        void handleRequest(int clientSocket);
        void sendResponse(int clientSocket, const std::string& response);
        void setNonBlocking(int socket);
		void handleDeleteRequest(int clientSocket, const std::string& filePath);
		void handlePostRequest(int clientSocket, const std::string& requestBody, const std::string& contentType);
		void handleMethodNotAllowed(int clientSocket);
		void processRequest(int clientSocket, const std::string& request);
		std::string sanitizePath(const std::string& path);
		std::string sanitizeFilename(const std::string& filename);
		std::string getContentType(const std::string& filePath);
		void handleGetRequest(int clientSocket, const std::string& filePath);
		std::string getDefaultErrorPage(int errorCode, std::string& contentType);
        std::unordered_multimap<std::string, std::string> _serverConfig;
        std::unordered_multimap<std::string, std::vector<std::string>> _locationConfig;
        std::vector<int> _serverSockets;
        std::vector<struct pollfd> _pollfds;
		std::string getCurrentTimeString();
		std::string getFilePath(const std::string& path);
};