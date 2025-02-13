/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parseConfig.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eleni <eleni@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/12 17:50:48 by eleni             #+#    #+#             */
/*   Updated: 2025/02/13 14:06:05 by eleni            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <stack>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <string>
#include <iostream>
#include <fstream>

class parseConfig
{
	public:
		std::stack<std::string> _blocks;
		std::unordered_multimap<std::string, std::string> _parsingServer;
		std::unordered_map<std::string, std::vector<std::string>> _parsingLocation;

	void parse(const std::string& filename);
	void splitMaps(std::string& line, int& brackets);
	void trimServer(const std::string& line);

	class SyntaxErrorException : public std::exception
	{
		public:
			const char* what() const throw();
	};
	
	private:
		std::string trim(const std::string& line);
};