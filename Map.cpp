#include "Map.h"
#include "json.h"
#include <sstream>

constexpr int TEXTURE_SIDE = 40;

Map::Map(const std::string& jsonStructureData, const std::string& jsonCoordinatesData, const std::string& jsonDynamicData, TextureManager& textureManager) : 
		Graph{ jsonStructureData, jsonCoordinatesData },
		textureManager{ textureManager } {
	posts.resize(adjacencyList.size(), { Post::PostTypes::NONE, 0, "", 0 });
	Update(jsonDynamicData);
	for (int i = 0; i < adjacencyList.size(); ++i) {
		if (posts[i].type == Post::PostTypes::MARKET) {
			markets.insert(i);
		}
		else if (posts[i].type == Post::PostTypes::STORAGE) {
			storages.insert(i);
		}
		else if (posts[i].type == Post::PostTypes::TOWN) {
			towns.insert(i);
		}
	}
}

std::pair<int, double> Map::GetBestMarket(int from, int home, double maxLoad, const std::unordered_set<int>& vBlackList, const std::unordered_set<edge> eBlackList, int dist, int onPathTo) {
	double maxTime = posts[home].goodsLoad / posts[home].populationLoad;
	int bestIdx = -1;
	double bestK = 0;
	for (int i = 0; i < posts.size(); ++i) {
		if (posts[i].type != Post::PostTypes::MARKET) {
			continue;
		}
		if (vBlackList.count(i) != 0) {
			continue;
		}
		double k = GetMarketK(from, i, home, maxLoad, vBlackList, eBlackList, dist, onPathTo);
		if (k > bestK || bestIdx == -1) {
			bestIdx = i;
			bestK = k;
		}
	}
	return { bestIdx, bestK };
}

std::pair<int, double> Map::GetBestStorage(int from, int home, double maxLoad, const std::unordered_set<int>& vBlackList, const std::unordered_set<edge> eBlackList, int dist, int onPathTo) {
	int bestIdx = -1;
	double bestK = 0;
	for (int i = 0; i < posts.size(); ++i) {
		if (posts[i].type != Post::PostTypes::STORAGE) {
			continue;
		}
		if (vBlackList.count(i) != 0) {
			continue;
		}
		double k = GetStorageK(from, i, home, maxLoad, vBlackList, eBlackList, dist, onPathTo);
		if (k > bestK || bestIdx == -1) {
			bestIdx = i;
			bestK = k;
		}
	}
	return { bestIdx, bestK };
}

int Map::GetArmor(int idx) {
	return posts[idx].armorLoad;
}

int Map::GetProduct(int idx) {
	return posts[idx].goodsLoad;
}

int Map::GetLevel(int idx)
{
	return posts[idx].level;
}

int Map::GetPopulation(int idx) {
	return posts[idx].populationLoad;
}

int Map::GetNextLevelPrice(int idx)
{
	return posts[idx].nextLevelPrice;
}

int Map::GetPostIdx(int idx)
{
	return posts[idx].idx;
}

Post::PostTypes Map::GetPostType(int idx) {
	return posts[idx].type;
}

const std::unordered_set<int>& Map::GetMarkets() {
	return markets;
}

double Map::GetMarketK(int from, int idx, int homeIdx, double maxLoad, const std::unordered_set<int>& vBlackList, const std::unordered_set<edge> eBlackList, int dist, int onPathTo) {
	std::unordered_set<int> forbidden = storages;
	forbidden.insert(vBlackList.begin(), vBlackList.end());
	double distanceTo = *GetDistance(from, idx, forbidden, eBlackList, dist, onPathTo);
	double distanceFrom = GetDistance(idx, homeIdx);
	double freeSpace = maxLoad;
	freeSpace -= std::min(posts[idx].goodsLoad + posts[idx].refillRate * distanceTo, posts[idx].goodsCapacity);
	freeSpace = std::max(0.0, freeSpace);
	double waitTime = freeSpace / posts[idx].refillRate;
	double gain = maxLoad - posts[homeIdx].populationLoad * (distanceTo + distanceFrom + waitTime);
	return gain / (distanceFrom + distanceTo + waitTime);
}

double Map::GetStorageK(int from, int idx, int homeIdx, double maxLoad, const std::unordered_set<int>& vBlackList, const std::unordered_set<edge> eBlackList, int dist, int onPathTo) {
	std::unordered_set<int> forbidden = markets;
	forbidden.insert(vBlackList.begin(), vBlackList.end());
	double distanceTo = *GetDistance(from, idx, forbidden, eBlackList, dist, onPathTo);
	double distanceFrom = GetDistance(idx, homeIdx);
	double freeSpace = maxLoad;
	freeSpace -= std::min(posts[idx].armorLoad + posts[idx].refillRate * distanceTo, posts[idx].armorCapacity);
	freeSpace = std::max(0.0, freeSpace);
	double waitTime = freeSpace / posts[idx].refillRate;
	return maxLoad / (distanceFrom + distanceTo + waitTime);
}

const std::unordered_set<int>& Map::GetStorages() {
	return storages;
}

const std::unordered_set<int>& Map::GetTowns() {
	return towns;
}

