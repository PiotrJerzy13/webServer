/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   socket.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: pwojnaro <pwojnaro@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/05 20:29:22 by piotr             #+#    #+#             */
/*   Updated: 2025/03/12 15:34:06 by pwojnaro         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "socket.hpp"

Socket::Socket() : _fd(-1) {}

Socket::Socket(int domain, int type, int protocol) : _fd(socket(domain, type, protocol))
{
    if (_fd < 0)
    {
        throw std::runtime_error("Failed to create socket");
    }
}

Socket::Socket(int fd) : _fd(fd)
{
    if (_fd < 0) {
        throw std::runtime_error("Invalid file descriptor");
    }
}

Socket::Socket(int fd, const std::string& serverName)
    : _fd(fd), _serverName(serverName)
{
    if (_fd < 0)
	{
        throw std::runtime_error("Invalid file descriptor");
    }
}

Socket::~Socket()
{
    if (_fd >= 0)
    {
        close(_fd);
    }
}

Socket::Socket(Socket&& other) noexcept : _fd(other._fd)
{
    other._fd = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept
{
    if (this != &other)
    {
        if (_fd >= 0)
        {
            close(_fd);
        }
        _fd = other._fd;
        other._fd = -1;
    }
    return *this;
}

int Socket::getFd() const
{
    return _fd;
}

std::string Socket::getServerName() const
{
    return _serverName;
}