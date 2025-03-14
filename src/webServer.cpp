/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webServer.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: pwojnaro <pwojnaro@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/16 21:12:30 by anamieta          #+#    #+#             */
/*   Updated: 2025/03/14 15:47:57 by pwojnaro         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "webServer.hpp"
#include <map>
#include "SocketManager.hpp"
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "parseConfig.hpp"
#include "FileUtilis.hpp"
#include <vector>
#include <dirent.h>

webServer::webServer(const std::unordered_multimap<std::string, std::string>& serverConfig,
	const std::unordered_multimap<std::string, std::vector<std::string>>& locationConfig)
: _serverConfig(serverConfig), _locationConfig(locationConfig), _socketManager()
{
std::cout << "[INFO] Initializing web server..." << std::endl;

for (const auto& entry : _serverConfig)
{
if (entry.first == "listen")
{
int port = std::stoi(entry.second);

if (auto portStatus = _socketManager.isPortAvailable(port); portStatus.has_value()) {
std::cerr << "Error: Port " << port << " is already in use. Details: " << *portStatus << std::endl;
continue;
}

try {
_socketManager.createSocket(port);
}
catch (const std::runtime_error& e)
{
std::cerr << "Error: " << e.what() << std::endl;
continue;
}
}
}

if (_socketManager.getServerSockets().empty())
{
std::cerr << "Error: No valid server sockets created" << std::endl;
throw std::runtime_error("No valid server sockets created");
}
}

void webServer::start()
{
    std::cout << "[SUCCESS] Server started on configured ports!" << std::endl;
    while (true)
	{
        int pollCount = poll(_socketManager.getPollFds().data(), _socketManager.getPollFds().size(), 500);  // 500ms timeout
        if (pollCount < 0)
		{
            std::cerr << "Error: Polling failed" << std::endl;
            continue;
        }

        for (int i = _socketManager.getPollFds().size() - 1; i >= 0; --i)
        {
            short revents = _socketManager.getPollFds()[i].revents; // Store revents in a local variable

            if (revents & POLLIN)
			{
                auto it = std::find_if(
                    _socketManager.getServerSockets().begin(),
                    _socketManager.getServerSockets().end(),
                    [fd = _socketManager.getPollFds()[i].fd](const Socket& socket)
					{
                        return socket.getFd() == fd;
                    });

					if (it != _socketManager.getServerSockets().end())
					{
						auto clientSocketOpt = _socketManager.acceptConnection(_socketManager.getPollFds()[i].fd);
						if (clientSocketOpt)
						{
							std::string serverName = it->getServerName();
							addConnection(*clientSocketOpt, serverName);
						}
					}
                else
                {
                    processRead(_socketManager.getPollFds()[i].fd);
                }
            }

            if (revents & POLLOUT)
			{
                processWrite(_socketManager.getPollFds()[i].fd);
            }

            if (revents & (POLLERR | POLLHUP))
			{
                _socketManager.closeSocket(_socketManager.getPollFds()[i].fd);
            }
        }
    }
}

void webServer::addConnection(int clientFd, const std::string& serverName)
{
    Connection conn;
    conn.socket = Socket(clientFd);
    conn.requestComplete = false;
    conn.serverName = serverName;  // Save the server name for later use
    _connections[clientFd] = std::move(conn);
}

std::string webServer::generateResponse(const HTTPRequest& request)
{
    return processRequest(request.getRawRequest());
}

void webServer::processRead(int clientSocket)
{
    // Verify the connection exists.
    if (_connections.find(clientSocket) == _connections.end())
    {
        std::cerr << "[ERROR] Received data for unknown client: " << clientSocket << std::endl;
        closeConnection(clientSocket);
        return;
    }
	
    std::string fullRequest = readFullRequest(clientSocket);
    if (fullRequest.empty())
    {
        closeConnection(clientSocket);
        return;
    }

    // Get the connection for this client.
    Connection& conn = _connections[clientSocket];
    conn.inputBuffer = fullRequest;

    // If the server name hasn't been set yet, extract it from the Host header.
    if (conn.serverName.empty())
    {
        HTTPRequest req(fullRequest);
        std::string host = req.getHeader("Host");
        conn.serverName = (!host.empty()) ? host : "default";
    }

    // Enforce the client_max_body_size limit.
    size_t maxBodySize = getClientMaxBodySize(conn.serverName);
    if (fullRequest.size() > maxBodySize)
    {
        std::cerr << "[ERROR] Request size (" << fullRequest.size() 
                  << " bytes) exceeds client_max_body_size (" << maxBodySize 
                  << " bytes) for server " << conn.serverName << "\n";
        std::string response = generateErrorResponse(413, "Payload Too Large");
        sendResponse(conn.socket, response);
        closeConnection(clientSocket);
        return;
    }

    // Process the full HTTP request.
    std::string responseStr = handleRequest(fullRequest);
    conn.outputBuffer = responseStr;
    updatePollEvents(clientSocket, POLLOUT);
}

void webServer::processWrite(int clientSocket)
{
    if (_connections.find(clientSocket) == _connections.end())
	{
        std::cerr << "[ERROR] Trying to write to unknown client: " << clientSocket << std::endl;
        closeConnection(clientSocket);
        return;
    }

    Connection &conn = _connections[clientSocket];
    if (!conn.outputBuffer.empty())
    {
        ssize_t bytesWritten = write(conn.socket.getFd(), conn.outputBuffer.c_str(), conn.outputBuffer.size());

        if (bytesWritten > 0)
        {
            conn.outputBuffer.erase(0, bytesWritten);
            // If all data is sent, close the connection
            if (conn.outputBuffer.empty())
            {
                closeConnection(clientSocket);
            }
        }
        else
        {
            std::cerr << "[ERROR] Failed to write to client: " << clientSocket << std::endl;
            closeConnection(clientSocket);
        }
    }
}

void webServer::updatePollEvents(int fd, short newEvent)
{
    auto it = std::find_if(_socketManager.getPollFds().begin(), _socketManager.getPollFds().end(), 
        [fd](const struct pollfd& pfd) { return pfd.fd == fd; });
    
    if (it != _socketManager.getPollFds().end())
	{
        it->events = newEvent;
    }
}

void webServer::closeConnection(int clientFd)
{
    _connections.erase(clientFd);
    _socketManager.closeSocket(clientFd);
}
std::string webServer::generateErrorResponse(int statusCode, const std::string& message)
{
    std::stringstream response;
    response << "HTTP/1.1 " << statusCode << " " << getStatusMessage(statusCode) << "\r\n"
             << "Content-Type: text/plain\r\n"
             << "\r\n"
             << message;
    return response.str();
}

std::unordered_map<std::string, std::string> webServer::parseHeaders(const std::string& headerSection)
{
    std::unordered_map<std::string, std::string> headerMap;
    std::istringstream stream(headerSection);
    std::string line;

    while (std::getline(stream, line) && line != "\r")
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
    return headerMap;
}

std::string webServer::resolveFilePath(const std::string& path, const std::string& rootDir)
{
    std::string resolvedPath = sanitizePath(path);
    if (resolvedPath.empty() || resolvedPath[0] != '/')
    {
        return "";
    }
    if (resolvedPath.find("/cgi-bin/") == 0)
    {
        return rootDir + resolvedPath;
    }
    return rootDir + resolvedPath;
}

std::string webServer::getStatusMessage(int statusCode)
{
    switch (statusCode)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 503: return "Service Unavailable";
        default:  return "Unknown Status";
    }
}

std::string urlDecode(const std::string& encoded)
{
    std::string result;
    for (size_t i = 0; i < encoded.length(); ++i)
	{
        if (encoded[i] == '%' && i + 2 < encoded.length())
		{
            int value;
            std::istringstream is(encoded.substr(i + 1, 2));
            if (is >> std::hex >> value)
			{
                result += static_cast<char>(value);
                i += 2;
            }
			else
			{
                result += encoded[i];
            }
        } 
		else if (encoded[i] == '+') 
		{
            result += ' ';
        } 
		else 
		{
            result += encoded[i];
        }
    }
    return result;
}

std::string webServer::handleRequest(const std::string& fullRequest) {
    if (fullRequest.empty())
        return generateErrorResponse(400, "Empty request");

    HTTPRequest httpRequest(fullRequest);
    std::string method = httpRequest.getMethod();
    std::string path = httpRequest.getPath();
    std::string decodedPath = urlDecode(path);

    std::cout << "[DEBUG] Request Path: " << path << "\n";
    std::cout << "[DEBUG] Decoded Path: " << decodedPath << "\n";

    // Debug: Show configured redirections
    std::cout << "[DEBUG] Checking against configured redirections:\n";
    for (const auto& [location, redirection] : _redirections) {
        std::cout << "  - Location: '" << location << "', Redirection: '" << redirection << "'\n";
    }

    // Special case: Handle paths that start with redirection codes (301, 302, etc.)
    if (decodedPath.size() > 4 && 
       (decodedPath.substr(0, 4) == "/301" || decodedPath.substr(0, 4) == "/302")) {
        size_t urlStart = decodedPath.find_first_not_of(" \t", 4);
        if (urlStart != std::string::npos) {
            std::string targetUrl = decodedPath.substr(urlStart);
            if (targetUrl.find("http://") != 0 && targetUrl.find("https://") != 0)
                targetUrl = "http://" + targetUrl;
            std::string statusCode = decodedPath.substr(1, 3);
            std::stringstream response;
            response << "HTTP/1.1 " << statusCode << " Moved\r\n";
            response << "Location: " << targetUrl << "\r\n";
            response << "Content-Length: 0\r\n";
            response << "\r\n";
            return response.str();
        }
    }

    // Handle configured redirections
    for (const auto& [location, redirection] : _redirections) {
        if (path.find(location) == 0 || decodedPath.find(location) == 0) {
            std::cout << "[DEBUG] Redirection match found for location: " << location << "\n";
            size_t spacePos = redirection.find_first_of(" \t");
            if (spacePos != std::string::npos) {
                std::string statusCode = redirection.substr(0, spacePos);
                std::string targetUrl = redirection.substr(spacePos + 1);
                if (targetUrl.find("http://") != 0 && targetUrl.find("https://") != 0)
                    targetUrl = "http://" + targetUrl;
                std::stringstream response;
                response << "HTTP/1.1 " << statusCode << " Moved\r\n";
                response << "Location: " << targetUrl << "\r\n";
                response << "Content-Length: 0\r\n";
                response << "\r\n";
                return response.str();
            } else {
                std::cerr << "[ERROR] Invalid redirection format: " << redirection << std::endl;
            }
        }
    }

    // Handle POST requests (including multipart/form-data)
    if (method == "POST" && (path == "/upload" || path.find("/upload/") == 0)) {
        std::string contentType = httpRequest.getHeader("Content-Type");
        std::string serverName = httpRequest.getHeader("Host");
        if (serverName.empty())
            serverName = "default";

        // Check if it's a multipart/form-data request
        if (contentType.find("multipart/form-data") != std::string::npos) {
            // Extract the boundary from the Content-Type header
            size_t boundaryPos = contentType.find("boundary=");
            if (boundaryPos == std::string::npos) {
                return generateErrorResponse(400, "Missing boundary in Content-Type");
            }
            std::string boundary = contentType.substr(boundaryPos + 9); // "boundary=" is 9 characters
            boundary = "--" + boundary; // Boundaries start with "--"

            // Get the request body
            std::string body = httpRequest.getBody();

            // Parse the multipart body
            std::vector<std::string> parts = splitMultipartBody(body, boundary);

            // Process each part
            for (const std::string& part : parts) {
                if (!processMultipartPart(part, serverName)) {
                    return generateErrorResponse(400, "Failed to process multipart part");
                }
            }

            // Return a success response
            return generateSuccessResponse("Files uploaded successfully");
        } else {
            // Handle non-multipart POST requests
            return generatePostResponse(httpRequest.getBody(), contentType, serverName);
        }
    }

    // Handle CGI requests
    if (path.find("/cgi-bin/") == 0) {
        std::string rootDir = "./www/html";
        std::string queryString;
        size_t queryPos = path.find('?');
        if (queryPos != std::string::npos) {
            queryString = path.substr(queryPos + 1);
            path = path.substr(0, queryPos);
        }
        std::string scriptPath = rootDir + path;
        return executeCGI(scriptPath, method, queryString, httpRequest.getBody());
    }

    // Determine the root directory from config (default "./www")
    std::string rootDir = "./www";
    auto it = _serverConfig.find("root");
    if (it != _serverConfig.end())
        rootDir = it->second;

    // Resolve the file path for the requested resource.
    std::string filePath = resolveFilePath(path, rootDir);
    if (filePath.empty())
        return generateErrorResponse(400, "Invalid path");

    // Debug: Print the resolved file path
    std::cout << "[DEBUG] Resolved File Path: " << filePath << "\n";

    // Check if the resolved path is a directory
    struct stat st;
    if (stat(filePath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        std::cout << "[DEBUG] Path is a directory\n";

        bool autoindexEnabled = false;
        std::string matchedLocation = "";
        std::cout << "[DEBUG] Autoindex Configuration:\n";
        for (const auto& [loc, enabled] : _autoindexConfig) {
            std::cout << "Location: " << loc << ", Autoindex: " << (enabled ? "on" : "off") << "\n";
        }

        for (const auto& locationPair : _autoindexConfig) {
            const std::string& loc = locationPair.first;
            if (!loc.empty() && path.find(loc) == 0) {
                if (loc.length() > matchedLocation.length()) {
                    matchedLocation = loc;
                    autoindexEnabled = locationPair.second;
                }
            }
        }

        std::cout << "[DEBUG] Matched Location: " << matchedLocation << ", Autoindex: " << (autoindexEnabled ? "on" : "off") << "\n";

        if (autoindexEnabled) {
            return generateDirectoryListing(filePath, path);
        } else {
            std::string indexPath = filePath;
            if (indexPath.back() != '/')
                indexPath += "/";
            indexPath += "index.html";
            if (access(indexPath.c_str(), F_OK) == 0) {
                return generateGetResponse(indexPath);
            } else {
                return generateErrorResponse(403, "Directory listing is disabled, and no index file found.");
            }
        }
		
    }
	if (method == "DELETE")
	{
		return generateDeleteResponse(filePath);
	}
    
    return generateGetResponse(filePath);
}

std::string webServer::generateGetResponse(const std::string& filePath)
{
    std::cout << "[INFO] Handling GET request for: " << filePath << std::endl;

    auto [fileContent, contentType] = FileUtils::readFile(filePath);
    if (fileContent.empty())
    {
        auto [defaultContent, defaultContentType] = HTTPResponse::getDefaultErrorPage(404);
        HTTPResponse errorResponse(404, defaultContentType, defaultContent);
        return errorResponse.generateResponse();
    }

    std::cout << "[INFO] Serving file: " << filePath << " (" 
              << fileContent.size() << " bytes, " << contentType << ")" << std::endl;

    HTTPResponse response(200, contentType, std::string(fileContent.data(), fileContent.size()));
    return response.generateResponse();
}

std::string webServer::processRequest(const std::string& request)
{
    HTTPRequest httpRequest(request);
    std::string method = httpRequest.getMethod();
    std::string path = httpRequest.getPath();

    // Handle CGI requests first.
    if (path.find("/cgi-bin/") == 0)
    {
        // Retrieve CGI configuration for the location.
        std::string location = "/cgi-bin/";
        auto it = _locationConfig.find(location);
        if (it == _locationConfig.end())
        {
            return generateErrorResponse(500, "CGI configuration not found for location: " + location);
        }

        // Extract CGI configuration from locationConfig.
        std::string cgiPass;
        std::string scriptFilename;
        for (const std::string& directive : it->second)
        {
            if (directive.find("cgi_pass") == 0)
            {
                size_t pos = directive.find_first_of(" \t");
                if (pos != std::string::npos)
                    cgiPass = directive.substr(pos + 1); // Already trimmed in parseConfig
            }
            else if (directive.find("cgi_param SCRIPT_FILENAME") == 0)
            {
                size_t pos = directive.find_first_of(" \t");
                if (pos != std::string::npos)
                    scriptFilename = directive.substr(pos + 1); // Already trimmed in parseConfig
            }
        }

        // Validate CGI configuration.
        if (cgiPass.empty() || scriptFilename.empty())
        {
            return generateErrorResponse(500, "CGI configuration is incomplete for location: " + location);
        }

        // Resolve the script path using the root directory from locationConfig.
        std::string rootDir = "./www"; // Default root directory
        for (const std::string& directive : it->second)
        {
            if (directive.find("root") == 0)
            {
                size_t pos = directive.find_first_of(" \t");
                if (pos != std::string::npos)
                    rootDir = directive.substr(pos + 1); // Already trimmed in parseConfig
                break;
            }
        }

        std::string scriptPath = rootDir + path;

        // Extract query string if present.
        std::string queryString;
        size_t queryPos = path.find('?');
        if (queryPos != std::string::npos)
        {
            queryString = path.substr(queryPos + 1);
            path = path.substr(0, queryPos);
            scriptPath = rootDir + path;
        }

        // Execute the CGI script.
        return executeCGI(scriptPath, method, queryString, httpRequest.getBody());
    }

    // Determine root directory from locationConfig.
	std::string rootDir = "./www";
    auto it = _serverConfig.find("root");
    if (it != _serverConfig.end())
    {
        rootDir = it->second;
    }

    // Resolve the file path.
    std::string filePath = resolveFilePath(path, rootDir);
    if (filePath.empty())
    {
        return generateErrorResponse(400, "Invalid path");
    }

    // Check if filePath is a directory.
    struct stat st;
    if (stat(filePath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
    {
        // Try to serve index.html.
        std::string indexPath = filePath;
        if (indexPath.back() != '/')
            indexPath += "/";
        indexPath += "index.html";
        if (access(indexPath.c_str(), F_OK) == 0)
        {
            return generateGetResponse(indexPath);
        }
        // Alternatively, you could call generateDirectoryListing() or return an error.
        return generateErrorResponse(403, "Directory listing is disabled, and no index file found.");
    }

    // Otherwise, handle as a file request.
    std::string responseStr;
    if (method == "GET")
    {
        responseStr = generateGetResponse(filePath);
    }
    else if (method == "POST")
    {
        std::string contentType = httpRequest.getHeader("Content-Type");
        if (contentType.empty())
            contentType = "text/plain";
        std::string serverName = httpRequest.getHeader("Host");
    	if (serverName.empty())
        	serverName = "default"; // Fallback if no Host header is provided.
    	responseStr = generatePostResponse(httpRequest.getBody(), contentType, serverName);
    }
    else if (method == "DELETE")
    {
        responseStr = generateDeleteResponse(filePath);
    }
    else
    {
        responseStr = generateMethodNotAllowedResponse();
    }

    return responseStr;
}

std::string webServer::getCurrentTimeString()
{
	_formNumber++;
	return std::to_string(_formNumber);
    // auto now = std::chrono::system_clock::now();
    // auto now_time_t = std::chrono::system_clock::to_time_t(now);
    // std::stringstream ss;
    // ss << std::put_time(std::localtime(&now_time_t), "%Y%m%d_%H%M%S");
    // return ss.str();
}

std::string webServer::generatePostResponse(const std::string& requestBody,
    const std::string& contentType,
    const std::string& serverName)
{
    std::cout << "[DEBUG] Processing POST request" << std::endl;
    std::cout << "[DEBUG] Content-Type: " << contentType << std::endl;
    std::cout << "[DEBUG] Request body size: " << requestBody.size() << " bytes" << std::endl;
    
    // Check if the request body exceeds the max allowed size
    size_t maxSize = getClientMaxBodySize(serverName);
    if (requestBody.size() > maxSize)
    {
        std::cerr << "[ERROR] Upload request exceeds client_max_body_size ("
            << maxSize << " bytes) for server " << serverName << "\n";
        return generateErrorResponse(413, "Payload Too Large");
    }

    // Get upload directory from server configuration
    std::string uploadDir = "./www/html/upload";
    auto it = _serverConfig.find("upload_dir");
    if (it != _serverConfig.end())
    {
        uploadDir = it->second;
    }
    
    // Create upload directory if it doesn't exist
    if (!FileUtils::createDirectoryIfNotExists(uploadDir)) {
        return generateErrorResponse(500, "Server configuration error");
    }

    // Handle multipart/form-data (file uploads)
    if (contentType.find("multipart/form-data") != std::string::npos)
    {
        std::cout << "[DEBUG] Processing multipart/form-data upload" << std::endl;
        
        // Extract boundary from Content-Type header
        std::string boundary;
        size_t boundaryPos = contentType.find("boundary=");
        if (boundaryPos != std::string::npos)
        {
            boundary = contentType.substr(boundaryPos + 9);
            // Remove quotes if present
            if (boundary.front() == '"' && boundary.back() == '"')
            {
                boundary = boundary.substr(1, boundary.size() - 2);
            }
            std::cout << "[DEBUG] Boundary: " << boundary << std::endl;
        }
        else
        {
            std::cerr << "[ERROR] No boundary found in Content-Type" << std::endl;
            return generateErrorResponse(400, "Missing boundary in multipart/form-data");
        }

        // Full boundary string
        std::string fullBoundary = "--" + boundary;
        
        // Find the first boundary
        size_t boundaryStart = requestBody.find(fullBoundary);
        if (boundaryStart == std::string::npos)
        {
            std::cerr << "[ERROR] Could not find boundary in request body" << std::endl;
            return generateErrorResponse(400, "Malformed multipart/form-data");
        }
        
        // Find the headers section
        size_t headersStart = boundaryStart + fullBoundary.length();
        if (headersStart >= requestBody.size())
        {
            std::cerr << "[ERROR] Headers section not found" << std::endl;
            return generateErrorResponse(400, "Malformed multipart/form-data");
        }
        
        // Skip CRLF after boundary
        if (requestBody[headersStart] == '\r' && headersStart + 1 < requestBody.size() && requestBody[headersStart + 1] == '\n')
        {
            headersStart += 2;
        }
        
        // Find the end of headers
        size_t headersEnd = requestBody.find("\r\n\r\n", headersStart);
        if (headersEnd == std::string::npos)
        {
            std::cerr << "[ERROR] End of headers not found" << std::endl;
            return generateErrorResponse(400, "Malformed multipart/form-data");
        }
        
        // Extract headers
        std::string headers = requestBody.substr(headersStart, headersEnd - headersStart);
        std::cout << "[DEBUG] Headers: " << headers << std::endl;
        
        // Extract filename
        std::string filename = "uploaded_file_" + getCurrentTimeString() + ".bin";
        size_t filenamePos = headers.find("filename=\"");
        if (filenamePos != std::string::npos)
        {
            size_t start = filenamePos + 10;
            size_t end = headers.find("\"", start);
            if (end != std::string::npos)
            {
                filename = sanitizeFilename(headers.substr(start, end - start));
                std::cout << "[DEBUG] Filename: " << filename << std::endl;
            }
        }
        
        // Content starts after headers
        size_t contentStart = headersEnd + 4; // Skip double CRLF
        
        // Find the next boundary
        size_t nextBoundary = requestBody.find(fullBoundary, contentStart);
        if (nextBoundary == std::string::npos)
        {
            std::cerr << "[ERROR] Closing boundary not found" << std::endl;
            return generateErrorResponse(400, "Malformed multipart/form-data");
        }
        
        // Content ends before next boundary
        size_t contentEnd = nextBoundary;
        if (contentEnd > 2 && requestBody[contentEnd - 2] == '\r' && requestBody[contentEnd - 1] == '\n')
        {
            contentEnd -= 2;
        }
        
        // Extract content
        std::string content = requestBody.substr(contentStart, contentEnd - contentStart);
        std::cout << "[DEBUG] Content size: " << content.size() << " bytes" << std::endl;
        
        // Save file
        std::string filePath = uploadDir + "/" + filename;
        std::cout << "[DEBUG] Saving file to: " << filePath << std::endl;
        
        if (!FileUtils::writeFile(filePath, content)) {
            return generateErrorResponse(500, "Failed to save file");
        }
        
        std::cout << "[POST] File saved: " << filename << std::endl;
        
        // Return success response
        std::string responseText = "File uploaded successfully: " + filename;
        return HTTPResponse(201, "text/plain", responseText).generateResponse();
    }
    // Handle application/x-www-form-urlencoded
    else if (contentType.find("application/x-www-form-urlencoded") != std::string::npos)
    {
        std::cout << "[DEBUG] Processing form-urlencoded upload" << std::endl;
        
        std::string filename = "form_data_" + getCurrentTimeString() + ".txt";
        std::string filePath = uploadDir + "/" + filename;
        
        std::cout << "[DEBUG] Saving form data to: " << filePath << std::endl;
        
        if (!FileUtils::writeFile(filePath, requestBody)) {
            return generateErrorResponse(500, "Failed to save file");
        }
        
        std::cout << "[POST] Form data saved: " << filename << std::endl;
        
        // Return success response
        std::string responseText = "Form data uploaded successfully: " + filename;
        return HTTPResponse(201, "text/plain", responseText).generateResponse();
    }
    // Handle plain text
    else if (contentType.find("text/plain") != std::string::npos)
    {
        std::cout << "[DEBUG] Processing plain text upload" << std::endl;
        
        std::string filename = "text_data_" + getCurrentTimeString() + ".txt";
        std::string filePath = uploadDir + "/" + filename;
        
        std::cout << "[DEBUG] Saving text data to: " << filePath << std::endl;
        
        if (!FileUtils::writeFile(filePath, requestBody)) {
            return generateErrorResponse(500, "Failed to save file");
        }
        
        std::cout << "[POST] Text data saved: " << filename << std::endl;
        
        // Return success response
        std::string responseText = "Text data uploaded successfully: " + filename;
        return HTTPResponse(201, "text/plain", responseText).generateResponse();
    }
    else
    {
        std::cerr << "[ERROR] Unsupported content type: " << contentType << std::endl;
        return generateErrorResponse(415, "Unsupported Media Type");
    }
}

std::string webServer::generateDeleteResponse(const std::string& filePath)
{
    // Direct path construction instead of using resolveUploadPath
    std::string adjustedFilePath;
    const std::string marker = "/upload/";
    size_t pos = filePath.find(marker);
    
    if (pos != std::string::npos) {
        std::string filename = filePath.substr(pos + marker.length());
        // Use the same path that the POST handler uses
        adjustedFilePath = "./www/html/upload/" + filename;
    } else 
	{

        adjustedFilePath = filePath;
    }
    
    if (!FileUtils::deleteFile(adjustedFilePath)) {
        if (!FileUtils::fileExists(adjustedFilePath)) {
            return generateErrorResponse(404, "File not found");
        }
        return generateErrorResponse(500, "Failed to delete file");
    }
    
    return "HTTP/1.1 204 No Content\r\n\r\n";
}

void webServer::sendResponse(Socket& clientSocket, const std::string& response)
{
    ssize_t bytesWritten = write(clientSocket.getFd(), response.c_str(), response.size());
    if (bytesWritten < 0)
	{
        std::cerr << "Error sending response: " << strerror(errno) << std::endl;
    }
}

std::string webServer::generateMethodNotAllowedResponse()
{
    auto [errorContent, errorContentType] = HTTPResponse::getDefaultErrorPage(405);
        
    std::stringstream response;
    response << "HTTP/1.1 405 Method Not Allowed\r\n";
    response << "Content-Type: " << errorContentType << "\r\n";
    response << "Content-Length: " << errorContent.size() << "\r\n";
    response << "Allow: GET, POST, DELETE\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    
    response.write(errorContent.data(), errorContent.size());
    return response.str();
}

std::string webServer::getFilePath(const std::string& path)
{
    std::string basePath = "./www/html";
    if (path == "/" || path.empty())
	{
        return basePath + "/index.html";
    }
    return basePath + path;
}

std::string webServer::executeCGI(const std::string& scriptPath, const std::string& method, 
    const std::string& queryString, const std::string& requestBody)
{
    // Remove any query string from the script path.
    std::string cleanScriptPath = scriptPath.substr(0, scriptPath.find('?'));

    int pipeToChild[2], pipeFromChild[2];
    if (pipe(pipeToChild) == -1 || pipe(pipeFromChild) == -1)
        return generateErrorResponse(500, "Failed to create pipes");

    pid_t pid = fork();
    if (pid == -1)
        return generateErrorResponse(500, "Failed to fork");

    if (pid == 0) { // Child process.
        close(pipeToChild[1]);  // Close unused write end.
        close(pipeFromChild[0]); // Close unused read end.

        if (dup2(pipeToChild[0], STDIN_FILENO) == -1 ||
            dup2(pipeFromChild[1], STDOUT_FILENO) == -1) {
            std::cerr << "Failed to redirect stdin/stdout" << std::endl;
            exit(1);
        }

        // Set CGI environment variables.
        setenv("REQUEST_METHOD", method.c_str(), 1);
        setenv("QUERY_STRING", queryString.c_str(), 1);
        setenv("CONTENT_LENGTH", std::to_string(requestBody.size()).c_str(), 1);
        setenv("CONTENT_TYPE", "application/x-www-form-urlencoded", 1);
        // SCRIPT_FILENAME and PATH_INFO should both be the full path to the script.
        setenv("SCRIPT_FILENAME", cleanScriptPath.c_str(), 1);
        setenv("PATH_INFO", cleanScriptPath.c_str(), 1);

        // Execute the Python interpreter with the CGI script as argument.
        execlp("/usr/bin/python3", "python3", cleanScriptPath.c_str(), nullptr);

        // If execlp returns, an error occurred.
        std::cerr << "Failed to execute CGI script: " << cleanScriptPath 
                  << " (Error: " << strerror(errno) << ")" << std::endl;
        exit(1);
    }

    // Parent process.
    close(pipeToChild[0]);  // Close unused read end.
    close(pipeFromChild[1]); // Close unused write end.

    if (method == "POST" && !requestBody.empty())
        write(pipeToChild[1], requestBody.c_str(), requestBody.size());
    close(pipeToChild[1]);

    std::string cgiOutput;
    char buffer[4096];
    fd_set readSet;
    
    while (true) {
        FD_ZERO(&readSet);
        FD_SET(pipeFromChild[0], &readSet);
        if (select(pipeFromChild[0] + 1, &readSet, nullptr, nullptr, nullptr) < 0) {
            std::cerr << "select() error" << std::endl;
            break;
        }
        if (FD_ISSET(pipeFromChild[0], &readSet)) {
            ssize_t bytesRead = read(pipeFromChild[0], buffer, sizeof(buffer));
            if (bytesRead > 0)
                cgiOutput.append(buffer, bytesRead);
            else if (bytesRead == 0)
                break; // EOF.
            else {
                std::cerr << "read() error" << std::endl;
                break;
            }
        }
    }
    close(pipeFromChild[0]);

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        // Prepend headers if not already present.
        if (cgiOutput.find("HTTP/1.") != 0) {
            std::string headers = "HTTP/1.1 200 OK\r\n";
            if (cgiOutput.find("\r\n\r\n") == std::string::npos)
                headers += "Content-Type: text/html\r\nContent-Length: " +
                           std::to_string(cgiOutput.size()) + "\r\n\r\n";
            cgiOutput = headers + cgiOutput;
        }
        return cgiOutput;
    }
    return generateErrorResponse(500, "CGI script failed");
}

std::string webServer::generateDirectoryListing(const std::string& directoryPath, const std::string& requestPath)
{
    DIR* dir = opendir(directoryPath.c_str());
    if (!dir)
    {
        std::cerr << "[ERROR] Failed to open directory: " << directoryPath << "\n";
        return generateErrorResponse(500, "Failed to open directory");
    }

    std::stringstream response;
    response << "<html><head><title>Index of " << requestPath << "</title></head><body>";
    response << "<h1>Index of " << requestPath << "</h1>";
    response << "<ul>";

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        std::string fileName = entry->d_name;
        if (fileName == "." || fileName == "..")
            continue;

        response << "<li><a href=\"" << requestPath << "/" << fileName << "\">" << fileName << "</a></li>";
    }

    response << "</ul></body></html>";
    closedir(dir);

    std::stringstream httpResponse;
    httpResponse << "HTTP/1.1 200 OK\r\n";
    httpResponse << "Content-Type: text/html\r\n";
    httpResponse << "Content-Length: " << response.str().size() << "\r\n";
    httpResponse << "\r\n";
    httpResponse << response.str();

    return httpResponse.str();
}
void webServer::setAutoindexConfig(const std::map<std::string, bool>& autoindexConfig)
{
    _autoindexConfig = autoindexConfig;
}
void webServer::setRedirections(const std::map<std::string, std::string>& redirections)
{
    _redirections = redirections;
}
void webServer::setServerNames(const std::map<std::string, std::string>& serverNames)
{
    _serverNames = serverNames; // Assign the parameter to the member variable
}
void webServer::setClientMaxBodySize(const std::string& serverName, size_t size) {
    _clientMaxBodySizes[serverName] = size;
}

size_t webServer::getClientMaxBodySize(const std::string& serverName) const {
    auto it = _clientMaxBodySizes.find(serverName);
    if (it != _clientMaxBodySizes.end())
        return it->second;
    // Provide a sensible default if no limit is set
    return 10485760; // e.g., 10MB
}

size_t webServer::getContentLength(const std::unordered_map<std::string, std::string>& headers)
{
    auto it = headers.find("Content-Length");
    if (it != headers.end()) {
        try {
            return std::stoul(it->second);
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Invalid Content-Length value: " << e.what() << std::endl;
        }
    }
    return 0;
}

std::string webServer::readFullRequest(int clientSocket) {
    std::string request;
    char buffer[4096] = {0};
    ssize_t bytesRead;

    // Read headers
    while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        request.append(buffer, bytesRead);

        // Check for end of headers
        size_t headersEnd = request.find("\r\n\r\n");
        if (headersEnd != std::string::npos) {
            break;
        }
    }

    // Parse headers
    std::string headerSection = request.substr(0, request.find("\r\n\r\n"));
    auto headers = parseHeaders(headerSection);

    // Handle "Expect: 100-continue" if present
    auto expectIt = headers.find("Expect");
    if (expectIt != headers.end() && expectIt->second == "100-continue") {
        const char* continueResponse = "HTTP/1.1 100 Continue\r\n\r\n";
        send(clientSocket, continueResponse, strlen(continueResponse), 0);
    }

    // Get content length and read body if needed
    size_t contentLength = getContentLength(headers);
    
    // Read the body
    if (contentLength > 0) {
        size_t bodyBytesRead = request.size() - (request.find("\r\n\r\n") + 4);
        while (bodyBytesRead < contentLength) {
            bytesRead = recv(clientSocket, buffer, std::min(sizeof(buffer) - 1, contentLength - bodyBytesRead), 0);
            if (bytesRead < 0) 
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No data available right now; you might want to poll again or sleep briefly
                    continue;
                } else {
                    std::cerr << "[ERROR] recv() failed" << std::endl;
                    break;
                }
            } else if (bytesRead == 0) {
                std::cerr << "[ERROR] Connection closed before full body received" << std::endl;
                break;
            }
            buffer[bytesRead] = '\0';
            request.append(buffer, bytesRead);
            bodyBytesRead += bytesRead;
        }
    }

    // Debug the full request
    std::cout << "[DEBUG] Full request: " << request << std::endl;
    std::cout << "[DEBUG] Request body size: " << request.substr(request.find("\r\n\r\n") + 4).size() << " bytes" << std::endl;

    return request;
}

std::string webServer::generateSuccessResponse(const std::string& message) {
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: text/plain\r\n";
    response << "Content-Length: " << message.size() << "\r\n";
    response << "\r\n";
    response << message;
    return response.str();
}

bool webServer::processMultipartPart(const std::string& part, const std::string& serverName) 
{

    size_t headersEnd = part.find("\r\n\r\n");
    if (headersEnd == std::string::npos) {
        std::cerr << "[ERROR] Invalid multipart part: missing headers" << std::endl;
        return false;
    }

    std::string headers = part.substr(0, headersEnd);
    std::string content = part.substr(headersEnd + 4);

    // Parse headers to get the filename and field name
    std::string filename;
    std::string fieldName;
    size_t namePos = headers.find("name=\"");
    if (namePos != std::string::npos) {
        size_t nameEnd = headers.find("\"", namePos + 6);
        if (nameEnd != std::string::npos) {
            fieldName = headers.substr(namePos + 6, nameEnd - (namePos + 6));
        }
    }
    size_t filenamePos = headers.find("filename=\"");
    if (filenamePos != std::string::npos) {
        size_t filenameEnd = headers.find("\"", filenamePos + 10);
        if (filenameEnd != std::string::npos) {
            filename = headers.substr(filenamePos + 10, filenameEnd - (filenamePos + 10));
        }
    }

    // If this part contains a file, save it
    if (!filename.empty()) {
        // Use serverName to determine the upload directory
        std::string uploadDir = "./www/html/upload/" + serverName;
        
        // Create directory if it doesn't exist
        if (!FileUtils::createDirectoryIfNotExists(uploadDir)) {
            return false;
        }

        std::string filePath = uploadDir + "/" + filename;
        
        if (!FileUtils::writeFile(filePath, content)) {
            return false;
        }
        
        std::cout << "[INFO] File uploaded to server '" << serverName << "': " << filePath << std::endl;
    } else if (!fieldName.empty()) {
        // Handle form fields (if needed)
        std::cout << "[DEBUG] Form field: " << fieldName << " = " << content << std::endl;
    }

    return true;
}

std::vector<std::string> webServer::splitMultipartBody(const std::string& body, const std::string& boundary) {
    std::vector<std::string> parts;
    size_t boundaryPos = body.find(boundary);

    while (boundaryPos != std::string::npos) {
        size_t nextBoundaryPos = body.find(boundary, boundaryPos + boundary.length());
        if (nextBoundaryPos == std::string::npos) break;

        std::string part = body.substr(boundaryPos + boundary.length(), nextBoundaryPos - (boundaryPos + boundary.length()));
        parts.push_back(part);

        boundaryPos = nextBoundaryPos;
    }

    return parts;
}