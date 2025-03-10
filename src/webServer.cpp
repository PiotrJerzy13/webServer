/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webServer.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: pwojnaro <pwojnaro@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/16 21:12:30 by anamieta          #+#    #+#             */
/*   Updated: 2025/03/10 20:13:52 by pwojnaro         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "webServer.hpp"
#include <map>
#include "SocketManager.hpp"
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "parseConfig.hpp"
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
                    // Accept only one connection per poll event
                    auto clientSocketOpt = _socketManager.acceptConnection(_socketManager.getPollFds()[i].fd);
                    if (clientSocketOpt)
					{
                        addConnection(*clientSocketOpt);
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
void webServer::addConnection(int clientFd)
{
    Connection conn;
    conn.socket = Socket(clientFd);
    conn.requestComplete = false;
    _connections[clientFd] = std::move(conn);
}

std::string webServer::generateResponse(const HTTPRequest& request)
{
    return processRequest(request.getRawRequest());
}

void webServer::processRead(int clientSocket)
{
    if (_connections.find(clientSocket) == _connections.end())
    {
        std::cerr << "[ERROR] Received data for unknown client: " << clientSocket << std::endl;
        closeConnection(clientSocket);
        return;
    }

    char buffer[1024];
    ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer));

    if (bytesRead > 0)
    {
        _connections[clientSocket].inputBuffer.append(buffer, bytesRead);

        if (_connections[clientSocket].inputBuffer.find("\r\n\r\n") != std::string::npos)
        {
            std::string rawRequest = _connections[clientSocket].inputBuffer;
            std::string responseStr = handleRequest(rawRequest);
            _connections[clientSocket].outputBuffer = responseStr;
            updatePollEvents(clientSocket, POLLOUT);
        }
    }
    else
    {
        closeConnection(clientSocket);
    }
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

std::unordered_map<std::string, std::string> webServer::parseHeaders(std::istringstream& requestStream)
{
    std::unordered_map<std::string, std::string> headerMap;
    std::string line;

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

std::string urlDecode(const std::string& encoded) {
    std::string result;
    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.length()) {
            int value;
            std::istringstream is(encoded.substr(i + 1, 2));
            if (is >> std::hex >> value) {
                result += static_cast<char>(value);
                i += 2;
            } else {
                result += encoded[i];
            }
        } else if (encoded[i] == '+') {
            result += ' ';
        } else {
            result += encoded[i];
        }
    }
    return result;
}

std::string webServer::handleRequest(const std::string& fullRequest)
{
    if (fullRequest.empty())
        return generateErrorResponse(400, "Empty request");

    HTTPRequest httpRequest(fullRequest);
    std::string method = httpRequest.getMethod();
    std::string path = httpRequest.getPath();
    std::string decodedPath = urlDecode(path);

    // Debug: Print the request path
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
    
    // Extract the URL part after the status code and any whitespace
    size_t urlStart = decodedPath.find_first_not_of(" \t", 4);
    if (urlStart != std::string::npos) {
        // Use this as the target URL for redirection
        std::string targetUrl = decodedPath.substr(urlStart);
        
        // Ensure the target URL is absolute
        if (targetUrl.find("http://") != 0 && targetUrl.find("https://") != 0) {
            targetUrl = "http://" + targetUrl;
        }
        
        // Get the status code from the path
        std::string statusCode = decodedPath.substr(1, 3);
        
        // Generate redirection response
        std::stringstream response;
        response << "HTTP/1.1 " << statusCode << " Moved\r\n";
        response << "Location: " << targetUrl << "\r\n";
        response << "Content-Length: 0\r\n";
        response << "\r\n";
        return response.str();
    }
}

    for (const auto& [location, redirection] : _redirections)
    {
        // Check both the original and decoded path
        if (path.find(location) == 0 || decodedPath.find(location) == 0)
        {
            std::cout << "[DEBUG] Redirection match found for location: " << location << "\n";
            
            // Split the redirection into status code and target URL
            size_t spacePos = redirection.find_first_of(" \t");
            if (spacePos != std::string::npos)
            {
                std::string statusCode = redirection.substr(0, spacePos);
                std::string targetUrl = redirection.substr(spacePos + 1);

                // Ensure the target URL is absolute
                if (targetUrl.find("http://") != 0 && targetUrl.find("https://") != 0) {
                    targetUrl = "http://" + targetUrl; // Default to HTTP if no scheme is provided
                }

                // Generate the redirect response
                std::cout << "[DEBUG] Generating redirect: " << statusCode << " to " << targetUrl << "\n";
                std::stringstream response;
                response << "HTTP/1.1 " << statusCode << " Moved\r\n";
                response << "Location: " << targetUrl << "\r\n";
                response << "Content-Length: 0\r\n";
                response << "\r\n";
                return response.str();
            }
            else
            {
                std::cerr << "[ERROR] Invalid redirection format: " << redirection << std::endl;
            }
        }
    }

    if (path.find("/cgi-bin/") == 0)
    {
        std::string rootDir = "./www/html";
        std::string queryString;
        size_t queryPos = path.find('?');
        if (queryPos != std::string::npos)
        {
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
    if (stat(filePath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
    {
        // Debug: Print directory check result
        std::cout << "[DEBUG] Path is a directory\n";

        // Check if autoindex is enabled for this location
        bool autoindexEnabled = false;
        std::string matchedLocation = "";

        // Debug: Print autoindex configuration
        std::cout << "[DEBUG] Autoindex Configuration:\n";
        for (const auto& [loc, enabled] : _autoindexConfig)
        {
            std::cout << "Location: " << loc << ", Autoindex: " << (enabled ? "on" : "off") << "\n";
        }

        // Iterate through the autoindex configuration to find the best match
        for (const auto& locationPair : _autoindexConfig)
        {
            const std::string& loc = locationPair.first;
            // Ensure the location is not empty and matches the start of the path
            if (!loc.empty() && path.find(loc) == 0)
            {
                // If the current location is more specific than the previous match, update the match
                if (loc.length() > matchedLocation.length())
                {
                    matchedLocation = loc;
                    autoindexEnabled = locationPair.second;
                }
            }
        }

        // Debug: Print matched location and autoindex status
        std::cout << "[DEBUG] Matched Location: " << matchedLocation << ", Autoindex: " << (autoindexEnabled ? "on" : "off") << "\n";

        if (autoindexEnabled)
        {
            // Generate and return a directory listing.
            return generateDirectoryListing(filePath, path);
        }
        else
        {
            // Attempt to serve the index.html file from the directory.
            std::string indexPath = filePath;
            if (indexPath.back() != '/')
                indexPath += "/";
            indexPath += "index.html";
            if (access(indexPath.c_str(), F_OK) == 0)
            {
                return generateGetResponse(indexPath);
            }
            else
            {
                return generateErrorResponse(403, "Directory listing is disabled, and no index file found.");
            }
        }
    }
    return processRequest(fullRequest);
}

std::pair<std::vector<char>, std::string> webServer::readFile(const std::string& filePath)
{
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) == -1)
	{
        std::cout << "[INFO] File not found: " << filePath << std::endl;
        return {{}, ""};
    }

    if (access(filePath.c_str(), R_OK) == -1)
	{
        std::cout << "[INFO] File not accessible: " << filePath << std::endl;
        return {{}, ""};
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
	{
        std::cout << "[ERROR] Failed to open file: " << filePath << std::endl;
        return {{}, ""};
    }

    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> fileContent(fileSize);
    if (!file.read(fileContent.data(), fileSize))
	{
        std::cout << "[ERROR] Failed to read file content: " << filePath << std::endl;
        file.close();
        return {{}, ""};
    }
    file.close();
    return {fileContent, HTTPResponse::getContentType(filePath)};
}

std::string webServer::generateGetResponse(const std::string& filePath)
{
    std::cout << "[INFO] Handling GET request for: " << filePath << std::endl;

    auto [fileContent, contentType] = readFile(filePath);
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
        std::string rootDir = "./www/html";
        std::string scriptPath = rootDir + path;
        std::string queryString;
        size_t queryPos = path.find('?');
        if (queryPos != std::string::npos)
        {
            queryString = path.substr(queryPos + 1);
            path = path.substr(0, queryPos);
            scriptPath = rootDir + path;
        }
        return executeCGI(scriptPath, method, queryString, httpRequest.getBody());
    }

    // Determine root directory from config.
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
        responseStr = generatePostResponse(httpRequest.getBody(), contentType);
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
std::string webServer::generatePostResponse(const std::string& requestBody, const std::string& contentType)
{
    // Determine upload directory from server configuration (or use default)
    std::string uploadDir = "./www/html/uploads";
    auto it = _serverConfig.find("upload_dir");
    if (it != _serverConfig.end())
    {
        uploadDir = it->second;
    }
    std::filesystem::create_directories(uploadDir);

    // Handle multipart/form-data
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
                filename = sanitizeFilename(requestBody.substr(start, end - start));
            }
        }

        size_t contentStart = requestBody.find("\r\n\r\n");
        if (contentStart == std::string::npos)
            return generateErrorResponse(400, "Malformed request");
        contentStart += 4;

        std::string filePath = uploadDir + "/" + filename;
        std::ofstream file(filePath, std::ios::binary);
        if (!file)
            return generateErrorResponse(500, "Failed to save file");

        file.write(requestBody.data() + contentStart, requestBody.size() - contentStart);
        file.close();
        std::cout << "[POST] File saved: " << filename << std::endl;

        return HTTPResponse(201, "text/plain", "File uploaded successfully!").generateResponse();
    }
    // Handle application/x-www-form-urlencoded
    else if (contentType.find("application/x-www-form-urlencoded") != std::string::npos)
    {
        std::string filename = "form_data_" + getCurrentTimeString() + ".txt";
        std::string filePath = uploadDir + "/" + filename;
        std::ofstream file(filePath);
        if (!file)
            return generateErrorResponse(500, "Failed to save file");

        file << requestBody;
        file.close();
        std::cout << "[POST] Form data saved: " << filename << std::endl;

        return HTTPResponse(201, "text/plain", "File uploaded successfully!").generateResponse();
    }
    else
    {
        return generateErrorResponse(415, "Unsupported content type");
    }
}

std::string webServer::generateDeleteResponse(const std::string& filePath)
{
    std::string adjustedFilePath = filePath;
    const std::string marker = "/uploads/";
    size_t pos = filePath.find(marker);
    if (pos != std::string::npos)
    {
        adjustedFilePath = "./www/uploads/" + filePath.substr(pos + marker.length());
    }
    
    if (!std::filesystem::exists(adjustedFilePath))
    {
        std::cerr << "[DELETE] File not found: " << adjustedFilePath << std::endl;
        return generateErrorResponse(404, "File not found");
    }

    if (!std::filesystem::is_regular_file(adjustedFilePath))
    {
        return generateErrorResponse(403, "Cannot delete directories");
    }
    try
    {
        if (std::filesystem::remove(adjustedFilePath))
        {
            std::cout << "[DELETE] Successfully deleted file: " << adjustedFilePath << std::endl;
            return "HTTP/1.1 204 No Content\r\n\r\n";
        }
        else
        {
            std::cerr << "[DELETE] Failed to delete file: " << adjustedFilePath << std::endl;
            return generateErrorResponse(500, "Failed to delete file");
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        std::cerr << "[DELETE] Filesystem error: " << e.what() << std::endl;
        return generateErrorResponse(500, "Failed to delete file");
    }
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
    // Remove any query string from the script path
    std::string cleanScriptPath = scriptPath.substr(0, scriptPath.find('?'));

    int pipeToChild[2], pipeFromChild[2];
    if (pipe(pipeToChild) == -1 || pipe(pipeFromChild) == -1)
        return generateErrorResponse(500, "Failed to create pipes");

    pid_t pid = fork();
    if (pid == -1)
        return generateErrorResponse(500, "Failed to fork");

    if (pid == 0) {
        // Child process
        close(pipeToChild[1]);  // Close unused write end
        close(pipeFromChild[0]); // Close unused read end

        if (dup2(pipeToChild[0], STDIN_FILENO) == -1 || dup2(pipeFromChild[1], STDOUT_FILENO) == -1) {
            std::cerr << "Failed to redirect stdin/stdout" << std::endl;
            exit(1);
        }

        setenv("REQUEST_METHOD", method.c_str(), 1);
        setenv("QUERY_STRING", queryString.c_str(), 1);
        setenv("CONTENT_LENGTH", std::to_string(requestBody.size()).c_str(), 1);
        setenv("CONTENT_TYPE", "application/x-www-form-urlencoded", 1);
        setenv("SCRIPT_NAME", cleanScriptPath.c_str(), 1);
        setenv("PATH_INFO", "", 1);

        execl(cleanScriptPath.c_str(), cleanScriptPath.c_str(), nullptr);
        std::cerr << "Failed to execute CGI script: " << cleanScriptPath 
                  << " (Error: " << strerror(errno) << ")" << std::endl;
        exit(1);
    }
    
    close(pipeToChild[0]);  // Close unused read end
    close(pipeFromChild[1]); // Close unused write end

    if (method == "POST" && !requestBody.empty())
        write(pipeToChild[1], requestBody.c_str(), requestBody.size());
    close(pipeToChild[1]);

    std::string cgiOutput;
    char buffer[4096];
    fd_set readSet;
    
    while (true)
	{
        FD_ZERO(&readSet);
        FD_SET(pipeFromChild[0], &readSet);
		
        if (select(pipeFromChild[0] + 1, &readSet, nullptr, nullptr, nullptr) < 0)
		{
            std::cerr << "select() error" << std::endl;
            break;
        }

        if (FD_ISSET(pipeFromChild[0], &readSet)) {
            ssize_t bytesRead = read(pipeFromChild[0], buffer, sizeof(buffer));
            if (bytesRead > 0)
                cgiOutput.append(buffer, bytesRead);
            else if (bytesRead == 0)
                break; // EOF
            else {
                std::cerr << "read() error" << std::endl;
                break;
            }
        }
    }
    close(pipeFromChild[0]);

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
	{
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
