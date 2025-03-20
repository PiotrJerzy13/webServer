/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parseConfig.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eperperi <eperperi@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/12 17:50:48 by eleni             #+#    #+#             */
/*   Updated: 2025/03/20 15:40:11 by eperperi         ###   ########.fr       */
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
#include "webServer.hpp"

class parseConfig {
    public:
        // Constructor
        parseConfig() {}
        
        // **Parsing Storage**
        std::unordered_multimap<std::string, std::string> _parsingServer;
        std::unordered_multimap<std::string, std::vector<std::string>> _parsingLocation;
        std::map<std::string, bool> _autoindexConfig;
        std::string _mainString;
    
        // **Struct for CGI Configuration**
        struct CGIConfig 
        {
            std::string cgiPass;
            std::string scriptFilename;
            std::string pathInfo;
            std::string queryString;
            std::string requestMethod;
        };
    
        // **Public Getter Methods**
        size_t getClientMaxBodySize(const std::string& serverBlock) const;
        const std::map<std::string, std::vector<std::string>>& getAllowedMethods() const;
        const std::map<std::string, CGIConfig>& getCGIConfigs() const;
        const std::map<std::string, std::string>& getRedirections() const;
        const std::map<std::string, std::string>& getServerNames() const;
        const std::map<std::string, std::string>& getErrorPages() const;
        const std::map<std::string, std::string>& getRootDirectories() const;
        const std::string& getIndex() const;
        const std::map<std::string, bool>& getAutoindexConfig() const;
        const parseConfig::CGIConfig& getCGIConfig(const std::string& location) const;
    
        // **Public Setter & Parsing Functions**
        void parseClientMaxBodySize(const std::string& line);
        void parse(const std::string& filename);
    
        // **Error Handling**
        class SyntaxErrorException : public std::exception 
        {
        public:
            const char* what() const throw();
        };
    
    private:
        // **Storage for Parsed Configuration**
        std::string _location;
        std::string _currentServerBlock;
        std::vector<std::string> _methods;
        std::stack<std::string> _blocks;
        std::string _index;
		
        // **Configuration Data Structures**
        std::map<std::string, CGIConfig> _cgiConfig;
        std::map<std::string, size_t> _clientMaxBodySize;
        std::map<std::string, std::string> _serverNames;
        std::map<std::string, std::string> _errorPages;
        std::map<std::string, std::string> _rootDirectories;
        std::map<std::string, std::string> _aliasDirectories;
        std::map<std::string, std::string> _redirections;
        std::map<std::string, std::vector<std::string>> _allowedMethods;
        std::map<std::string, std::vector<std::string>> _tryFiles;
        std::map<std::string, std::string> _allow;
        std::map<std::string, std::string> _indexFiles;
        std::map<std::string, std::string> _cgiPass;
        std::map<std::string, std::map<std::string, std::string>> _cgiParams;
    
        // **Parsing Functions**
        void splitMaps(std::string& line, int& brackets);
        void trimServer(const std::string& line);
        std::string trimLocation(const std::string& line);
        std::string trim(const std::string& line);
        void fillLocationMap(std::string& line, const std::string& location);
    
};
