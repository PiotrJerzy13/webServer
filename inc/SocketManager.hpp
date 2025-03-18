/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: piotr <piotr@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/07 16:04:37 by pwojnaro          #+#    #+#             */
/*   Updated: 2025/03/18 10:36:13 by piotr            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# pragma once

#include <vector>
#include <poll.h>
#include <netinet/in.h>
#include <stdexcept>
#include <iostream>
#include "socket.hpp"

class SocketManager {
public:
    SocketManager();
    ~SocketManager();

    void createSocket(int port);
	std::optional<int> acceptConnection(int serverFd);
    void closeSocket(int fd);
	void setNonBlocking(int socketFd);
    std::vector<pollfd>& getPollFds();
    std::vector<Socket>& getServerSockets();
	std::optional<std::string> isPortAvailable(int port);

private:
    std::vector<Socket> _serverSockets;
    std::vector<pollfd> _pollFds;

};
