/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eleni <eleni@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/20 13:29:16 by eleni             #+#    #+#             */
/*   Updated: 2025/02/20 13:32:26 by eleni            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "parseConfig.hpp"

std::vector<parseConfig> splitServers(std::ifstream& file);
void printParsingLocation(const std::unordered_multimap<std::string, std::vector<std::string>>& parsingLocation);