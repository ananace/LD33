#include "Level.hpp"
#include "Components.hpp"
#include "Wave.hpp"

#include <Base/Fonts.hpp>
#include <Base/Profiling.hpp>
#include <Base/VectorMath.hpp>

#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Window/Event.hpp>

#include <fstream>
#include <set>
#include <sstream>
#include <unordered_map>


namespace std
{
	template<>
	struct hash<sf::Vector2i>
	{
		size_t operator()(const sf::Vector2i& vec) const
		{
			return std::hash<int>()(vec.x) ^ (std::hash<int>()(vec.y) << 1);
		}
	};
}

Level::Level() : Kunlaboro::Component("Game.Level"),
	mRebuildPath(true), mBestPath(new Path(Path::Invalid))
{
	mTilesTexture.loadFromFile("Resources/Tiles.png");
}

Level::~Level()
{
	if (mBestPath)
		delete mBestPath;
}

void Level::addedToEntity()
{
	requestMessage("LD33.Tick", [this](const Kunlaboro::Message& msg) {
		if (mRebuildPath) {
			auto newPath = new Path(findPath({ 1, 10 }, { 8, 10 }));
			mRebuildPath = false;

			sendGlobalMessage("Level.PathRebuilt", newPath);

			if (mBestPath)
				delete mBestPath;
			mBestPath = newPath;
		}
	});
	requestMessage("LD33.Draw", [this](const Kunlaboro::Message& msg) { draw(*msg.payload.get<sf::RenderTarget*>()); });
	changeRequestPriority("LD33.Draw", -1);

	requestMessage("Level.Valid", [this](Kunlaboro::Message& msg) {
		sf::Vector2i hex;
		if (msg.payload.is<sf::Vector2i>())
			hex = msg.payload.get<sf::Vector2i>();
		else if (msg.payload.is<sf::Vector2f>())
			hex = coordsToHex(msg.payload.get<sf::Vector2f>());
		else
			return;

		std::vector<sf::Vector2i> blocked;
		{
			std::vector<Game::Physical*> blockers;
			sendGlobalMessage("Game.Physical.Blocking", &blockers);
			for (auto& it : blockers)
				blocked.push_back(coordsToHex(it->Position));

			blocked.push_back(sf::Vector2i(mStart));
			blocked.push_back(sf::Vector2i(mGoal));
		}

		if (hex.x < 0 || hex.y < 0 || hex.x >= mLevelSize.x || hex.y >= mLevelSize.y)
			msg.handle(false);
		else if (mTiles[hex.x + hex.y * mLevelSize.x] != Tile_Grass)
			msg.handle(false);
		else
		{
			bool b = false;
			for (auto& i : blocked)
				if (i == hex)
				{
					b = true;
					break;
				}

			msg.handle(!b);
		}
	});
	requestMessage("Level.HexToCoords", [this](Kunlaboro::Message& msg) {
		msg.handle(hexToCoords(msg.payload.get<sf::Vector2i>()));
	});
	requestMessage("Level.CoordsToHex", [this](Kunlaboro::Message& msg) {
		msg.handle(coordsToHex(msg.payload.get<sf::Vector2f>()));
	});
	requestMessage("Level.GetPath", [this](Kunlaboro::Message& msg) {
		msg.handle(mBestPath);
	});
	requestMessage("LoadLevel", [this](Kunlaboro::Message& msg){
		loadFromFile(msg.payload.get<std::string>());
		msg.handle(nullptr);
	}, true);

	requestComponent("Game.Tower", [this](const Kunlaboro::Message&)
	{
		mRebuildPath = true;
	}, false);
	changeRequestPriority("Game.Tower", 9001);
}

