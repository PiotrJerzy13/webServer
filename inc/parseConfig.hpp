/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parseConfig.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eleni <eleni@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/12 17:50:48 by eleni             #+#    #+#             */
/*   Updated: 2025/02/12 20:38:36 by eleni            ###   ########.fr       */
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
		std::unordered_map<std::string, std::string> _parsingServer;
		std::unordered_map<std::string, std::vector<std::string>> _parsingLocation;

	void parse(const std::string& filename);
	void trim(std::string& line, int& brackets);

	class SyntaxErrorException : public std::exception
	{
		public:
			const char* what() const throw();
	};
		
};