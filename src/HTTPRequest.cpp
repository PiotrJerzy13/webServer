/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTPRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: pwojnaro <pwojnaro@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/07 18:49:05 by pwojnaro          #+#    #+#             */
/*   Updated: 2025/03/09 16:45:22 by pwojnaro         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HTTPRequest.hpp"
#include <iostream>
#include <algorithm>

HTTPRequest::HTTPRequest(const std::string& rawRequest)
{
    this->rawRequest = rawRequest; // Store the raw request
    parseRequest(rawRequest);
}

/**
 * Parses a raw HTTP request string into its components:
 * - Request line (method, path, version)
 * - Headers (key-value pairs)
 * - Body (if present)
 * The >> operator is used to extract formatted input from a stream 
 * std::istringstream allows you to treat a string as an input stream.
 */
void HTTPRequest::parseRequest(const std::string& rawRequest)
{
    std::istringstream requestStream(rawRequest);
    requestStream >> method >> path >> version;

    std::string line;
    std::getline(requestStream, line);

    while (std::getline(requestStream, line) && line != "\r") //In HTTP requests, "\r" is part of the line-ending sequence.
	{
        size_t colonPos = line.find(':'); // Looking for first ":" usualy at Host:
        if (colonPos != std::string::npos) // Creates headers[key] = value stored in headers map
		{
            std::string key = line.substr(0, colonPos); // Extract the key (before the colon)
            std::string value = line.substr(colonPos + 1);// Extract the value (after the colon)
            value.erase(0, value.find_first_not_of(" \t"));// Trim leading whitespace
            value.erase(value.find_last_not_of("\r\n") + 1);// Trim trailing whitespace
            headers[key] = value; // Store the key-value pair in the headers map
        }
    }
	
    size_t headerEnd = rawRequest.find("\r\n\r\n");
    if (headerEnd != std::string::npos) //npos special constant defined in the std::string class. It represents a value that indicates "not found" or "no position."
	{
        body = rawRequest.substr(headerEnd + 4);
    }
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