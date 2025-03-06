/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   socket.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: piotr <piotr@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/05 20:32:32 by piotr             #+#    #+#             */
/*   Updated: 2025/03/05 21:21:22 by piotr            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdexcept>
#include <unistd.h>
#include <sys/socket.h>

class Socket
{
public:
    Socket();
    Socket(int domain, int type, int protocol);
    Socket(int fd);
    ~Socket();
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    
    int getFd() const;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

private:
    int _fd;
};