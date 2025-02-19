/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eleni <eleni@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/12 14:42:10 by eleni             #+#    #+#             */
/*   Updated: 2025/02/19 18:42:33 by eleni            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "parseConfig.hpp"
#include "webServer.hpp"


void printParsingLocation(const std::unordered_multimap<std::string, std::vector<std::string>>& parsingLocation) {
    for (const auto& pair : parsingLocation) {  // Use pair instead of structured bindings
        std::cout << "Location: " << pair.first << std::endl;

        for (const auto& value : pair.second) {
            std::cout << "  - " << value << std::endl;
        }
    }
}

int main(int argc, char** argv)
{
    std::string filename;
    if (argc > 2)
    {
        std::cout << "Usage: ./webserv <file.config>" << std::endl;
        return 1;
    }
    else if (argc == 1)
        filename = "config/config1.config";
    else if (argc == 2)
        filename = argv[1];

    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open configuration file: " << filename << std::endl;
        return 1;
    }

	std::string line;
	std::string temp;
	std::vector<parseConfig> parser;
	int i = -1;
	int brackets = 1;
	
	while (std::getline(file, line))
	{
		if (line.find("server ") == std::string::npos || line.find("server	") == std::string::npos)
			break ;
	}
	while (std::getline(file, line))
	{
		if (line.find("server ") != std::string::npos || line.find("server	") != std::string::npos)
		{
			if (i >= 1 && !parser[i]._mainString.empty())
			{
				std::cout << "Server " << i + 1 << " configuration:\n";
				std::cout << parser[i]._mainString << std::endl;	
			}
			parser.push_back(parseConfig());
			parser[i]._mainString += line;
			
			// std::cout << parser[i]._mainString << "!" << std::endl;
			i++;
			continue ;
		}
		if (i >= 0)
		{
			// std::cout << line << std::endl;
			parser[i]._mainString += '\n';
			parser[i]._mainString += line;
			// std::cout << parser[i]._mainString << std::endl;
		}
		if (line.find('{') != std::string::npos)
		{
			brackets++;
			// std::cout << brackets << std::endl;
		}
		else if(line.find('}') != std::string::npos)
		{
			brackets--;
			// std::cout << brackets << std::endl;
			
			if (brackets == 0)
			{
				if (i >= 0 && !parser[i]._mainString.empty())
				{
					std::cout << "Server " << i + 1 << " configuration (completed):\n";
					std::cout << parser[i]._mainString << std::endl;
				}
				parser[i]._mainString.clear();
			}
		}
	}
    file.close();

	for (size_t j = 0; j < parser.size(); j++)
	{
		try
		{
			parser[j].parse(filename);

			// printParsingLocation(parser[j]._parsingLocation);
			
			// webServer server(parser._parsingServer, parser._parsingLocation);
			// server.start();

			
		}
		catch(const std::exception& e)
		{
			std::cerr << e.what() << '\n';
		}
	}

    return 0;
}
