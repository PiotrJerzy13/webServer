/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webServer.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: anamieta <anamieta@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/16 21:12:30 by anamieta          #+#    #+#             */
/*   Updated: 2025/03/21 19:25:09 by anamieta         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <map>
#include "SocketManager.hpp"
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "parseConfig.hpp"
#include "FileUtilis.hpp"
#include <vector>
#include <dirent.h>
#include "CGIHandler.hpp"

volatile sig_atomic_t timeoutOccurred = 0;

webServer::webServer(const std::unordered_multimap<std::string, std::string>& serverConfig,
	const std::unordered_multimap<std::string, std::vector<std::string>>& locationConfig)
	: _serverConfig(serverConfig), _locationConfig(locationConfig), _socketManager(), _cgiHandler(serverConfig)
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
	return handleRequest(request.getRawRequest());
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

std::string webServer::generateErrorResponse(int errorCode, const std::string& errorMessage) {
	// Get the default error page for the given error code
	auto [errorPageContent, contentType] = HTTPResponse::getDefaultErrorPage(errorCode);

	// Generate the HTTP response
	std::string response = "HTTP/1.1 " + std::to_string(errorCode) + " " + errorMessage + "\r\n";
	response += "Content-Type: " + contentType + "\r\n";
	response += "Content-Length: " + std::to_string(errorPageContent.size()) + "\r\n";
	response += "\r\n";
	response += errorPageContent;

	return response;
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
		return "";
	// if (resolvedPath.find("/cgi-bin/") == 0)
	// 	return rootDir + resolvedPath;

	// Debug: Print out all entries in _rootDirectories
	std::cout << "[DEBUG] _rootDirectories contents:\n";
	for (const auto& [loc, root] : _rootDirectories) {
		std::cout << "  Location: '" << loc << "', Root: '" << root << "'\n";
	}

	// Check for location-specific root directory from _rootDirectories
	std::string matchedLocation = "";
	std::string locationRootDir = rootDir; // Default to server root
	for (const auto& [location, customRoot] : _rootDirectories) {
		std::cout << "[DEBUG] Checking location '" << location << "' against path '" << resolvedPath << "'\n";
		if (!location.empty() && resolvedPath.find(location) == 0) {
			std::cout << "[DEBUG] Match found!\n";
			if (location.length() > matchedLocation.length()) {
				matchedLocation = location;
				locationRootDir = customRoot;
				std::cout << "[DEBUG] Using location '" << matchedLocation << "' with root '" << locationRootDir << "'\n";
			}
		}
	}

	if (!matchedLocation.empty()) {
		std::string relativePath = resolvedPath.substr(matchedLocation.size());
		if (relativePath.empty() || relativePath[0] != '/')
			relativePath = "/" + relativePath;
		std::cout << "[DEBUG] Final path: '" << locationRootDir + relativePath << "'\n";
		return locationRootDir + relativePath;
	}

	// Check for alias mapping
	for (const auto& [locationPrefix, aliasDir] : _aliasDirectories) {
		if (resolvedPath.compare(0, locationPrefix.size(), locationPrefix) == 0) {
			std::string relativePath = resolvedPath.substr(locationPrefix.size());
			return aliasDir + relativePath;
		}
	}
	std::cout << "[DEBUG] No special location found, using default: '" << rootDir + resolvedPath << "'\n";
	return rootDir + resolvedPath;
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
    std::string rawPath = httpRequest.getPath();
    std::string decodedPath = urlDecode(rawPath);

    std::cout << "[DEBUG] Request Path: " << rawPath << "\n";
    std::cout << "[DEBUG] Decoded Path: " << decodedPath << "\n";

    bool methodAllowed = false;
    std::string matchedLocation = "";

    // Find the longest matching location for the path
    for (const auto& [location, allowedMethods] : _allowedMethods) {
        if (decodedPath.find(location) == 0 && location.length() > matchedLocation.length()) {
            matchedLocation = location;
            methodAllowed = (std::find(allowedMethods.begin(), allowedMethods.end(), method) != allowedMethods.end());
        }
    }

    // If no matching location is found, assume the method is allowed (or handle as needed)
    if (matchedLocation.empty()) {
        methodAllowed = true; // Default behavior if no location-specific rules are defined
    }

    // If the method is not allowed, return a 405 Method Not Allowed response
    if (!methodAllowed) {
        std::cout << "[DEBUG] Method " << method << " not allowed for path: " << decodedPath << "\n";
        return generateMethodNotAllowedResponse();
    }

    // Handle explicit redirection codes (e.g. /301 or /302 in the URL)
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

    for (const auto& [location, redirection] : _redirections) {
        if (rawPath.find(location) == 0 || decodedPath.find(location) == 0) {
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

    // Check if the request path matches a CGI location
    if (rawPath.find("/cgi-bin/") == 0) {
        std::string scriptPath = rawPath.substr(0, rawPath.find('?'));
        std::string queryString = rawPath.substr(rawPath.find('?') + 1);
        std::string requestBody = httpRequest.getBody();
        return _cgiHandler.executeCGI(scriptPath, method, queryString, requestBody);
    }

    // Determine the root directory from _serverConfig, defaulting to "./www"
    std::string rootDir = "./www"; // Default root directory
    auto itServer = _serverConfig.find("root");
    if (itServer != _serverConfig.end()) {
        rootDir = itServer->second;
    }

    // Handle POST requests (uploads and form submissions).
    if (method == "POST") {
        std::string contentType = httpRequest.getContentTypeFromHeaders("Content-Type");
        if (contentType.empty())
            contentType = "text/plain";
        std::string serverName = httpRequest.getHeader("Host");
        if (serverName.empty())
            serverName = "default";

        // Special handling for upload paths.
        if (rawPath == "/upload" || rawPath.find("/upload/") == 0) {
            // Check if it's a multipart/form-data request
            if (contentType.find("multipart/form-data") != std::string::npos) {
                // Extract the boundary from the Content-Type header
                size_t boundaryPos = contentType.find("boundary=");
                if (boundaryPos == std::string::npos) {
                    return generateErrorResponse(400, "Missing boundary in Content-Type");
                }
                std::string boundary = contentType.substr(boundaryPos + 9);
                boundary = "--" + boundary;

                // Get the request body
                std::string body = httpRequest.getBody();

                // Check the Content-Length before processing
                std::string contentLengthStr = httpRequest.getHeader("Content-Length");
                // Process the upload if size check passes
                return generateSuccessResponse("Files uploaded successfully");
            }
        }

        // For all other POST requests, delegate to generatePostResponse
        return generatePostResponse(httpRequest.getBody(), contentType, serverName);
    }

    // Resolve the file path for non-POST requests.
    std::string filePath = resolveFilePath(decodedPath, rootDir);
    if (filePath.empty())
        return generateErrorResponse(400, "Invalid path");

    std::cout << "[DEBUG] Resolved File Path: " << filePath << "\n";

    // Check if the resolved path is a directory.
    struct stat st;
    if (stat(filePath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        std::cout << "[DEBUG] Path is a directory\n";

        // Check autoindex configuration.
        bool autoindexEnabled = false;
        std::string matchedLocation = "";
        std::cout << "[DEBUG] Autoindex Configuration:\n";
        for (const auto& [loc, enabled] : _autoindexConfig) {
            std::cout << "  - Location: '" << loc << "', Autoindex: "
                    << (enabled ? "on" : "off") << "\n";
        }
        for (const auto& locationPair : _autoindexConfig) {
            const std::string& loc = locationPair.first;
            if (!loc.empty() && rawPath.find(loc) == 0) {
                if (loc.length() > matchedLocation.length()) {
                    matchedLocation = loc;
                    autoindexEnabled = locationPair.second;
                }
            }
        }
        std::cout << "[DEBUG] Matched Location: " << matchedLocation
                << ", Autoindex: " << (autoindexEnabled ? "on" : "off") << "\n";

        if (autoindexEnabled) {
            return generateDirectoryListing(filePath, decodedPath);
        } else {
            // Attempt to serve index.html.
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

    // Handle specific HTTP methods
    if (method == "GET") {
        return generateGetResponse(filePath);
    } else if (method == "DELETE") {
        return generateDeleteResponse(filePath);
    } else {
        // Handle unsupported methods
        return generateMethodNotAllowedResponse();
    }
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
	(void)serverName; // to remove

	std::cout << "[DEBUG] Processing POST request" << std::endl;
	std::cout << "[DEBUG] Content-Type: " << contentType << std::endl;
	std::cout << "[DEBUG] Request body size: " << requestBody.size() << " bytes" << std::endl;

	// Get upload directory from server configuration
	std::string uploadDir = "./www/html/upload";
	auto it = _serverConfig.find("upload_dir");
	if (it != _serverConfig.end()) {
		uploadDir = it->second;
	}

	// Create upload directory if it doesn't exist
	if (!FileUtils::createDirectoryIfNotExists(uploadDir)) {
		return generateErrorResponse(500, "Server configuration error");
	}
	// Handle multipart/form-data (file uploads)
	if (contentType.find("multipart/form-data") != std::string::npos) {
		return handleMultipartUpload(requestBody, contentType, uploadDir);
	}
	// Handle application/x-www-form-urlencoded
	else if (contentType.find("application/x-www-form-urlencoded") != std::string::npos) {
		return handleFormUrlEncodedUpload(requestBody, uploadDir);
	}
	// Handle text/plain
	else if (contentType.find("text/plain") != std::string::npos) {
		return handleTextUpload(requestBody, uploadDir);
	}
	// Unsupported content type
	else {
		std::cerr << "[ERROR] Unsupported content type: " << contentType << std::endl;
		std::cout << "Received Content-Type: " << contentType << std::endl;
		return generateErrorResponse(415, "Unsupported Media Type");
	}
}

std::string webServer::handleMultipartUpload(const std::string& requestBody,
    const std::string& contentType,
    const std::string& uploadDir)
{
    std::cout << "[DEBUG] Processing multipart/form-data upload" << std::endl;

    // Extract boundary from Content-Type header
    std::string boundary;
    size_t boundaryPos = contentType.find("boundary=");
    if (boundaryPos == std::string::npos) {
        std::cerr << "[ERROR] No boundary found in Content-Type" << std::endl;
        return generateErrorResponse(400, "Missing boundary in multipart/form-data");
    }

    boundary = contentType.substr(boundaryPos + 9);
    if (boundary.front() == '"' && boundary.back() == '"') {
        boundary = boundary.substr(1, boundary.size() - 2);
    }
    std::cout << "[DEBUG] Boundary: " << boundary << std::endl;

    // Full boundary string
    std::string fullBoundary = "--" + boundary;

    // Find the first boundary
    size_t boundaryStart = requestBody.find(fullBoundary);
    if (boundaryStart == std::string::npos) {
        std::cerr << "[ERROR] Could not find boundary in request body" << std::endl;
        return generateErrorResponse(400, "Malformed multipart/form-data");
    }

    // Find the headers section
    size_t headersStart = boundaryStart + fullBoundary.length();
    if (headersStart >= requestBody.size()) {
        std::cerr << "[ERROR] Headers section not found" << std::endl;
        return generateErrorResponse(400, "Malformed multipart/form-data");
    }

    // Skip CRLF after boundary
    if (requestBody[headersStart] == '\r' && headersStart + 1 < requestBody.size() && requestBody[headersStart + 1] == '\n') {
        headersStart += 2;
    }

    // Find the end of headers
    size_t headersEnd = requestBody.find("\r\n\r\n", headersStart);
    if (headersEnd == std::string::npos) {
        std::cerr << "[ERROR] End of headers not found" << std::endl;
        return generateErrorResponse(400, "Malformed multipart/form-data");
    }

    // Extract headers
    std::string headers = requestBody.substr(headersStart, headersEnd - headersStart);
    std::cout << "[DEBUG] Headers: " << headers << std::endl;

    // Extract filename (or generate a default name if missing)
    std::string filename = "uploaded_file_" + getCurrentTimeString() + ".bin";  // Default name
    size_t filenamePos = headers.find("filename=\"");
    if (filenamePos != std::string::npos) {
        size_t start = filenamePos + 10;
        size_t end = headers.find("\"", start);
        if (end != std::string::npos) {
            filename = sanitizeFilename(headers.substr(start, end - start));
            std::cout << "[DEBUG] Filename: " << filename << std::endl;
        }
    }

    // Content starts after headers
    size_t contentStart = headersEnd + 4; // Skip double CRLF

    // Find the next boundary
    size_t nextBoundary = requestBody.find(fullBoundary, contentStart);
    if (nextBoundary == std::string::npos) {
        std::cerr << "[ERROR] Closing boundary not found" << std::endl;
        return generateErrorResponse(400, "Malformed multipart/form-data");
    }

    // Content ends before next boundary
    size_t contentEnd = nextBoundary;
    if (contentEnd > 2 && requestBody[contentEnd - 2] == '\r' && requestBody[contentEnd - 1] == '\n') {
        contentEnd -= 2;
    }

    // Extract content (file data)
    std::string content = requestBody.substr(contentStart, contentEnd - contentStart);
    std::cout << "[DEBUG] Content size: " << content.size() << " bytes" << std::endl;

    // Save file to the upload directory
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


std::string webServer::handleFormUrlEncodedUpload(const std::string& requestBody,
	const std::string& uploadDir)
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

std::string webServer::generateDeleteResponse(const std::string& filePath)
{
	std::string adjustedFilePath;
	const std::string marker = "/upload/";
	size_t pos = filePath.find(marker);

	if (pos != std::string::npos) {
		std::string filename = filePath.substr(pos + marker.length());
		adjustedFilePath = "./www/html/upload/" + filename;
	} else {
		adjustedFilePath = filePath;
	}

	if (!FileUtils::deleteFile(adjustedFilePath))
	{
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

void handleTimeout(int signal) {
	if (signal == SIGALRM) {
		timeoutOccurred = 1;
	}
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

    // Debug: Print out the stored autoindex configuration
    std::cout << "[DEBUG] Autoindex Configuration set in webServer:\n";
    for (const auto& [location, autoindex] : _autoindexConfig)
    {
        std::cout << "Location: " << location << ", Autoindex: " << (autoindex ? "on" : "off") << "\n";
    }
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
	std::cout << "[DEBUG] Set client_max_body_size for server '" << serverName
			  << "' to " << size << " bytes\n";
}

size_t webServer::getClientMaxBodySize(const std::string& serverName) const {
	// Try exact match first
	auto it = _clientMaxBodySizes.find(serverName);
	if (it != _clientMaxBodySizes.end()) {
		std::cout << "[DEBUG] Found client_max_body_size for server '" << serverName
				 << "': " << it->second << " bytes\n";
		return it->second;
	}

	// Try without port number
	size_t colonPos = serverName.find(":");
	if (colonPos != std::string::npos) {
		std::string nameWithoutPort = serverName.substr(0, colonPos);
		it = _clientMaxBodySizes.find(nameWithoutPort);
		if (it != _clientMaxBodySizes.end()) {
			std::cout << "[DEBUG] Found client_max_body_size for server '" << nameWithoutPort
					 << "': " << it->second << " bytes\n";
			return it->second;
		}
	}

	// Try just "localhost" as fallback
	it = _clientMaxBodySizes.find("localhost");
	if (it != _clientMaxBodySizes.end()) {
		std::cout << "[DEBUG] Found client_max_body_size for 'localhost': "
				 << it->second << " bytes\n";
		return it->second;
	}

	std::cout << "[DEBUG] Using default client_max_body_size for server '" << serverName
			 << "': 1048576 bytes\n";
	return 1048576;
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
std::string webServer::handleTextUpload(const std::string& requestBody,
	const std::string& uploadDir)
{
	std::cout << "[DEBUG] Processing plain text upload" << std::endl;

	// Parse headers to extract filename (if available)
	std::unordered_map<std::string, std::string> headers = parseHeaders(requestBody);
	std::string filename = "text_data_" + getCurrentTimeString() + ".txt";

	if (headers.find("Content-Disposition") != headers.end()) {
		std::string disposition = headers["Content-Disposition"];
		size_t filenamePos = disposition.find("filename=\"");
		if (filenamePos != std::string::npos) {
			size_t start = filenamePos + 10;
			size_t end = disposition.find("\"", start);
			if (end != std::string::npos) {
				filename = sanitizeFilename(disposition.substr(start, end - start));
				std::cout << "[DEBUG] Original filename extracted: " << filename << std::endl;
			}
		}
	}

	// Save file
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

std::string webServer::readFullRequest(int clientSocket) {
    std::string request;
    request.reserve(10 * 1024 * 1024);
    char buffer[4096 * 4];
    ssize_t bytesRead;
    bool headersComplete = false;
    size_t contentLength = 0;
    size_t totalRead = 0;

    // Read headers
    while (!headersComplete) {
        bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead < 0) {
            std::cerr << "[ERROR] recv() failed" << std::endl;
            return "";
        } else if (bytesRead == 0) {
            if (request.empty()) {
                std::cerr << "[ERROR] Connection closed before any data received" << std::endl;
                return "";
            }
            break;
        }
        request.append(buffer, bytesRead);

        // Check for end of headers
        size_t headersEnd = request.find("\r\n\r\n");
        if (headersEnd != std::string::npos) {
            headersComplete = true;
            size_t headerEndIndex = headersEnd + 4;

            // Parse headers
            std::string headerSection = request.substr(0, headersEnd);
            auto headers = parseHeaders(headerSection);

            // Handle "Expect: 100-continue" if present
            auto expectIt = headers.find("Expect");
            if (expectIt != headers.end() && expectIt->second == "100-continue") {
                const char* continueResponse = "HTTP/1.1 100 Continue\r\n\r\n";
                send(clientSocket, continueResponse, strlen(continueResponse), 0);
            }

            // Get content length
            contentLength = getContentLength(headers);
            totalRead = request.size() - headerEndIndex;

            // Sprawdzenie, czy Host istnieje
            size_t maxBodySize = 0;
            auto hostIt = headers.find("Host");
            if (hostIt != headers.end()) {
                maxBodySize = getClientMaxBodySize(hostIt->second);
            }

            // Check if the request size exceeds the maximum allowed size
            if (contentLength > maxBodySize) {
                std::cerr << "[ERROR] Request size (" << contentLength
                          << " bytes) exceeds client_max_body_size (" << maxBodySize
                          << " bytes)\n";
                std::string response = generateErrorResponse(413, "Payload Too Large");
                Socket clientSock(clientSocket);
                sendResponse(clientSock, response);
                return "";
            }
        }
    }

    // Read the body
    while (totalRead < contentLength) {
        size_t toRead = std::min(sizeof(buffer), contentLength - totalRead);
        bytesRead = recv(clientSocket, buffer, toRead, 0);
        if (bytesRead < 0) {
            std::cerr << "[ERROR] recv() failed" << std::endl;
            continue;
        } else if (bytesRead == 0) {
            std::cerr << "[ERROR] Connection closed before full body received" << std::endl;
            return "";
        }
        request.append(buffer, bytesRead);
        totalRead += bytesRead;
    }

    // Debug the full request
    std::cout << "[DEBUG] Full request received, size: " << request.size() << " bytes" << std::endl;

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
void webServer::setRootDirectories(const std::map<std::string, std::string>& rootDirectories) {
	_rootDirectories = rootDirectories;
}

std::vector<std::string> webServer::getAllowedMethods(const std::string& location) const {
	auto it = _allowedMethods.find(location);
	if (it != _allowedMethods.end()) {
		return it->second;
	}
	return {}; // Return an empty vector if no methods are specified
}

void webServer::setAllowedMethods(const std::map<std::string, std::vector<std::string>>& allowedMethods) {
	_allowedMethods = allowedMethods;
}

CGIHandler& webServer::getCGIHandler() {
    return _cgiHandler;
}
