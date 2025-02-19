/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webServer.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eleni <eleni@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/16 21:11:58 by anamieta          #+#    #+#             */
/*   Updated: 2025/02/19 15:35:53 by eleni            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <netinet/in.h>
#include <poll.h>
#include <fcntl.h>

class webServer {
	public:
		webServer(const std::unordered_multimap<std::string, std::string>& serverConfig,
					const std::unordered_multimap<std::string, std::vector<std::string>>& locationConfig);
		void start();
	private:
		void handleRequest(int clientSocket);
		void sendResponse(int clientSocket, const std::string& response);
		void setNonBlocking(int socket);

		std::unordered_multimap<std::string, std::string> _serverConfig;
		std::unordered_multimap<std::string, std::vector<std::string>> _locationConfig;
		std::vector<int> _serverSockets;
		std::vector<struct pollfd> _pollfds;
};
