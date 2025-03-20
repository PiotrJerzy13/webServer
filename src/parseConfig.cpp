/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parseConfig.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: pwojnaro <pwojnaro@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/12 18:29:11 by eleni             #+#    #+#             */
/*   Updated: 2025/03/20 17:03:03 by pwojnaro         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "parseConfig.hpp"
#include <map>

#include "parseConfig.hpp"
#include <sstream>
#include <iostream>
#include <algorithm>

// ------------------------------------------------------------------------
// Constructor and Parsing Methods
// ------------------------------------------------------------------------
void parseConfig::parse(const std::string& filename) {
    std::string line;
    int brackets = 0;
    std::istringstream file(filename);

    std::cout << "DEBUG: Parsing configuration file..." << std::endl;

    while (std::getline(file, line)) {
        std::cout << "DEBUG: Processing line: " << line << std::endl;
        splitMaps(line, brackets);
    }

    if (brackets != 0) {
        std::cerr << "DEBUG: Mismatched brackets in configuration file." << std::endl;
        throw SyntaxErrorException();
    }

    std::cout << "DEBUG: Configuration file parsed successfully." << std::endl;
}

// ------------------------------------------------------------------------
// String Trimming Helpers
// ------------------------------------------------------------------------
std::string parseConfig::trim(const std::string& line) 
{
    size_t first = line.find_first_not_of(" \t");
    size_t last = line.find_last_not_of(" \t");

    if (first == std::string::npos || last == std::string::npos)
        return "";

    std::string final = line.substr(first, (last - first + 1));

    if (!final.empty() && final[final.size() - 1] == ';')
        final = final.substr(0, final.size() - 1);

    return final;
}

std::string parseConfig::trimLocation(const std::string& line) {
    if (line.find("location") != std::string::npos && line.find('{') != std::string::npos) {
        size_t posBracket = line.find('{');
        std::string temp = trim(line.substr(0, posBracket));

        size_t pos = temp.find_first_of(" \t");
        return temp.substr(pos + 1);
    }
    return "";
}

// ------------------------------------------------------------------------
// Configuration Parsing (Server and Location)
// ------------------------------------------------------------------------

// void parseConfig::splitMaps(std::string& line, int& brackets) {
//     if (line.find('{') != std::string::npos) {
//         brackets++;
//     } else if (line.find('}') != std::string::npos) {
//         if (brackets == 4) {
//             auto it = _parsingLocation.find(_location);
//             if (it != _parsingLocation.end())
//                 it->second.push_back("}");
//             else
//                 _parsingLocation.insert({_location, {"}"}});
//         }
//         brackets--;
//     }

//     if (line.find("server ") != std::string::npos || line.find("server\t") != std::string::npos) {
//         _blocks.push("server");
//         _currentServerBlock = "server" + std::to_string(_serverNames.size() + 1);
//         std::cout << "DEBUG: Found server block: " << _currentServerBlock << std::endl;
//         return;
//     } else if (line.find("location") != std::string::npos) {
//         if (brackets > 3)
//             throw SyntaxErrorException();

//         _blocks.push("location");
//         _location = trimLocation(line);
//         _parsingLocation.insert({_location, std::vector<std::string>()});
//         std::cout << "DEBUG: Found location block: " << _location << std::endl;
//         return;
//     } else if (!_blocks.empty() && _blocks.top() == "server") {
//         std::cout << "DEBUG: Parsing server directive: " << line << std::endl;
//         trimServer(line);
//     } else if (!_blocks.empty() && _blocks.top() == "location") {
//         std::cout << "DEBUG: Parsing location directive: " << line << std::endl;
//         fillLocationMap(line, _location);
//     }
// }

void parseConfig::trimServer(const std::string& line) 
{
    std::string trimmedLine = trim(line);
    if (trimmedLine.empty())
        return;

    size_t pos = trimmedLine.find_first_of(" \t");
    if (pos != std::string::npos) {
        std::string key = trim(trimmedLine.substr(0, pos));
        std::string value = trim(trimmedLine.substr(pos + 1));

        if (key == "server_name") {
            _serverNames[_currentServerBlock] = value;
        } else if (key == "client_max_body_size") {
            parseClientMaxBodySize(value);
        } else {
            _parsingServer.insert({key, value});
        }
    }
}

std::pair<std::string, std::string> parseConfig::parseKeyValue(const std::string& line) {
	size_t pos = line.find_first_of(" \t");
	if (pos == std::string::npos) {
		return std::make_pair("", "");
	}
	std::string key = trim(line.substr(0, pos)); // Now trim is accessible
	std::string value = trim(line.substr(pos + 1)); // Now trim is accessible
	return std::make_pair(key, value);
}

void parseConfig::fillLocationMap(std::string& line, const std::string& location) {
    std::string trimmedLine = trim(line);
    if (trimmedLine.empty())
        return;

    size_t pos = trimmedLine.find_first_of(" \t");
    if (pos != std::string::npos) {
        std::string key = trim(trimmedLine.substr(0, pos));
        std::string value = trim(trimmedLine.substr(pos + 1));

        std::cout << "DEBUG: Parsing location directive - Key: " << key << ", Value: " << value << std::endl;

        if (key == "root") {
            _rootDirectories[location] = value;
            std::cout << "DEBUG: Set root for location '" << location << "': " << value << std::endl;
        } else if (key == "alias") {
            _aliasDirectories[location] = value;
            std::cout << "DEBUG: Set alias for location '" << location << "': " << value << std::endl;
        } else if (key == "autoindex") {
            _autoindexConfig[location] = (value == "on");
            std::cout << "DEBUG: Set autoindex for location '" << location << "': " << (value == "on" ? "on" : "off") << std::endl;
        } else if (key == "return") {
            _redirections[location] = value;
            std::cout << "DEBUG: Set redirection for location '" << location << "': " << value << std::endl;
        } else if (key == "methods") {
            std::istringstream methodStream(value);
            std::string method;
            while (methodStream >> method) {
                _allowedMethods[location].push_back(method);
                std::cout << "DEBUG: Added method '" << method << "' for location '" << location << "'" << std::endl;
            }
        } else if (key == "cgi_pass") {
            _cgiConfig[location].cgiPass = value;
            std::cout << "DEBUG: Set cgi_pass for location '" << location << "': " << value << std::endl;
        }
    }
}
// ------------------------------------------------------------------------
// CGI Configuration Parsing
// ------------------------------------------------------------------------
const parseConfig::CGIConfig& parseConfig::getCGIConfig(const std::string& location) const 
{
    static CGIConfig emptyConfig;
    auto it = _cgiConfig.find(location);
    return (it != _cgiConfig.end()) ? it->second : emptyConfig;
}

// ------------------------------------------------------------------------
// Utility Functions
// ------------------------------------------------------------------------
void parseConfig::parseClientMaxBodySize(const std::string& value) 
{
    std::string trimmedValue = trim(value);
    if (trimmedValue.empty())
        throw SyntaxErrorException();

    size_t numericPart = 0;
    std::string unit;

    size_t unitPos = trimmedValue.find_first_not_of("0123456789");
    if (unitPos == std::string::npos) {
        numericPart = std::stoi(trimmedValue);
    } else {
        numericPart = std::stoi(trimmedValue.substr(0, unitPos));
        unit = trim(trimmedValue.substr(unitPos));
        std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);
    }

    size_t size = numericPart;
    if (unit == "k" || unit == "kb")
        size *= 1024;
    else if (unit == "m" || unit == "mb")
        size *= 1024 * 1024;
    else if (unit == "g" || unit == "gb")
        size *= 1024 * 1024 * 1024;
    else if (!unit.empty() && unit != "b")
        throw SyntaxErrorException();

    _clientMaxBodySize[_currentServerBlock] = size;
}

// ------------------------------------------------------------------------
// Getters for Configuration Data
// ------------------------------------------------------------------------
const std::map<std::string, std::string>& parseConfig::getRedirections() const 
{
    return _redirections;
}

const std::map<std::string, std::string>& parseConfig::getServerNames() const 
{
    return _serverNames;
}

const std::map<std::string, std::string>& parseConfig::getRootDirectories() const 
{
    return _rootDirectories;
}

const std::map<std::string, bool>& parseConfig::getAutoindexConfig() const 
{
    return _autoindexConfig;
}

size_t parseConfig::getClientMaxBodySize(const std::string& serverBlock) const 
{
    auto it = _clientMaxBodySize.find(serverBlock);
    return (it != _clientMaxBodySize.end()) ? it->second : 1048576;
}

const std::map<std::string, std::vector<std::string>>& parseConfig::getAllowedMethods() const 
{
    return _allowedMethods;
}

const std::map<std::string, parseConfig::CGIConfig>& parseConfig::getCGIConfigs() const 
{
    return _cgiConfig;
}

// ------------------------------------------------------------------------
// Exception Handling
// ------------------------------------------------------------------------
const char* parseConfig::SyntaxErrorException::what() const throw() 
{
    return "Exception: Brackets or ';' is missing";
}

const std::map<std::string, std::string>& parseConfig::getErrorPages() const
 {
 	return _errorPages;
 }
 
 const std::string& parseConfig::getIndex() const
 {
 	return _index;
 }