void Map::Draw(SdlWindow& window) {
	DrawEdges(window);
	int k = 0;
	for (int i = 0; i < posts.size(); ++i) {
		SDL_Texture* texture = nullptr;
		int textureSide = TEXTURE_SIDE;
		int offsetY = 0;
		switch (posts[i].type) {
		case Post::PostTypes::NONE:
			offsetY -= TEXTURE_SIDE * 0.3;
			texture = textureManager["assets//none.png"];
			break;
		case Post::PostTypes::TOWN:

			switch (posts[i].level) {
			case 1:
				texture = textureManager["assets//town1.png"];
				break;
			case 2:
				texture = textureManager["assets//town2.png"];
				break;
			case 3:
				texture = textureManager["assets//town3.png"];
				break;
			default:
				texture = textureManager["assets//town1.png"];
			}
			textureSide *= 2;
			offsetY -= TEXTURE_SIDE * 0.66;
			break;
		case Post::PostTypes::MARKET:
			texture = textureManager["assets//market.png"];
			break;
		case Post::PostTypes::STORAGE:
			texture = textureManager["assets//storage.png"];
			break;
		}
		 
		window.DrawTexture(adjacencyList[i].point.x, adjacencyList[i].point.y, textureSide, textureSide, texture, offsetY); 
		switch (posts[i].type) {
		case Post::PostTypes::NONE:
			break;
		case Post::PostTypes::TOWN:
		{
			int x = adjacencyList[i].point.x;
			int y = adjacencyList[i].point.y;
			window.SetDrawColor(255, 0, 0);
			window.DrawRectangle(x, y, 15, textureSide, -textureSide);
			window.SetDrawColor(0, 255, 0);
			window.FillRectangle(x, y, 5, textureSide * (posts[i].goodsLoad / posts[i].goodsCapacity), -textureSide);
			window.SetDrawColor(0, 0, 255);
			window.FillRectangle(adjacencyList[i].point.x, adjacencyList[i].point.y, 5, textureSide * (posts[i].armorLoad / posts[i].armorCapacity), 5 - textureSide);
			window.SetDrawColor(255, 0, 255);
			window.FillRectangle(adjacencyList[i].point.x, adjacencyList[i].point.y, 5, textureSide * (posts[i].populationLoad / posts[i].populationCapacity), -(5 + textureSide));
		}
			break;
		case Post::PostTypes::MARKET:
			window.SetDrawColor(255, 0, 0);
			window.DrawRectangle(adjacencyList[i].point.x, adjacencyList[i].point.y, 5, textureSide, -textureSide / 1.5);
			window.SetDrawColor(0, 255, 0);
			window.FillRectangle(adjacencyList[i].point.x, adjacencyList[i].point.y, 5, textureSide * (posts[i].goodsLoad / posts[i].goodsCapacity), -textureSide / 1.5);
			break;
		case Post::PostTypes::STORAGE:
			window.SetDrawColor(255, 0, 0);
			window.DrawRectangle(adjacencyList[i].point.x, adjacencyList[i].point.y, 5, textureSide, -textureSide / 1.5);
			window.SetDrawColor(0, 0, 255);
			window.FillRectangle(adjacencyList[i].point.x, adjacencyList[i].point.y, 5, textureSide * (posts[i].armorLoad / posts[i].armorCapacity), -textureSide / 1.5);
			break;
		}
	}
}

void Map::Update(const std::string& jsonDynamicData) {
	try {
		std::stringstream ss;
		ss << jsonDynamicData;
		Json::Document doc = Json::Load(ss);
		auto nodeMap = doc.GetRoot().AsMap();
		for (const auto& node : nodeMap["posts"].AsArray()) {
			auto postMap = node.AsMap();
			posts[TranslateVertexIdx(postMap["point_idx"].AsInt())] = { static_cast<Post::PostTypes>(postMap["type"].AsInt()),
				static_cast<size_t>(postMap["idx"].AsInt()),
				postMap["name"].AsString(), static_cast<size_t>(postMap["point_idx"].AsInt()) };
			if (posts[TranslateVertexIdx(postMap["point_idx"].AsInt())].type == Post::PostTypes::TOWN) {
				posts[TranslateVertexIdx(postMap["point_idx"].AsInt())].goodsCapacity = postMap["product_capacity"].AsDouble();
				posts[TranslateVertexIdx(postMap["point_idx"].AsInt())].goodsLoad = postMap["product"].AsDouble();
				posts[TranslateVertexIdx(postMap["point_idx"].AsInt())].armorCapacity = postMap["armor_capacity"].AsDouble();
				posts[TranslateVertexIdx(postMap["point_idx"].AsInt())].armorLoad = postMap["armor"].AsDouble();
				posts[TranslateVertexIdx(postMap["point_idx"].AsInt())].populationCapacity = postMap["population_capacity"].AsDouble();
				posts[TranslateVertexIdx(postMap["point_idx"].AsInt())].populationLoad = postMap["population"].AsDouble();
				posts[TranslateVertexIdx(postMap["point_idx"].AsInt())].level = postMap["level"].AsInt();
				posts[TranslateVertexIdx(postMap["point_idx"].AsInt())].nextLevelPrice = postMap["next_level_price"].AsInt();
			}
			else if(posts[TranslateVertexIdx(postMap["point_idx"].AsInt())].type == Post::PostTypes::MARKET) {
				posts[TranslateVertexIdx(postMap["point_idx"].AsInt())].goodsCapacity = postMap["product_capacity"].AsDouble();
				posts[TranslateVertexIdx(postMap["point_idx"].AsInt())].goodsLoad = postMap["product"].AsDouble();
				posts[TranslateVertexIdx(postMap["point_idx"].AsInt())].refillRate = postMap["replenishment"].AsDouble();
			}
			else if (posts[TranslateVertexIdx(postMap["point_idx"].AsInt())].type == Post::PostTypes::STORAGE) {
				posts[TranslateVertexIdx(postMap["point_idx"].AsInt())].armorCapacity = postMap["armor_capacity"].AsDouble();
				posts[TranslateVertexIdx(postMap["point_idx"].AsInt())].armorLoad = postMap["armor"].AsDouble();
			}
		}
	}
	catch (...) {
		throw std::runtime_error{ "Map::Update error" };
	}
}
