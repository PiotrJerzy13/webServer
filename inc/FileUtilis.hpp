/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileUtilis.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: piotr <piotr@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/13 20:02:19 by piotr             #+#    #+#             */
/*   Updated: 2025/03/13 20:28:09 by piotr            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <string>
#include <vector>
#include <utility>

class FileUtils {
public:

    static bool fileExists(const std::string& filePath);

    static std::pair<std::vector<char>, std::string> readFile(const std::string& filePath);
    static bool writeFile(const std::string& filePath, const std::string& content);
    static bool createDirectoryIfNotExists(const std::string& dirPath);
    static bool deleteFile(const std::string& filePath);
    static std::string resolveUploadPath(const std::string& filePath, const std::string& serverName = "");
};

