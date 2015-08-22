#pragma once

#include <SFML/System/Vector2.hpp>

#include <vector>

class Path
{
public:
	typedef std::vector<sf::Vector2i>::const_iterator iterator;

	static const Path Invalid;

	Path();

	void begin(size_t size);
	void push(const sf::Vector2i& pos);

	iterator begin() const;
	iterator end() const;

private:
	std::vector<sf::Vector2i> mPoints;
};