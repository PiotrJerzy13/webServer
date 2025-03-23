/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   security.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: anamieta <anamieta@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/02 15:42:48 by pwojnaro          #+#    #+#             */
/*   Updated: 2025/03/23 15:32:36 by anamieta         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "WebServer.hpp"

std::string webServer::sanitizePath(const std::string& path)
{
    std::vector<std::string> segments;
    std::string currentSegment;

    for (size_t i = 0; i < path.length(); i++)
	{
        if (path[i] == '/')
		{
            if (!currentSegment.empty())
			{
                segments.push_back(currentSegment);
                currentSegment.clear();
            }
        }
		else
		{
            currentSegment += path[i];
        }
    }
    if (!currentSegment.empty())
	{
        segments.push_back(currentSegment);
    }

    std::vector<std::string> normalizedSegments;
    for (const auto& segment : segments)
	{
        if (segment == ".")
		{
            continue;
        }
		else if (segment == "..")
		{
            if (!normalizedSegments.empty())
			{
                normalizedSegments.pop_back();
            }
        }
		else
		{
            normalizedSegments.push_back(segment);
        }
    }

    std::string normalizedPath = "/";
    for (size_t i = 0; i < normalizedSegments.size(); i++)
	{
        if (i > 0)
		{
            normalizedPath += "/";
        }
        normalizedPath += normalizedSegments[i];
    }
    return normalizedPath;
}
std::string webServer::sanitizeFilename(const std::string& filename)
{
    std::string result;
    for (char c : filename)
	{
        if (isalnum(c) || c == '_' || c == '-' || c == '.')
		{
            result += c;
        }
		else
		{
            result += '_';
        }
    }
    return result;
}
