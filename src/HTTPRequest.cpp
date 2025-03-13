/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTPRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: pwojnaro <pwojnaro@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/07 18:49:05 by pwojnaro          #+#    #+#             */
/*   Updated: 2025/03/12 15:31:47 by pwojnaro         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HTTPRequest.hpp"
#include <iostream>
#include <algorithm>


HTTPRequest::HTTPRequest(const std::string& rawRequest) 
{
    this->rawRequest = rawRequest;
    parseRequest(rawRequest);
}

void HTTPRequest::parseRequest(const std::string& rawRequest) {
    std::istringstream requestStream(rawRequest);
    requestStream >> method >> path >> version;

    std::string line;
    std::getline(requestStream, line); // Skip the first line (already parsed)

    // Parse headers until empty line (which marks end of headers)
    while (std::getline(requestStream, line) && !line.empty()) {
        // Remove trailing \r if present
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }

        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            headers[key] = value;
        }
    }

    // Extract body based on Content-Length
    size_t headerEnd = rawRequest.find("\r\n\r\n");
    if (headerEnd != std::string::npos) {
        body = rawRequest.substr(headerEnd + 4); // Skip \r\n\r\n

        // If Content-Length is present, ensure the body is the correct size
        std::string contentLengthStr = getHeader("Content-Length");
        if (!contentLengthStr.empty()) {
            size_t contentLength = std::stoul(contentLengthStr);
            if (body.size() > contentLength) {
                body = body.substr(0, contentLength); // Truncate to Content-Length
            } else if (body.size() < contentLength) {
                std::cerr << "[ERROR] Incomplete body: Expected " << contentLength
                          << " bytes, got " << body.size() << " bytes" << std::endl;
                body.clear(); // Clear body if incomplete
            }
        }
    } 
	else 
	{
        std::cout << "[DEBUG] No body found in request" << std::endl;
    }

    std::cout << "[DEBUG] Extracted body size: " << body.size() << " bytes" << std::endl;
    std::cout << "[DEBUG] Body: " << body << std::endl;
}

std::string HTTPRequest::getMethod() const
{
    return method;
}

std::string HTTPRequest::getPath() const
{
    return path;
}

std::string HTTPRequest::getHeader(const std::string& key) const
{
    auto it = headers.find(key);
    return (it != headers.end()) ? it->second : "";
}

std::string HTTPRequest::getBody() const
{
    return body;
}

std::string HTTPRequest::getRawRequest() const
{
    return rawRequest;
}