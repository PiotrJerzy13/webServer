/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: pwojnaro <pwojnaro@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/05 15:57:27 by piotr             #+#    #+#             */
/*   Updated: 2025/03/07 14:05:42 by pwojnaro         ###   ########.fr       */
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
            std::shared_ptr<webServer> server = std::make_shared<webServer>(
                parser[j]._parsingServer, parser[j]._parsingLocation);
                
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
