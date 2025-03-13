/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parseConfig.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eleni <eleni@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/12 18:29:11 by eleni             #+#    #+#             */
/*   Updated: 2025/03/13 10:39:39 by eleni            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "parseConfig.hpp"
#include <map>

std::string parseConfig::trim(const std::string& line)
{
	size_t first = line.find_first_not_of(" \t");
	size_t last = line.find_last_not_of(" \t");
	std::string final = line;

	if (first == std::string::npos || last == std::string::npos)
		final = "";
	if (final.empty())
		return "";

	final = final.substr(first, (last - first + 1));

	if (!final.empty() && final[final.size() - 1] == ';')
        final = final.substr(0, final.size() - 1);

	return final;
}

void parseConfig::fillLocationMap(std::string& line, const std::string& location)
{
    std::string trimmedLine = trim(line);
    if (trimmedLine.empty())
        return;

    size_t pos = trimmedLine.find_first_of(" \t");
    if (pos != std::string::npos)
    {
        std::string key = trim(trimmedLine.substr(0, pos));
        std::string value = trim(trimmedLine.substr(pos + 1));

        // Store the key-value pair in the location map
        auto it = this->_parsingLocation.find(location);
        if (it != this->_parsingLocation.end()) 
            it->second.push_back(key + " " + value);
        else 
            this->_parsingLocation.insert({location, {key + " " + value}});

        // Handle autoindex directive
        if (key == "autoindex")
        {
            bool autoindex_value = (value == "on");
            _autoindexConfig[location] = autoindex_value;
            std::cout << "[DEBUG] autoindex for location '" << location 
                      << "' set to " << (autoindex_value ? "on" : "off") << std::endl;
        }
        // Handle redirection directive
        else if (key == "return")
        {
            // Split the value into status code and target URL
            size_t spacePos = value.find_first_of(" \t");
            if (spacePos != std::string::npos)
            {
                std::string statusCode = value.substr(0, spacePos);
                std::string targetUrl = value.substr(spacePos + 1);

                // Store the redirection as "status_code target_url"
                _redirections[location] = statusCode + " " + targetUrl;
                std::cout << "[DEBUG] Redirection for location '" << location 
                          << "' set to " << statusCode << " " << targetUrl << std::endl;
            }
            else
            {
                std::cerr << "[ERROR] Invalid return directive: " << value << std::endl;
            }
        }
		else if (key == "methods")
		{
			size_t spacePos = value.find_first_of(" \t");
			if (spacePos != std::string::npos)
			{
				std::istringstream methodStream(value);
				std::string method;
				
				while (methodStream >> method)
				{
					trim(method);
					_methods.push_back(method);
				}
			}
			else
			{
				std::cerr << "[ERROR] Invalid return directive: " << value << std::endl;
			}
		}
    }
}

std::string parseConfig::trimLocation(const std::string& line)
{
	std::string temp;

	if (line.find("location") != std::string::npos && line.find('{') != std::string::npos)
	{
		size_t posBracket = line.find('{');
		temp = line.substr(0, posBracket);
		// std::cout << temp << std::endl;

		temp = trim(temp);

		size_t pos = temp.find_first_of(" \t");
		std::string location = temp.substr(pos + 1);

		// std::cout << location << std::endl;
		
		return location;
	}
	return "";
}

void parseConfig::trimServer(const std::string& line)
{
    std::string trimmedline = trim(line);

    if (trimmedline.empty())
        return;

    size_t pos = trimmedline.find_first_of(" \t");
    if (pos != std::string::npos)
    {
        std::string key = trimmedline.substr(0, pos);
        std::string value = trimmedline.substr(pos + 1);

        key = trim(key);
        value = trim(value);

        if (key == "server_name")
        {
            _serverNames[_currentServerBlock] = value;
            std::cout << "[DEBUG] server_name for server block '" << _currentServerBlock 
                      << "' set to " << value << std::endl;
        }
        else if (key == "client_max_body_size")
        {
            try {
                parseClientMaxBodySize(value);
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Invalid client_max_body_size: " << value << "\n";
                throw;
            }
        }
        else
        {
            this->_parsingServer.insert({key, value});
        }
    }
}

void parseConfig::splitMaps(std::string& line, int& brackets)
{
    std::string location;

    if (line.find('{') != std::string::npos)
    {
        brackets++;
    }
    else if (line.find('}') != std::string::npos)
    {
        if (brackets == 4)
        {
            auto it = this->_parsingLocation.find(this->_location);
            if (it != this->_parsingLocation.end())
                it->second.push_back("}");
            else
                this->_parsingLocation.insert({this->_location, {"}"}});
        }
        brackets--;
    }

    if (line.find("server ") != std::string::npos || line.find("server\t") != std::string::npos)
    {
        this->_blocks.push("server");
        _currentServerBlock = "server" + std::to_string(_serverNames.size() + 1); // Assign a unique identifier
        return;
    }
    else if (line.find("location") != std::string::npos)
    {
        if (brackets > 3)
            throw SyntaxErrorException();
        this->_blocks.push("location");
        this->_location = trimLocation(line); // Update _location here
        std::cout << "[DEBUG] Parsing location block for: " << this->_location << "\n";
        this->_parsingLocation.insert({this->_location, std::vector<std::string>()});
        return;
    }
    else if (!this->_blocks.empty() && this->_blocks.top() == "server")
    {
        trimServer(line);
    }
    else if (!this->_blocks.empty() && this->_blocks.top() == "location")
    {
        fillLocationMap(line, this->_location); // Pass _location to fillLocationMap
    }
}

void parseConfig::parse(const std::string& filename)
{
	// std::ifstream file(filename);
	// if (!file.is_open())
	// {
	// 	std::cerr << "Error: Could not open configuration file: " << filename << std::endl;
	// }
	// else
	// {
		// std::cout << "File printed line by line ready to be parsed" << std::endl;
		std::string line;
		int brackets = 0;
		std::istringstream file(filename);
		while (std::getline(file, line))
		{
			splitMaps(line, brackets);
		}
		if (brackets != 0)
			throw SyntaxErrorException();
	// }

	// for (const auto& pair : _parsingServer)
	// {
	// 	std::cout << pair.first << ": " << pair.second << std::endl;
	// }

	
		std::cout << "Methods in the vector:" << std::endl;
	for (const auto& m : _methods) {
		std::cout << "." << m  << "." << std::endl;
	}
}

const char* parseConfig::SyntaxErrorException::what() const throw()
{
    return "Exception: Brackets or ';' is missing";
}
const std::map<std::string, bool>& parseConfig::getAutoindexConfig() const
{
    return _autoindexConfig;
}

const std::map<std::string, std::string>& parseConfig::getRedirections() const
{
    return _redirections;
}

const std::map<std::string, std::string>& parseConfig::getServerNames() const
{
    return _serverNames;
}

void parseConfig::parseClientMaxBodySize(const std::string& value)
{
    std::string trimmedValue = trim(value);

    if (trimmedValue.empty())
	{
        throw SyntaxErrorException();
    }

    size_t numericPart = 0;
    std::string unit;
    
    // Extract numeric part
    size_t unitPos = trimmedValue.find_first_not_of("0123456789");
    if (unitPos == std::string::npos)
	{ 
        // Only numbers found, no unit
        numericPart = std::stoi(trimmedValue);
    }
	else
	{
        numericPart = std::stoi(trimmedValue.substr(0, unitPos));
        unit = trimmedValue.substr(unitPos);
        unit = trim(unit); // Trim spaces

        // Convert unit to lowercase for case-insensitive comparison
        std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);
    }

    // Convert size based on unit
    size_t size = 0;
    if (unit.empty() || unit == "b") {
        size = numericPart; // bytes
    } else if (unit == "k" || unit == "kb") {
        size = numericPart * 1024; // kilobytes
    } else if (unit == "m" || unit == "mb") {
        size = numericPart * 1024 * 1024; // megabytes
    } else if (unit == "g" || unit == "gb") {
        size = numericPart * 1024 * 1024 * 1024; // gigabytes
    } else {
        throw SyntaxErrorException(); // Invalid unit
    }

    this->_clientMaxBodySize = size;
    std::cout << "[DEBUG] client_max_body_size set to " << size << " bytes\n";
}

const parseConfig::CGIConfig& parseConfig::getCGIConfig(const std::string& location) const
{
    static CGIConfig emptyConfig;
    auto it = _cgiConfig.find(location);
    if (it != _cgiConfig.end())
        return it->second;
    return emptyConfig;
}
