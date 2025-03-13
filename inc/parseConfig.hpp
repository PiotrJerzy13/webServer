/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parseConfig.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eleni <eleni@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/12 17:50:48 by eleni             #+#    #+#             */
/*   Updated: 2025/03/13 10:16:53 by eleni            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <stack>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <string>
#include <iostream>
#include <fstream>
#include <map>

class parseConfig
{
	public:

		struct CGIConfig 
		{
    	std::string cgiPass; // Path to the CGI interpreter (e.g., /usr/bin/python3)
    	std::string scriptFilename; // SCRIPT_FILENAME template
    	std::string pathInfo; // PATH_INFO template
    	std::string queryString; // QUERY_STRING template
    	std::string requestMethod; // REQUEST_METHOD template
		};
		
		std::map<std::string, CGIConfig> _cgiConfig;
		parseConfig() : _clientMaxBodySize(0) {}
		size_t getClientMaxBodySize() const { return _clientMaxBodySize; }

		std::string _mainString;
		std::stack<std::string> _blocks;
		std::unordered_multimap<std::string, std::string> _parsingServer;
		std::unordered_multimap<std::string, std::vector<std::string>> _parsingLocation;
		std::string _location;
		std::map<std::string, bool> _autoindexConfig;
		std::map<std::string, std::string> _redirections;
		std::vector<std::string> _methods;
		const std::map<std::string, std::string>& getRedirections() const;
		const std::map<std::string, std::string>& getServerNames() const;
		void parseClientMaxBodySize(const std::string& line);
		void parse(const std::string& filename);

	class SyntaxErrorException : public std::exception
	{
		public:
			const char* what() const throw();
	};
	
	private:
		size_t _clientMaxBodySize;
		std::string _currentServerBlock;
		std::map<std::string, std::string> _serverNames;
		void splitMaps(std::string& line, int& brackets);
		void trimServer(const std::string& line);
		std::string trimLocation(const std::string& line);
		std::string trim(const std::string& line);
		void fillLocationMap(std::string& line, const std::string& location);
		const std::map<std::string, bool>& getAutoindexConfig() const;
		const parseConfig::CGIConfig& getCGIConfig(const std::string& location) const;
	
};