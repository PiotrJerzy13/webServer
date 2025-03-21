/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileUtilis.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: pwojnaro <pwojnaro@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/13 20:02:19 by piotr             #+#    #+#             */
/*   Updated: 2025/03/21 20:58:43 by pwojnaro         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <string>
#include <vector>
#include <utility>

class FileUtils {
public:

	static std::pair<std::vector<char>, std::string> readFile(const std::string& filePath);
	static bool writeFile(const std::string& filePath, const std::string& content);
	static bool createDirectoryIfNotExists(const std::string& dirPath);
	static bool deleteFile(const std::string& filePath);
};

