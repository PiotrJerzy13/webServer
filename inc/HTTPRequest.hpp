/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTPRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: anamieta <anamieta@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/07 18:48:10 by pwojnaro          #+#    #+#             */
/*   Updated: 2025/03/23 15:30:15 by anamieta         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <string>
#include <unordered_map>
#include <sstream>
#include "Colors.hpp"

class HTTPRequest {
	public:
		HTTPRequest(const std::string& rawRequest);
		std::string getMethod() const;
		std::string getPath() const;
		std::string getHeader(const std::string& key) const;
		std::string getBody() const;
		std::string getRawRequest() const;
		std::string getContentTypeFromHeaders(const std::string& fullRequest);

	private:
		std::string method;
		std::string path;
		std::string version;
		std::unordered_map<std::string, std::string> headers; // Holds key:value pairs from the header, e.g., ["Host"] = "localhost:8080"
		std::string body;
		std::string rawRequest;

		void parseRequest(const std::string& rawRequest);
	};
