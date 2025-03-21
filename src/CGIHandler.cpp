/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGIHandler.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: pwojnaro <pwojnaro@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/21 20:54:27 by pwojnaro          #+#    #+#             */
/*   Updated: 2025/03/21 21:14:57 by pwojnaro         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "CGIHandler.hpp"

volatile sig_atomic_t CGIHandler::timeoutOccurred = 0;

CGIHandler::CGIHandler(const std::unordered_multimap<std::string, std::string>& serverConfig,
					   const std::map<std::string, CGIConfig>& cgiConfig)
	: _serverConfig(serverConfig), _cgiConfig(cgiConfig) {}

CGIHandler::CGIHandler(const std::unordered_multimap<std::string, std::string>& serverConfig) : _serverConfig(serverConfig) {}

void CGIHandler::handleTimeout(int signal)
{
	if (signal == SIGALRM) {
		timeoutOccurred = 1;
	}
}

const CGIHandler::CGIConfig& CGIHandler::getCGIConfig(const std::string& requestPath) const 
{
	std::string bestMatch = "";
	for (const auto& [location, config] : _cgiConfig) 
	{
		if (requestPath.find(location) == 0) 
		{
			if (location.length() > bestMatch.length()) 
			{
				bestMatch = location;
			}
		}
	}
	if (!bestMatch.empty()) 
	{
		return _cgiConfig.at(bestMatch);
	}
	static const CGIConfig defaultConfig;
	return defaultConfig;
}

const std::map<std::string, CGIHandler::CGIConfig>& CGIHandler::getCGIConfigs() const 
{
	return _cgiConfig;
}

void CGIHandler::setCGIConfig(const std::map<std::string, CGIConfig>& cgiConfig) 
{
	_cgiConfig = cgiConfig;
}

std::string CGIHandler::executeCGI(const std::string& scriptPath, const std::string& method, 
	const std::string& queryString, const std::string& requestBody) 
{
	signal(SIGALRM, handleTimeout);
	timeoutOccurred = 0;
	alarm(5);

	std::string location = scriptPath.substr(0, scriptPath.rfind('/'));
	if (location.empty()) location = "/";
	const CGIConfig& cgiConfig = getCGIConfig(location);

	std::string cleanScriptPath = scriptPath.substr(0, scriptPath.find('?'));

	std::string rootDir = "./www";
	auto itRoot = _serverConfig.find("root");
	if (itRoot != _serverConfig.end())
	{
		rootDir = itRoot->second;
	}

	int pipeToChild[2], pipeFromChild[2];
	if (pipe(pipeToChild) == -1 || pipe(pipeFromChild) == -1)
	{
		return "500 Internal Server Error";
	}

	pid_t pid = fork();
	if (pid == -1)
	{
		return "500 Internal Server Error";
	}

	if (pid == 0)
	{
		close(pipeToChild[1]);
		close(pipeFromChild[0]);

		if (dup2(pipeToChild[0], STDIN_FILENO) == -1 || dup2(pipeFromChild[1], STDOUT_FILENO) == -1)
		{
			exit(1);
		}

		std::string requestMethodEnv = cgiConfig.requestMethod.empty() ? method : cgiConfig.requestMethod;
		if (requestMethodEnv.find("$request_method") != std::string::npos)
		{
			requestMethodEnv = method;
		}
		setenv("REQUEST_METHOD", requestMethodEnv.c_str(), 1);

		std::string queryStringEnv = cgiConfig.queryString.empty() ? queryString : cgiConfig.queryString;
		if (queryStringEnv.find("$query_string") != std::string::npos)
		{
			queryStringEnv = queryString;
		}
		setenv("QUERY_STRING", queryStringEnv.c_str(), 1);

		setenv("CONTENT_LENGTH", std::to_string(requestBody.size()).c_str(), 1);

		std::string contentType = "application/x-www-form-urlencoded";
		setenv("CONTENT_TYPE", contentType.c_str(), 1);

		std::string scriptFilename = cgiConfig.scriptFilename.empty() ? cleanScriptPath : cgiConfig.scriptFilename;
		size_t pos;
		if ((pos = scriptFilename.find("$fastcgi_script_name")) != std::string::npos)
		{
			std::string scriptName = cleanScriptPath.substr(cleanScriptPath.rfind('/') + 1);
			scriptFilename.replace(pos, 20, scriptName);
		}

		if (!scriptFilename.empty() && scriptFilename[0] != '/')
		{
			scriptFilename = rootDir + "/" + scriptFilename;
		} else {
			scriptFilename = rootDir + scriptFilename;
		}

		while ((pos = scriptFilename.find("//")) != std::string::npos)
		{
			scriptFilename.replace(pos, 2, "/");
		}

		setenv("SCRIPT_FILENAME", scriptFilename.c_str(), 1);

		std::string pathInfo = cgiConfig.pathInfo.empty() ? cleanScriptPath : cgiConfig.pathInfo;
		if ((pos = pathInfo.find("$fastcgi_script_name")) != std::string::npos)
		{
			std::string scriptName = cleanScriptPath.substr(cleanScriptPath.rfind('/') + 1);
			pathInfo.replace(pos, 20, scriptName);
		}
		setenv("PATH_INFO", pathInfo.c_str(), 1);

		std::string interpreter = cgiConfig.cgiPass;
		if (interpreter.empty())
		{
			auto it = _serverConfig.find("default_cgi_interpreter");
			if (it != _serverConfig.end())
			{
				interpreter = it->second;
			}
			else
			{
				interpreter = "/usr/bin/python3";
			}
		}

		std::string interpreterName = interpreter.substr(interpreter.rfind('/') + 1);
		execlp(interpreter.c_str(), interpreterName.c_str(), scriptFilename.c_str(), nullptr);

		exit(1);
	}

	close(pipeToChild[0]);
	close(pipeFromChild[1]);

	if (method == "POST" && !requestBody.empty())
	{
		write(pipeToChild[1], requestBody.c_str(), requestBody.size());
	}
	close(pipeToChild[1]);

	std::string cgiOutput;
	char buffer[4096];
	fd_set readSet;
	struct timeval timeout;

	timeout.tv_sec = 5;
	timeout.tv_usec = 0;

	while (true)
	{
		FD_ZERO(&readSet);
		FD_SET(pipeFromChild[0], &readSet);

		int selectResult = select(pipeFromChild[0] + 1, &readSet, nullptr, nullptr, &timeout);
		if (selectResult < 0)
		{
			break;
		}
		else if (selectResult == 0)
		{
			kill(pid, SIGKILL);
			waitpid(pid, nullptr, 0);
			return "504 Gateway Timeout";
		}

		if (FD_ISSET(pipeFromChild[0], &readSet)) {
			ssize_t bytesRead = read(pipeFromChild[0], buffer, sizeof(buffer));
			if (bytesRead > 0)
			{
				cgiOutput.append(buffer, bytesRead);
			} 
			else if (bytesRead == 0)
			{
				break;
			} 
			else
			{
				break;
			}
		}
	}
	close(pipeFromChild[0]);

	int status;
	waitpid(pid, &status, 0);

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
	{
		if (cgiOutput.find("HTTP/1.") != 0)
		{
			std::string httpVersion = "HTTP/1.1";
			auto it = _serverConfig.find("http_version");
			if (it != _serverConfig.end()) {
				httpVersion = it->second;
			}

			std::string headers = httpVersion + " 200 OK\r\n";

			if (cgiOutput.find("\r\n\r\n") == std::string::npos)
			{
				std::string defaultContentType = "text/html";
				auto it = _serverConfig.find("default_cgi_content_type");
				if (it != _serverConfig.end())
				{
					defaultContentType = it->second;
				}

				headers += "Content-Type: " + defaultContentType + "\r\n";
				headers += "Content-Length: " + std::to_string(cgiOutput.size()) + "\r\n\r\n";
			}
			cgiOutput = headers + cgiOutput;
		}
		return cgiOutput;
	}

	return "500 Internal Server Error";
}
