/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: anamieta <anamieta@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/05 15:57:27 by piotr             #+#    #+#             */
/*   Updated: 2025/03/23 15:52:40 by anamieta         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ParseConfig.hpp"
#include "WebServer.hpp"
#include "Utils.hpp"
#include "CGIHandler.hpp"
#include <thread>
#include <vector>
#include <memory>

int main(int argc, char** argv)
{
	std::string filename;
	if (argc > 2)
	{
		std::cout << BOLD(YELLOW("Usage: ./webserv <file.config>")) << std::endl;
		return 1;
	}
	else if (argc == 1)
		filename = "config/config1.config";
	else if (argc == 2)
		filename = argv[1];

	std::ifstream file(filename);
	if (!file.is_open())
	{
		std::cerr << RED("[ERROR] Could not open configuration file: " << filename) << std::endl;
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

			server->setAutoindexConfig(parser[j]._autoindexConfig);
			server->setRedirections(parser[j].getRedirections());
			server->setServerNames(parser[j].getServerNames());
			server->setRootDirectories(parser[j].getRootDirectories());
			server->setAllowedMethods(parser[j].getAllowedMethods());

			std::map<std::string, CGIHandler::CGIConfig> webServerCGIConfig;
			const auto& parserCGIConfig = parser[j].getCGIConfigs();

			for (const auto& [location, config] : parserCGIConfig)
			{
				CGIHandler::CGIConfig serverConfig;
				serverConfig.cgiPass = config.cgiPass;
				serverConfig.scriptFilename = config.scriptFilename;
				serverConfig.pathInfo = config.pathInfo;
				serverConfig.queryString = config.queryString;
				serverConfig.requestMethod = config.requestMethod;
				webServerCGIConfig[location] = serverConfig;
			}

			server->getCGIHandler().setCGIConfig(webServerCGIConfig);

			for (const auto& [serverBlock, serverName] : parser[j].getServerNames())
			{
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
			std::cerr << RED("[ERROR] starting server: " << e.what()) << '\n';
		}
	}

	for (auto &t : threads)
	{
		if (t.joinable())
			t.join();
	}
	return 0;
}

/**
 * Evaluation commands:
 * brew install siege
 * siege -b -t30s http://127.0.0.1:8082/
 * brew install watch
 * ps aux | grep webServ
 * watch -n 1 "ps -o pid,rss,vsz,etime,command -p PID"
 * lsof -i :8082
 * Setup multiple servers with different hostnames (use something like: curl --resolve
 *
 * curl -v --resolve server2.local:8081:127.0.0.1 http://server2.local:8081/
 * curl -v --resolve localhost:8082:127.0.0.1 http://localhost:8082/
 *
 * POST commands:
 * curl -v -X POST --resolve localhost:8082:127.0.0.1 \
 * -H "Content-Type: text/plain" --data "This is a short request." http://localhost:8082/
 *
 * curl -v -X POST --resolve localhost:8082:127.0.0.1 -H "Content-Type: multipart/form-data" -F "file=@file.txt" http://localhost:8082/upload
 *
 * curl -v -X DELETE --resolve localhost:8082:127.0.0.1 http://localhost:8082/upload/file.txt
 * curl -v -X FOO http://localhost:8082/
 * curl -v http://localhost:8082/upload/file.txt -o downloaded.txt
 *
 * CGI
 * curl -v http://localhost:8082/cgi-bin/script.py
 * curl -X POST -d "name=John&age=25" "http://localhost:8082/cgi-bin/script.py"
 * curl -v http://localhost:8082/cgi-bin/infinite.py
 *
*/