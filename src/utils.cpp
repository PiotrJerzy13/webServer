/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: piotr <piotr@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/20 13:20:02 by eleni             #+#    #+#             */
/*   Updated: 2025/03/05 20:11:41 by piotr            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "utils.hpp"

std::vector<parseConfig> splitServers(std::ifstream& file)
{
	std::string line;
	std::vector<parseConfig> parser;
	int i = -1;
	int brackets = 1;
	
	while (std::getline(file, line))
	{
		if (line.find("server ") == std::string::npos || line.find("server	") == std::string::npos)
			break ;
	}
	while (std::getline(file, line))
	{
		if (line.find("server ") != std::string::npos || line.find("server	") != std::string::npos)
		{
			i++;
			parser.push_back(parseConfig());
			parser[i]._mainString += line;
			continue ;
		}
		if (i >= 0)
		{
			parser[i]._mainString += '\n';
			parser[i]._mainString += line;
		}
		if (line.find('{') != std::string::npos)
		{
			brackets++;
		}
		else if(line.find('}') != std::string::npos)
		{
			brackets--;
			
			if (brackets == 0)
			{
				brackets++;
			}
		}
	}

	int lastServer = parser.size() - 1;
	std::string::size_type pos = parser[lastServer]._mainString.find_last_of('}');
	if (pos != std::string::npos)
    	parser[lastServer]._mainString.erase(pos); 

	return parser;
}

// void printParsingLocation(const std::unordered_multimap<std::string, std::vector<std::string>>& parsingLocation) 
// {
//     for (const auto& pair : parsingLocation) 
// 	{
//         std::cout << "Location: " << pair.first << std::endl;

//         for (const auto& value : pair.second) 
// 		{
//             std::cout << "  - " << value << std::endl;
//         }
//     }
// }