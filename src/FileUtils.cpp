/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileUtils.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: anamieta <anamieta@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/13 20:03:48 by piotr             #+#    #+#             */
/*   Updated: 2025/03/23 15:47:37 by anamieta         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "FileUtils.hpp"
#include "HTTPResponse.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <utility>
#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>
#include <sstream>

std::pair<std::vector<char>, std::string> FileUtils::readFile(const std::string& filePath)
{

	if (!std::filesystem::exists(filePath))
	{
		return {{}, ""};
	}

	std::ifstream file(filePath, std::ios::binary);
	if (!file.is_open())
	{
		std::cout << RED("[ERROR] Failed to open file: " << filePath) << std::endl;
		return {{}, ""};
	}

	std::uintmax_t fileSize = std::filesystem::file_size(filePath);
	std::vector<char> fileContent(fileSize);

	if (!file.read(fileContent.data(), fileSize))
	{
		std::cout << RED("[ERROR] Failed to read file content: " << filePath) << std::endl;
		file.close();
		return {{}, ""};
	}

	file.close();
	return {fileContent, HTTPResponse::getContentType(filePath)};
	}

	bool FileUtils::writeFile(const std::string& filePath, const std::string& content)
	{
	std::ofstream file(filePath, std::ios::binary);
	if (!file.is_open())
	{
		std::cerr << RED("[ERROR] Failed to open file for writing: " << filePath) << std::endl;
		return false;
	}
	file.write(content.data(), content.size());
	file.close();
	return file.good();
	}

	bool FileUtils::createDirectoryIfNotExists(const std::string& dirPath)
	{
	try {
		std::cout << BLUE("[INFO] Creating directory: " << dirPath) << std::endl;
		if (!std::filesystem::exists(dirPath))
		{
			std::filesystem::create_directories(dirPath);
		}
		return true;
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		std::cerr << RED("[ERROR] Failed to create directory: " << e.what()) << std::endl;
		return false;
	}
}

bool FileUtils::deleteFile(const std::string& filePath)
{
	if (!std::filesystem::is_regular_file(filePath))
	{
		std::cerr << YELLOW("[DELETE] File not found or not a regular file: " << filePath) << std::endl;
		return false;
	}

	try {
		bool result = std::filesystem::remove(filePath);
		if (result)
		{
			std::cout << PINK("[DELETE] Successfully deleted file: " << filePath) << std::endl;
		}
		else
		{
			std::cerr << YELLOW("[DELETE] Failed to delete file: " << filePath) << std::endl;
		}
		return result;

	}
	catch (const std::filesystem::filesystem_error& e)
	{
		std::cerr << RED("[DELETE] Filesystem error: " << e.what()) << std::endl;
		return false;
	}
}
