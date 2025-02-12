/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parseConfig.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eleni <eleni@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/12 18:29:11 by eleni             #+#    #+#             */
/*   Updated: 2025/02/12 20:41:17 by eleni            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "parseConfig.hpp"

void parseConfig::trim(std::string& line, int& brackets)
{
	std::cout << line << std::endl;

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
		// std::cout << _blocks.top() << std::endl;
		return ;
	}
	if (line.find("location") != std::string::npos)
	{
		this->_blocks.push("location");
		return ;
	}
	if (!this->_blocks.empty() && this->_blocks.top() =="server")
	{
		// std::cout << "Hi from Server" << std::endl;
		// trimServer(line);
	}
	else if (!this->_blocks.empty() && this->_blocks.top() == "location")
	{
		// std::cout << "Hi from Location" << std::endl;
		// trimLocation(line);
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
		std::cout << "File printed line by line ready to be parsed" << std::endl;
		std::string line;
		int brackets = 0;
		while (std::getline(file, line))
		{
			trim(line, brackets);
			// std::cout << line << std::endl;
		}
		if (brackets != 0)
			throw SyntaxErrorException();
	}
}

const char* parseConfig::SyntaxErrorException::what() const throw()
{
    return "Exception: Closing bracket is missing";
}