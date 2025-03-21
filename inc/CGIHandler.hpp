#pragma once

#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <cstring>
#include <fcntl.h>

class CGIHandler {
public:
    CGIHandler(const std::unordered_multimap<std::string, std::string>& serverConfig);

    struct CGIConfig {
        std::string cgiPass;
        std::string scriptFilename;
        std::string pathInfo;
        std::string queryString;
        std::string requestMethod;
    };

    CGIHandler(const std::unordered_multimap<std::string, std::string>& serverConfig,
               const std::map<std::string, CGIConfig>& cgiConfig);

    std::string executeCGI(const std::string& scriptPath, const std::string& method,
                           const std::string& queryString, const std::string& requestBody);

    void setCGIConfig(const std::map<std::string, CGIConfig>& cgiConfig);
    const std::map<std::string, CGIConfig>& getCGIConfigs() const;
    const CGIConfig& getCGIConfig(const std::string& requestPath) const;

private:
    std::unordered_multimap<std::string, std::string> _serverConfig;
    std::map<std::string, CGIConfig> _cgiConfig;
    static volatile sig_atomic_t timeoutOccurred;

    static void handleTimeout(int signal);
};