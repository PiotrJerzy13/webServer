/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: piotr <piotr@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/05 15:57:27 by piotr             #+#    #+#             */
/*   Updated: 2025/03/16 15:53:14 by piotr            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "parseConfig.hpp"
#include "webServer.hpp"
#include "utils.hpp"
#include <thread>
#include <vector>
#include <memory>

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
    
    std::vector<parseConfig> parser = splitServers(file);
    file.close();
    
    std::vector<std::thread> threads;
    
    for (size_t j = 0; j < parser.size(); j++)
    {
        try
        {
            parser[j].parse(parser[j]._mainString);
            
            std::cout << "Autoindex Configuration for Server " << j << ":\n";
            for (const auto& [location, autoindex] : parser[j]._autoindexConfig)
            {
                std::cout << "Location: " << location << ", Autoindex: " << (autoindex ? "on" : "off") << "\n";
            }

            std::cout << "Redirections for Server " << j << ":\n";
            for (const auto& [location, target] : parser[j].getRedirections())
            {
                std::cout << "Location: " << location << ", Target: " << target << "\n";
            }

            std::cout << "Server Names for Server " << j << ":\n";
            for (const auto& [serverBlock, serverName] : parser[j].getServerNames())
            {
                std::cout << "Server Block: " << serverBlock << ", Server Name: " << serverName << "\n";
            }
            
            std::shared_ptr<webServer> server = std::make_shared<webServer>(
                parser[j]._parsingServer, parser[j]._parsingLocation);
            
            // Set the autoindex configuration before starting the server
            server->setAutoindexConfig(parser[j]._autoindexConfig);

            // Set the redirections before starting the server
            server->setRedirections(parser[j].getRedirections());

            // Set the server names before starting the server
            server->setServerNames(parser[j].getServerNames());

            // Set the root directories before starting the server
            server->setRootDirectories(parser[j].getRootDirectories());

            // Set the allowed methods before starting the server
            server->setAllowedMethods(parser[j].getAllowedMethods()); // <-- Add this line

            // Set the client max body size for each server block
            for (const auto& [serverBlock, serverName] : parser[j].getServerNames()) {
                size_t maxBodySize = parser[j].getClientMaxBodySize(serverBlock);
                server->setClientMaxBodySize(serverName, maxBodySize);
            }
	
            threads.push_back(std::thread([server]()
            {
                server->start();
            }));
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error starting server: " << e.what() << '\n';
        }
    }
    
    for (auto &t : threads)
    {
        if (t.joinable())
            t.join();
    }
    return 0;
}