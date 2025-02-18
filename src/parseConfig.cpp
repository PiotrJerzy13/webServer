/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parseConfig.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: anamieta <anamieta@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/12 18:29:11 by eleni             #+#    #+#             */
/*   Updated: 2025/02/18 18:37:08 by anamieta         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "parseConfig.hpp"

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
		std::string key = trimmedLine.substr(0, pos);
		std::string value = trimmedLine.substr(pos + 1);

		key = trim(key);
		value = trim(value);

		this->_parsingLocation[location].push_back(key + " " + value);
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
		// std::cout << temp << std::endl;

		size_t pos = temp.find_first_of(" \t");
		std::string location = temp.substr(pos + 1);

		return location;
	}
	return "";
}


void parseConfig::trimServer(const std::string& line)
{
	std::string trimmedline = trim(line);

	if (trimmedline.empty())
		return ;
	// std::cout << trimmedline << std::endl;

	size_t pos = trimmedline.find_first_of(" \t");
	if (pos != std::string::npos)
	{
		std::string key = trimmedline.substr(0, pos);
		std::string value = trimmedline.substr(pos + 1);

		key = trim(key);
		value = trim(value);

		// std::cout << key << " : " << value << std::endl;

		this->_parsingServer.insert({key, value});

	}
}

void parseConfig::splitMaps(std::string& line, int& brackets)
{
	std::string location;

	if (line.find('{') != std::string::npos)
	{
			brackets++;
			// std::cout << brackets << std::endl;
	}
	else if (line.find('}') != std::string::npos)
	{
			brackets--;
			// std::cout << brackets << std::endl;
	}

	if (line.find("server ") != std::string::npos || line.find("server	") != std::string::npos)
	{
		this->_blocks.push("server");
		// std::cout << line << std::endl;
		return ;
	}
	else if (line.find("location") != std::string::npos)
	{
		if (brackets > 3)
			throw SyntaxErrorException();
		this->_blocks.push("location");
		location = trimLocation(line);

		return ;
	}
	else if (!this->_blocks.empty() && this->_blocks.top() =="server")
	{
		// std::cout << "Hi from Server" << std::endl;
		trimServer(line);
	}
	else if (!this->_blocks.empty() && this->_blocks.top() == "location")
	{
		fillLocationMap(line, location);
	}
}

void parseConfig::parse(const std::string& filename)
{
	std::ifstream file(filename);
	if (!file.is_open())
	{
		std::cerr << "Error: Could not open configuration file: " << filename << std::endl;
	}
	else
	{
		// std::cout << "File printed line by line ready to be parsed" << std::endl;
		std::string line;
		int brackets = 0;
		while (std::getline(file, line))
		{
			splitMaps(line, brackets);
		}
		if (brackets != 0)
			throw SyntaxErrorException();
	}

	// for (const auto& pair : _parsingServer)
	// {
	// 	std::cout << pair.first << ": " << pair.second << std::endl;
	// }
}

const char* parseConfig::SyntaxErrorException::what() const throw()
{
    return "Exception: Brackets or ';' is missing";
}