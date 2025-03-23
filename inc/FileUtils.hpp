/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileUtils.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: anamieta <anamieta@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/13 20:02:19 by piotr             #+#    #+#             */
/*   Updated: 2025/03/23 15:23:00 by anamieta         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <string>
#include <vector>
#include <utility>
#include "Colors.hpp"

class FileUtils {
public:

	static std::pair<std::vector<char>, std::string> readFile(const std::string& filePath);
	static bool writeFile(const std::string& filePath, const std::string& content);
	static bool createDirectoryIfNotExists(const std::string& dirPath);
	static bool deleteFile(const std::string& filePath);
};
