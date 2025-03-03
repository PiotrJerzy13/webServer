/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webServer.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: pwojnaro <pwojnaro@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/16 21:12:30 by anamieta          #+#    #+#             */
/*   Updated: 2025/03/03 14:58:37 by pwojnaro         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "webServer.hpp"
#include <map>

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

			// std::cout << port << std::endl;

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
    std::vector<char> buffer(8192, 0);
    ssize_t bytesRead = 0;
    std::string fullRequest;
    
    do {
        bytesRead = read(clientSocket, buffer.data(), buffer.size() - 1);
        if (bytesRead > 0) {
            fullRequest.append(buffer.data(), bytesRead);
            
            if (fullRequest.find("\r\n\r\n") != std::string::npos)
			{
                break;
            }
        }
    } while (bytesRead > 0);
    
    if (fullRequest.empty())
	{
        sendResponse(clientSocket, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nEmpty request");
        return;
    }

    std::istringstream requestStream(fullRequest);
    std::string method, path, version;
    requestStream >> method >> path >> version;

    if (method.empty() || path.empty() || version.empty())
	{
        sendResponse(clientSocket, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nMalformed Request");
        return;
    }
    
    std::unordered_map<std::string, std::string> headerMap;
    std::string line;
    std::getline(requestStream, line);
    
    while (std::getline(requestStream, line) && line != "\r")
	{
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos)
		{
            std::string name = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of("\r\n") + 1);
            
            headerMap[name] = value;
        }
    }
    std::string requestBody;
    if (headerMap.find("Content-Length") != headerMap.end())
	{
        size_t contentLength = std::stoul(headerMap["Content-Length"]);
        size_t headerEnd = fullRequest.find("\r\n\r\n");
        
        if (headerEnd != std::string::npos)
		{
            requestBody = fullRequest.substr(headerEnd + 4);
            
            while (requestBody.length() < contentLength)
			{
                bytesRead = read(clientSocket, buffer.data(), buffer.size() - 1);
                if (bytesRead > 0)
				{
                    buffer[bytesRead] = '\0';
                    requestBody.append(buffer.data(), bytesRead);
                }
				else
				{
                    break;
                }
            }
        }
    }
    path = sanitizePath(path);
    if (path.empty() || path[0] != '/')
	{
        sendResponse(clientSocket, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid path");
        return;
    }

    std::string rootDir = "./www";
    if (_serverConfig.find("root") != _serverConfig.end())
	{
        rootDir = _serverConfig.find("root")->second;
    }
    
    if (path.back() == '/')
	{
        path += "index.html";
    }

    std::string filePath = rootDir + path;
    std::cout << method << " " << path << " " << version << std::endl;
	std::string contentType = "application/x-www-form-urlencoded";

	auto it = headerMap.find("Content-Type");
	if (it != headerMap.end())
	{
		contentType = it->second;
	}
	
    if (method == "GET")
	{
        handleGetRequest(clientSocket, filePath);
    }
	else if (method == "POST")
	{
        handlePostRequest(clientSocket, requestBody, contentType);
    }
	else if (method == "DELETE")
	{
        handleDeleteRequest(clientSocket, filePath);
    }
	else
	{
		handleMethodNotAllowed(clientSocket);
    }
}

void webServer::handleGetRequest(int clientSocket, const std::string& filePath)
{
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) == -1)
	{
        std::cerr << "File not found: " << filePath << std::endl;
        
        std::string contentType;
        std::string errorPage = getDefaultErrorPage(404, contentType);
        
        std::stringstream response;
        response << "HTTP/1.1 404 Not Found\r\n"
                 << "Content-Type: " << contentType << "\r\n"
                 << "Content-Length: " << errorPage.size() << "\r\n"
                 << "Connection: close\r\n"
                 << "\r\n"
                 << errorPage;
                 
        sendResponse(clientSocket, response.str());
        return;
    }
    
    if (access(filePath.c_str(), R_OK) == -1)
	{
        std::cerr << "Permission denied: " << filePath << std::endl;
        
        std::string contentType;
        std::string errorPage = getDefaultErrorPage(403, contentType);
        
        std::stringstream response;
        response << "HTTP/1.1 403 Forbidden\r\n"
                 << "Content-Type: " << contentType << "\r\n"
                 << "Content-Length: " << errorPage.size() << "\r\n"
                 << "Connection: close\r\n"
                 << "\r\n"
                 << errorPage;
                 
        sendResponse(clientSocket, response.str());
        return;
    }
    
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
	{
        std::cerr << "Failed to open file: " << filePath << std::endl;
        
        std::string contentType;
        std::string errorPage = getDefaultErrorPage(500, contentType);
        
        std::stringstream response;
        response << "HTTP/1.1 500 Internal Server Error\r\n"
                 << "Content-Type: " << contentType << "\r\n"
                 << "Content-Length: " << errorPage.size() << "\r\n"
                 << "Connection: close\r\n"
                 << "\r\n"
                 << errorPage;

        sendResponse(clientSocket, response.str());
        return;
    }

    std::string contentType = getContentType(filePath);
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<char> fileContent(fileSize);
    file.read(fileContent.data(), fileSize);
    file.close();
    
    std::stringstream responseHeader;
    responseHeader << "HTTP/1.1 200 OK\r\n"
                   << "Content-Type: " << contentType << "\r\n"
                   << "Content-Length: " << fileSize << "\r\n"
                   << "Connection: close\r\n"
                   << "\r\n";
    
    std::string header = responseHeader.str();
    write(clientSocket, header.c_str(), header.size());
    write(clientSocket, fileContent.data(), fileSize);
}

std::string webServer::getContentType(const std::string& filePath)
{
    size_t dotPos = filePath.find_last_of(".");
    if (dotPos == std::string::npos)
	{
        return "application/octet-stream";
    }
    
    std::string ext = filePath.substr(dotPos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "json") return "application/json";
    if (ext == "txt") return "text/plain";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "png") return "image/png";
    if (ext == "gif") return "image/gif";
    if (ext == "ico") return "image/x-icon";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "pdf") return "application/pdf";
    
    return "application/octet-stream";
}

void webServer::processRequest(int clientSocket, const std::string& request)
{
    std::string method, path, version;
    std::istringstream requestStream(request);
    requestStream >> method >> path >> version;
    
    std::map<std::string, std::string> headers;
    std::string line;
    while (std::getline(requestStream, line) && line != "\r")
	{
        size_t colonPos = line.find(":");
        if (colonPos != std::string::npos)
		{
            std::string headerName = line.substr(0, colonPos);
            std::string headerValue = line.substr(colonPos + 1);
            headerValue.erase(0, headerValue.find_first_not_of(" "));
            headerValue.erase(headerValue.find_last_not_of("\r") + 1);
            headers[headerName] = headerValue;
        }
    }
    std::string requestBody;
    if (method == "POST")
	{
        std::stringstream bodyStream;
        bodyStream << requestStream.rdbuf();
        requestBody = bodyStream.str();
    }
    
    if (method == "GET")
	{
        handleGetRequest(clientSocket, getFilePath(path));
    } else if (method == "POST")
	{
        std::string contentType = "text/plain";
        auto it = headers.find("Content-Type");
        if (it != headers.end())
		{
            contentType = it->second;
        }
        handlePostRequest(clientSocket, requestBody, contentType);
    }
	else
	{
        handleMethodNotAllowed(clientSocket);
    }
}

void webServer::handlePostRequest(int clientSocket, const std::string& requestBody, const std::string& contentType)
{
    std::string uploadDir = "./www/html/uploads";
    if (_serverConfig.find("upload_dir") != _serverConfig.end())
    {
        uploadDir = _serverConfig.find("upload_dir")->second;
    }

    std::filesystem::create_directories(uploadDir);
    
    if (contentType.find("multipart/form-data") != std::string::npos)
	{
        std::string filename = "default_upload.txt";
        size_t filenamePos = requestBody.find("filename=\"");
        if (filenamePos != std::string::npos)
		{
            size_t start = filenamePos + 10;
            size_t end = requestBody.find("\"", start);
            if (end != std::string::npos)
			{
                filename = requestBody.substr(start, end - start);
                filename = sanitizeFilename(filename);
            }
        }

        std::string filePath = uploadDir + "/" + filename;
        size_t contentStart = requestBody.find("\r\n\r\n");
        if (contentStart != std::string::npos)
		{
            contentStart += 4;
        }
		else
		{
            sendResponse(clientSocket, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nMalformed request");
            return;
        }

        std::ofstream file(filePath, std::ios::out | std::ios::binary);
        if (!file)
        {
            sendResponse(clientSocket, "HTTP/1.1 500 Internal Server Error\r\n"
                                   "Content-Type: text/plain\r\n\r\n"
                                   "Failed to save file");
            return;
        }
        
        file.write(requestBody.data() + contentStart, requestBody.size() - contentStart);
        file.close();
        std::cout << "[POST] File saved: " << filename << std::endl;
    }
    else if (contentType.find("application/x-www-form-urlencoded") != std::string::npos)
	{

        std::string filename = "form_data_" + getCurrentTimeString() + ".txt";
        std::string filePath = uploadDir + "/" + filename;
        
        std::ofstream file(filePath, std::ios::out);
        if (!file)
        {
            sendResponse(clientSocket, "HTTP/1.1 500 Internal Server Error\r\n"
                                   "Content-Type: text/plain\r\n\r\n"
                                   "Failed to save file");
            return;
        }
        file << requestBody;
        file.close();
        std::cout << "[POST] Form data saved: " << filename << std::endl;
    }
    else
	{

        sendResponse(clientSocket, "HTTP/1.1 415 Unsupported Media Type\r\n"
                               "Content-Type: text/plain\r\n\r\n"
                               "Unsupported content type");
        return;
    }
    std::stringstream response;
    response << "HTTP/1.1 201 Created\r\n"
             << "Content-Type: text/plain\r\n\r\n"
             << "File uploaded successfully!";
    
    sendResponse(clientSocket, response.str());
}

std::string webServer::getCurrentTimeString()
{
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}


void webServer::handleDeleteRequest(int clientSocket, const std::string& filePath)
{
    std::string adjustedFilePath = filePath;
    size_t pos = filePath.find("/uploads/");
    if (pos != std::string::npos)
	{
         std::string filename = filePath.substr(pos + std::string("/uploads/").length());
         adjustedFilePath = "./www/uploads/" + filename;
    }
    
    if (!std::filesystem::exists(adjustedFilePath))
    {
        std::cerr << "[DELETE] File not found: " << adjustedFilePath << std::endl;
        sendResponse(clientSocket, "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile not found");
        return;
    }

    if (!std::filesystem::is_regular_file(adjustedFilePath))
    {
        sendResponse(clientSocket, "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\n\r\nCannot delete directories");
        return;
    }
    try
    {
        if (std::filesystem::remove(adjustedFilePath))
        {
            std::cout << "[DELETE] Successfully deleted file: " << adjustedFilePath << std::endl;
            sendResponse(clientSocket, "HTTP/1.1 204 No Content\r\n\r\n");
        }
        else
        {
            std::cerr << "[DELETE] Failed to delete file: " << adjustedFilePath << std::endl;
            sendResponse(clientSocket, "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nFailed to delete file");
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        std::cerr << "[DELETE] Filesystem error: " << e.what() << std::endl;
        sendResponse(clientSocket, "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nFailed to delete file");
    }
}

void webServer::sendResponse(int clientSocket, const std::string& response)
{
    ssize_t bytesWritten = write(clientSocket, response.c_str(), response.size());
    if (bytesWritten < 0)
	{
        std::cerr << "Error sending response: " << strerror(errno) << std::endl;
    }
}

std::string webServer::getDefaultErrorPage(int errorCode, std::string& contentType)
{
    const std::string basePath = "./www/html/error_pages/";
    const std::string imagePath = basePath + std::to_string(errorCode) + ".jpg";
    const std::string defaultImagePath = basePath + "default.jpg";
    
    std::cout << "[INFO] Serving error page for code: " << errorCode << std::endl;
    
    std::ifstream imageFile(imagePath, std::ios::binary);
    if (imageFile.good())
	{
        try {
            std::stringstream imageStream;
            imageStream << imageFile.rdbuf();
            
            if (imageStream.fail())
			{
                std::cerr << "[WARN] Failed to read error image: " << imagePath << std::endl;
            }
			else
			{
                contentType = "image/jpeg";
                return imageStream.str();
            }
        }
		catch (const std::exception& e) {
            std::cerr << "[ERROR] Exception reading error image: " << e.what() << std::endl;
        }
        imageFile.close();
    }

    std::ifstream defaultImageFile(defaultImagePath, std::ios::binary);
    if (defaultImageFile.good())
	{
        try {
            std::stringstream imageStream;
            imageStream << defaultImageFile.rdbuf();
            
            if (imageStream.fail())
			{
                std::cerr << "[WARN] Failed to read default error image" << std::endl;
            }
			else
			{
                contentType = "image/jpeg";  
                return imageStream.str();
            }
        }
		catch (const std::exception& e)
		{
            std::cerr << "[ERROR] Exception reading default error image: " << e.what() << std::endl;
        }
        defaultImageFile.close();
    }
	else
	{
        std::cerr << "[ERROR] Missing default error image: " << defaultImagePath << std::endl;
    }

    contentType = "text/plain";
    return "Error " + std::to_string(errorCode) + ": Missing error page.";
}
void webServer::handleMethodNotAllowed(int clientSocket)
{
    std::string contentType;
    std::string errorPage = getDefaultErrorPage(405, contentType);
    
    std::stringstream response;
    response << "HTTP/1.1 405 Method Not Allowed\r\n"
             << "Content-Type: " << contentType << "\r\n"
             << "Content-Length: " << errorPage.size() << "\r\n"
             << "Allow: GET, POST, DELETE\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << errorPage;
             
    sendResponse(clientSocket, response.str());
}
std::string webServer::getFilePath(const std::string& path)
{
    std::string basePath = "./public";
    if (path == "/" || path.empty())
	{
        return basePath + "/index.html";
    }
    return basePath + path;
}
