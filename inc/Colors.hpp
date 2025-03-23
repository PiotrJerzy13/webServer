/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Colors.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: anamieta <anamieta@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/23 15:22:49 by anamieta          #+#    #+#             */
/*   Updated: 2025/03/23 15:25:59 by anamieta         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#define BOLD(text) "\033[1m" << text << "\033[0m"
#define ITALIC(text) "\033[3m" << text << "\033[0m"

#define GREEN(text) "\033[38;5;28m" << text << "\033[0m"
#define BLUE(text) "\033[38;5;32m" << text << "\033[0m"
#define YELLOW(text) "\033[38;5;220m" << text << "\033[0m"
#define RED(text) "\033[38;5;160m" << text << "\033[0m"
#define PINK(text) "\033[38;5;205m" << text << "\033[0m"