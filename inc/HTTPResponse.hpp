/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTPResponse.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: anamieta <anamieta@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/07 18:49:43 by pwojnaro          #+#    #+#             */
/*   Updated: 2025/03/23 15:29:07 by anamieta         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <string>
#include <sstream>
#include "Colors.hpp"

class HTTPResponse {
	public:
		HTTPResponse() : statusCode(200), contentType("text/plain"), body("") {}
		HTTPResponse(int statusCode, const std::string& contentType, const std::string& body);
		static std::pair<std::string, std::string> getDefaultErrorPage(int errorCode);
		static std::string getContentType(const std::string& filePath);
		std::string generateResponse() const;

	private:
		int statusCode;
		std::string contentType;
		std::string body;
};