void Level::loadFromFile(const std::string& file)
{
	std::ifstream ifs(file.c_str());

	if (!ifs || ifs.eof())
		return;

	std::string title, subtitle;
	std::getline(ifs, title);
	std::getline(ifs, subtitle);

	ifs >> mLevelSize.x >> mLevelSize.y
		>> mStart.x >> mStart.y
		>> mGoal.x >> mGoal.y;

	std::string temp;
	std::getline(ifs, temp);

	mTiles.resize(mLevelSize.x * mLevelSize.y);
	for (size_t y = 0; y < mLevelSize.y; ++y)
	{
		std::string line;
		std::getline(ifs, line);

		for (size_t x = 0; x < mLevelSize.x; ++x)
		{
			char c = line[x];
			Tile t = Tile_None;
			switch (c)
			{
			case 'w': t = Tile_Water; break;
			case 'g': t = Tile_Grass; break;
			case 'm': t = Tile_Mountain; break;
			case 'f': t = Tile_Forest; break;
			case 'o': t = Tile_Office; break;
			}

			mTiles[y * mLevelSize.x + x] = t;
		}
	}

	auto& wm = *dynamic_cast<WaveManager*>(getEntitySystem()->getAllComponentsOnEntity(getOwnerId(), "Game.WaveManager").front());
	unsigned int waves;
	ifs >> waves;

	for (unsigned int i = 0; i < waves; ++i)
	{
		unsigned int num;
		float str, rate;
		std::string name(256, 0);

		ifs >> num >> str >> rate;
		ifs.getline(&name.front(), 256);
		name = name.c_str();

		wm.addWave({ num, str, rate, name });
	}

	mRebuildPath = true;
}

const sf::Vector2u& Level::getSize() const
{
	return mLevelSize;
}

sf::Vector2f Level::hexToCoords(const sf::Vector2i& hex) const
{
	return{ hex.x * 49.f + 32, hex.y * 32.f + ((hex.x & 1) == 0) * 16 + 47 };
}
sf::Vector2i Level::coordsToHex(const sf::Vector2f& coords) const
{
	return{ int(coords.x - 8) / 49, int(coords.y - 32 - (((int(coords.x - 8) / 49) % 2) == 0) * 16) / 32 };
}

void Level::draw(sf::RenderTarget& target)
{ PROFILE
	sf::VertexArray tiles(sf::Quads, mTiles.size());

	for (int y = 0; y < mLevelSize.y; ++y)
	{
		for (int x = 0; x < mLevelSize.x; x += 2)
		{
			drawTile({ x + 1, y }, mTiles[x + 1 + y * mLevelSize.x], tiles);
			drawTile({ x, y }, mTiles[x + y * mLevelSize.x], tiles);
		}
	}

	for (auto& it : *mBestPath)
	{
		drawTile(it, Tile_Picker, tiles);
	}

	target.draw(tiles, &mTilesTexture);
}

inline void Level::drawTile(const sf::Vector2i& pos, Tile tile, sf::VertexArray& tiles)
{
	if (tile == Tile_None)
		return;

	const float width = 64;
	const float height = 32;

	int x = pos.x;
	int y = pos.y;
	int tx = tile % 3;
	int ty = tile / 3;

	tiles.append({
		{ x * 49.f, y * 32.f + (x % 2 == 0) * 16 },
		sf::Color::White,
		{ tx * 64.f, ty * 96.f }
	});
	tiles.append({
		{ x * 49.f + width, y * 32.f + (x % 2 == 0) * 16 },
		sf::Color::White,
		{ tx * 64.f + 64, ty * 96.f }
	});
	tiles.append({
		{ x * 49.f + width, y * 32.f + (x % 2 == 0) * 16 + 96 },
		sf::Color::White,
		{ tx * 64.f + 64, ty * 96.f + 96 }
	});
	tiles.append({
		{ x * 49.f, y * 32.f + (x % 2 == 0) * 16 + 96 },
		sf::Color::White,
		{ tx * 64.f, ty * 96.f + 96 }
	});
}

