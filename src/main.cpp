/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eleni <eleni@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/12 14:42:10 by eleni             #+#    #+#             */
/*   Updated: 2025/02/12 20:04:26 by eleni            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "parseConfig.hpp"

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
	file.close();
	try
	{
		parseConfig parser;
		parser.parse(filename);	
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << '\n';
	}
	
	return 0;
}