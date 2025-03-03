/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: pwojnaro <pwojnaro@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/12 14:42:10 by eleni             #+#    #+#             */
/*   Updated: 2025/03/02 14:13:01 by pwojnaro         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "parseConfig.hpp"
#include "webServer.hpp"
#include "utils.hpp"

// add killing ports in the signals

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

	std::vector<parseConfig> parser;
	parser = splitServers(file);

    file.close();

	for (size_t j = 0; j < parser.size(); j++)
	{
		try
		{
			parser[j].parse(parser[j]._mainString);
    		webServer server(parser[j]._parsingServer, parser[j]._parsingLocation);
		    server.start();
		}
		catch(const std::exception& e)
		{
			std::cerr << e.what() << '\n';
		}
		// std::cout << "Server " << 1 << " configuration:\n";
		// std::cout << parser[1]._mainString << std::endl;
	}

    return 0;
}