Path Level::findPath(const sf::Vector2i& from, const sf::Vector2i& to) const
{
	if (from == to)
		return Path::Invalid;

	sf::IntRect level{ 0, 0, int(mLevelSize.x), int(mLevelSize.y) };
	if (!level.contains(from) || !level.contains(to))
		return Path::Invalid;

	std::vector<sf::Vector2i> blocked;
	{
		std::vector<Game::Physical*> blockers;
		sendGlobalMessage("Game.Physical.Blocking", &blockers);
		for (auto& it : blockers)
			blocked.push_back(coordsToHex(it->Position));
	}
		
	Path ret;

	auto heuristic = [&from](const sf::Vector2i& a, const sf::Vector2i& b) -> float
	{
		sf::Vector3f eA(a.x, -a.x - a.y, a.y - (a.x - (a.x & 1)) / 2.f),
			eB(b.x, -b.x - b.y, b.y - (b.x - (b.x & 1)) / 2.f);

		float dx1 = a.x - b.x,
			dy1 = a.y - b.y,
			dx2 = from.x - b.x,
			dy2 = from.y - b.y;
		float cross = std::abs(dx1*dy2 - dx2*dy1);

		return (std::abs(eA.x - eB.x) + std::abs(eA.y - eB.y) + std::abs(eA.z - eB.z)) / 2.f + cross * 0.001f;
	};
	auto cost = [this,&blocked](const sf::Vector2i& a) -> float
	{
		if (a.x < 0 || a.y < 0 || a.x >= mLevelSize.x || a.y >= mLevelSize.y)
			return -1;
		if (mTiles[a.x + a.y * mLevelSize.x] != Tile_Grass)
			return -1;

		/// \TODO Check if tower, then return ~10 - 100 - or something.
		/// So that blocked customers can walk through blocking towers.
		if (std::find(blocked.begin(), blocked.end(), a) != blocked.end())
			return 100;

		return 1;
	};

	struct AStarNode
	{
		sf::Vector2i Parent;
		sf::Vector2i Pos;
		float Priority;

		inline bool operator==(const sf::Vector2i& rhs) const
		{
			return Pos == rhs;
		}
		inline bool operator!=(const sf::Vector2i& rhs) const
		{
			return Pos != rhs;
		}
		inline bool operator<(const AStarNode& rhs) const
		{
			return Priority < rhs.Priority;
		}
	};

	AStarNode start{ from, from, 0 };

	std::unordered_map<sf::Vector2i, AStarNode> openList;
	auto cheapest = [&openList]() -> AStarNode
	{
		AStarNode lowest = { {}, {}, FLT_MAX };
		for (auto& it : openList)
			if (it.second < lowest)
				lowest = it.second;

		return lowest;
	};

	std::unordered_map<sf::Vector2i, AStarNode> closedSet;
	
	openList.emplace(from, AStarNode{ from, from, 0 });
	while (!openList.empty() && cheapest() != to)
	{
		auto cur = cheapest();
		openList.erase(cur.Pos);

		closedSet[cur.Pos] = cur;
		
		sf::Vector2i neighbors[6] = {
			cur.Pos + sf::Vector2i(-1, 0),
			cur.Pos + sf::Vector2i(1, 0),
			cur.Pos + sf::Vector2i(0, -1),
			cur.Pos + sf::Vector2i(0, 1),

			cur.Pos + sf::Vector2i(-1, ((cur.Pos.x & 1) ? -1 : 1)),
			cur.Pos + sf::Vector2i(1, ((cur.Pos.x & 1) ? -1 : 1))
		};

		for (auto& n : neighbors)
		{
			if (cost(n) < 0)
				continue;

			float g = cur.Priority + cost(n) + heuristic(cur.Pos, n);
			
			if (openList.count(n) > 0 && openList[n].Priority > g)
				openList.erase(n);
			if (closedSet.count(n) > 0 && closedSet[n].Priority > g)
				closedSet.erase(n);

			if (closedSet.count(n) == 0 && openList.count(n) == 0)
			{
				AStarNode ne;
				ne.Pos = n;
				ne.Priority = g + heuristic(n, to);
				ne.Parent = cur.Pos;

				openList[n] = ne;
			}
		}
	}

	if (openList.empty())
		return Path::Invalid;

	// Follow Parent pointers back to start
	auto curPoint = cheapest();
	
	do
	{
		ret.push(curPoint.Pos);
		curPoint = closedSet[curPoint.Parent];
	} while (curPoint.Pos != from);
	ret.push(from);
	ret.finish();

	return ret;
}

