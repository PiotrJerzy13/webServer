/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parseConfig.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: pwojnaro <pwojnaro@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/12 17:50:48 by eleni             #+#    #+#             */
/*   Updated: 2025/03/15 20:13:30 by pwojnaro         ###   ########.fr       */
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

class parseConfig {
	public:
		struct CGIConfig {
			std::string cgiPass; // Path to the CGI interpreter (e.g., /usr/bin/python3)
			std::string scriptFilename; // SCRIPT_FILENAME template
			std::string pathInfo; // PATH_INFO template
			std::string queryString; // QUERY_STRING template
			std::string requestMethod; // REQUEST_METHOD template
		};
	
		std::map<std::string, CGIConfig> _cgiConfig;
	
		parseConfig() {} // Default constructor
	
		size_t getClientMaxBodySize(const std::string& serverName) const {
			auto it = _clientMaxBodySize.find(serverName);
			if (it != _clientMaxBodySize.end()) {
				return it->second; // Return the size for the specific server
			}
			return 1048576; // Default value if serverName is not found
		}
	
		std::string _mainString;
		std::stack<std::string> _blocks;
		std::unordered_multimap<std::string, std::string> _parsingServer;
		std::unordered_multimap<std::string, std::vector<std::string>> _parsingLocation;
		std::map<std::string, bool> _autoindexConfig;
		std::string _location;
		std::vector<std::string> _methods;
	
		const std::map<std::string, std::string>& getRedirections() const;
		const std::map<std::string, std::string>& getServerNames() const;
		void parseClientMaxBodySize(const std::string& value);
		void parse(const std::string& filename);
		const std::map<std::string, std::string>& getRootDirectories() const {
			return _rootDirectories;
		}
	
		class SyntaxErrorException : public std::exception {
		public:
			const char* what() const throw();
		};
	
	private:
		std::map<std::string, size_t> _clientMaxBodySize; // Map of server names to sizes
		std::string _currentServerBlock;
		std::map<std::string, std::string> _serverNames;
		void splitMaps(std::string& line, int& brackets);
		void trimServer(const std::string& line);
		std::string trimLocation(const std::string& line);
		std::string trim(const std::string& line);
		void fillLocationMap(std::string& line, const std::string& location);
		const std::map<std::string, bool>& getAutoindexConfig() const;
		const parseConfig::CGIConfig& getCGIConfig(const std::string& location) const;
		std::map<std::string, std::string> _rootDirectories; // For "root"
		std::map<std::string, std::string> _aliasDirectories; // For "alias"
		std::map<std::string, std::string> _redirections; // For "return"
		std::map<std::string, std::vector<std::string>> _allowedMethods; // For "methods"
		std::map<std::string, std::vector<std::string>> _tryFiles; // For "try_files"
		std::map<std::string, std::string> _allow; // For "allow"
		std::map<std::string, std::string> _indexFiles; // For "index"
		std::map<std::string, std::string> _cgiPass; // For "cgi_pass"
		std::map<std::string, std::map<std::string, std::string>> _cgiParams;
	};