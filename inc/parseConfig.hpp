/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parseConfig.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: pwojnaro <pwojnaro@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/12 17:50:48 by eleni             #+#    #+#             */
/*   Updated: 2025/03/10 19:25:53 by pwojnaro         ###   ########.fr       */
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
#include <map>

class parseConfig
{
	public:
		std::string _mainString;
		std::stack<std::string> _blocks;
		std::unordered_multimap<std::string, std::string> _parsingServer;
		std::unordered_multimap<std::string, std::vector<std::string>> _parsingLocation;
		std::string _location;
		std::map<std::string, bool> _autoindexConfig;
		std::map<std::string, std::string> _redirections;
		const std::map<std::string, std::string>& getRedirections() const;

		void parse(const std::string& filename);

	class SyntaxErrorException : public std::exception
	{
		public:
			const char* what() const throw();
	};
	
	private:
		void splitMaps(std::string& line, int& brackets);
		void trimServer(const std::string& line);
		std::string trimLocation(const std::string& line);
		std::string trim(const std::string& line);
		void fillLocationMap(std::string& line, const std::string& location);
		const std::map<std::string, bool>& getAutoindexConfig() const;
		
};