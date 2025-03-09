/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webServer.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: pwojnaro <pwojnaro@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/16 21:11:58 by anamieta          #+#    #+#             */
/*   Updated: 2025/03/09 14:19:31 by pwojnaro         ###   ########.fr       */
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
#include "socket.hpp"
#include "SocketManager.hpp"
#include "HTTPRequest.hpp"

class webServer {
    public:
        webServer(const std::unordered_multimap<std::string, std::string>& serverConfig,
        const std::unordered_multimap<std::string, std::vector<std::string>>& locationConfig);
        void start();
		int _formNumber = 0;
    private:
    struct Connection
    {
        Socket socket;
        std::string inputBuffer;
        std::string outputBuffer;
        bool requestComplete;
        int cgiPipeIn[2];
        int cgiPipeOut[2];
        pid_t cgiPid;
    };
		std::unordered_multimap<std::string, std::string> _serverConfig;
		std::unordered_multimap<std::string, std::vector<std::string>> _locationConfig;
		SocketManager _socketManager; 
        std::unordered_map<int, Connection> _connections;
        std::string handleRequest(const std::string& fullRequest);
        void sendResponse(Socket& clientSocket, const std::string& response);
		std::string generateDeleteResponse(const std::string& filePath);
		std::string generateMethodNotAllowedResponse();
        void addConnection(int clientFd);
        std::string processRequest(const std::string& request);
        void processRead(int clientSocket);
        void processWrite(int clientSocket);
        void updatePollEvents(int fd, short newEvent);
        void closeConnection(int fd);
		std::string generatePostResponse(const std::string& requestBody, const std::string& contentType);
		std::string sanitizePath(const std::string& path);
		std::string sanitizeFilename(const std::string& filename);
		std::string generateGetResponse(const std::string& filePath);
        std::vector<struct pollfd> _pollfds;
		std::string getCurrentTimeString();
		std::string generateResponse(const HTTPRequest& request);
		std::string getStatusMessage(int statusCode);
		std::string getFilePath(const std::string& path);
		std::string generateErrorResponse(int statusCode, const std::string& message);
		std::unordered_map<std::string, std::string> parseHeaders(std::istringstream& requestStream);
		std::string resolveFilePath(const std::string& path, const std::string& rootDir);
		std::pair<std::vector<char>, std::string> readFile(const std::string& filePath);
		std::string executeCGI(const std::string& scriptPath, const std::string& method, const std::string& queryString, const std::string& requestBody);
};