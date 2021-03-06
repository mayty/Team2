#include "GameWorld.h"
#include "json.h"
#include <sstream>
#include <algorithm>
#include <unordered_set>

#define NO_BUG_COLLISION

#define PATHFINDING_DEBUG

#ifdef _DEBUG
#ifdef PATHFINDING_DEBUG
#define _PATHFINDING_DEBUG
#endif
#endif

void makeMoveRequest(int lineIdx, int speed, int trainIdx, ServerConnection& connection) {
	try {
		if (!connection.IsEstablished()) {
			connection.Establish();
		}
		connection.MoveTrain(lineIdx, speed, trainIdx);
	}
	catch (std::runtime_error& error) {
		std::cout << error.what() << std::endl;
	}
}

GameWorld::GameWorld(const std::string& playerName, const std::string& gameName, int playerCount, int numTurns, TextureManager& textureManager) : 
		connection{ playerName, playerCount, gameName, numTurns }, textureManager { textureManager },
		map{ connection.GetMapStaticObjects(), connection.GetMapCoordinates(), connection.GetMapDynamicObjects(), textureManager } {
	Update(connection.GetMapDynamicObjects());
}

double GameWorld::GetScore() {
	double score = map.GetPopulation(map.TranslateVertexIdx(connection.GetHomeIdx())) * 1000.0;
	score += spentArmor * 4;
	score += map.GetArmor(map.TranslateVertexIdx(connection.GetHomeIdx()));
	score += map.GetProduct(map.TranslateVertexIdx(connection.GetHomeIdx()));

	return score;
}

void GameWorld::Update() {
	Update(connection.GetMapDynamicObjects());
}

void GameWorld::Draw(SdlWindow& window) {
	map.Draw(window);
	DrawTrains(window);
}

void GameWorld::MakeMove() {
	takenPosts.clear();
	++gameTick;
	for (const auto& [idx, target] : trainsTargets) {
		takenPosts.insert(target);
	}
	int armor = map.GetArmor(map.TranslateVertexIdx(connection.GetHomeIdx()));
	auto town = GetPosition(map.TranslateVertexIdx(connection.GetHomeIdx()));
	std::vector<size_t> trainsToUpgrade;
	std::vector<size_t> townsToUpgrade;
	double prevSpent = spentArmor;
	
	bool allTrainsUpgraded = true;
	for (const auto& i : trains) {
		if (i.level >= 3) {
			continue;
		}
		allTrainsUpgraded = false;
		if (town != GetPosition(i.lineIdx, i.position)) {
			continue;
		}
		if (i.nextLevelPrice <= armor) {
			armor -= i.nextLevelPrice;
			spentArmor += i.nextLevelPrice;
			trainsToUpgrade.push_back(i.idx);
		}
	}

	if (map.GetLevel(map.TranslateVertexIdx(connection.GetHomeIdx())) < 2) {
		int price = map.GetNextLevelPrice(map.TranslateVertexIdx(connection.GetHomeIdx()));
		if (price <= armor) {
			townsToUpgrade.push_back(map.GetPostIdx(map.TranslateVertexIdx(connection.GetHomeIdx())));
			armor -= price;
			spentArmor += price;
		}
	}
	else if (map.GetLevel(map.TranslateVertexIdx(connection.GetHomeIdx())) < 3 && allTrainsUpgraded) {
		int price = map.GetNextLevelPrice(map.TranslateVertexIdx(connection.GetHomeIdx()));
		if (price <= armor) {
			armor -= price;
			if (armor > (500 - gameTick) / 25) {
				townsToUpgrade.push_back(map.GetPostIdx(map.TranslateVertexIdx(connection.GetHomeIdx())));
				spentArmor += price;
			}
		}
	}
	
	if (!trainsToUpgrade.empty() || !townsToUpgrade.empty()) {
		try {
			connection.Upgrade(townsToUpgrade, trainsToUpgrade);
		}
		catch (...) {
			spentArmor = prevSpent;
			--gameTick;
			throw;
		}
	}

	try {
		MoveTrains();
		connection.EndTurn();
	}
	catch (...) {
		spentArmor = prevSpent;
		--gameTick;
		throw;
	}
}

void GameWorld::Update(const std::string& jsonData) {
	map.Update(jsonData);
	whitePositions.clear();
	for (int i : map.GetTowns()) {
		whitePositions.insert(GetPosition(i));
	}
	UpdateTrains(jsonData);
}

void GameWorld::MoveTrains() {
#ifdef _PATHFINDING_DEBUG
	std::cout << std::endl << std::endl;
	std::cout << "point black list:";
	for (auto i : pointBlackList) {
		std::cout << std::endl << i;
	}
	std::cout << std::endl << "edges black list: ";
	for (auto i : edgesBlackList) {
		std::cout << std::endl << i.first << ' ' << i.second;
	}
#endif
	std::sort(trains.begin(), trains.end(), [](const Train& a, const Train& b) {return a.level > b.level; });
	std::vector<std::thread> helpThreads;
	std::vector<TrainMoveData> moveData;
	int trainsCount = 0;
	int count = 0;
	switch (map.GetPopulation(map.TranslateVertexIdx(connection.GetHomeIdx())))
	{
	case 0:
	case 1:
		marketsToFocus = 1;
		break;
	case 2:
	case 3:
	case 4:
		marketsToFocus = 2;
		break;
	case 5:
	case 6:
		marketsToFocus = 3;
		break;
	case 7:
	default:
		marketsToFocus = 4;
	}
	for (auto& i : trains) {
		if (i.cooldown != 0) {
			if (trainsTargets.count(i.idx)) {
				trainsTargets.erase(i.idx);
			}
			continue;
		}
		if (i.owner != connection.GetPlayerIdx()) {
			continue;
		}
		for (uint64_t i : whitePositions) {
			if (takenPositions.count(i)) {
				takenPositions.erase(i);
			}
		}
		for (int i : map.GetTowns()) {
			if (pointBlackList.count(i)) {
				pointBlackList.erase(i);
			}
		}
		if (auto trainMove = MoveTrain(i)) {
			++trainsCount;
			moveData.push_back(*trainMove);
		}
	}
	while (helpConnections.size() < trainsCount) {
		helpConnections.emplace_back(connection.GetLogin(), connection.GetPassword(), connection.GetGameName(), false, false);
	}
	for (int i = 0; i < trainsCount; ++i) {
		helpThreads.emplace_back(makeMoveRequest, std::get<0>(moveData[i]), std::get<1>(moveData[i]), std::get<2>(moveData[i]), std::ref(helpConnections[i]));
	}
	for (int i = 0; i < trainsCount; ++i) {
		helpThreads[i].join();
	}
}

std::optional<GameWorld::TrainMoveData> GameWorld::MoveTrain(Train& train) {
	if (train.load > 0 && train.load != train.capacity) {
		if (marketsToFocus) {
			--marketsToFocus;
		}
#ifdef _PATHFINDING_DEBUG
		std::cout << std::endl;
		std::cout << "idx: " << train.idx << "; Farming resources";
#endif
		return std::nullopt;
	}

	auto [source, onPathTo] = map.GetEdgeVertices(train.lineIdx);
	double dist = GetDistAndFixSource(train, source, onPathTo);

	int target = map.TranslateVertexIdx(connection.GetHomeIdx());
	if (train.load == 0) {
		bool toTargetMarket = false;
		if (trainsTargets.count(train.idx)) {
			if (map.GetPostType(trainsTargets[train.idx]) == Post::PostTypes::MARKET) {
				toTargetMarket = true;
			}
			takenPosts.erase(trainsTargets[train.idx]);
		}

		if (gameTick < 150) {
			target = map.GetBestStorage(source, target, train.capacity, takenPosts, edgesBlackList, dist, onPathTo).first;
			if (target == -1) {
				target = map.GetBestStorage(source, target, train.capacity, {}, edgesBlackList, dist, onPathTo).first;
			}
		}
		else if (marketsToFocus || toTargetMarket || train.level == 3) {
			target = map.GetBestMarket(source, target, train.capacity, {}, edgesBlackList, dist, onPathTo).first;
		}
		else {
			target = map.GetBestStorage(source, target, train.capacity, takenPosts, edgesBlackList, dist, onPathTo).first;
			if (target == -1) {
				target = map.GetBestStorage(source, target, train.capacity, {}, edgesBlackList, dist, onPathTo).first;
			}
		}
		takenPosts.insert(target);
		trainsTargets[train.idx] = target;
	}
	else {
		if (trainsTargets.count(train.idx)) {
			if (takenPosts.count(trainsTargets[train.idx])) {
				takenPosts.erase(trainsTargets[train.idx]);
			}
			trainsTargets.erase(train.idx);
		}
	}

	if (marketsToFocus) {
		--marketsToFocus;
	}
	return MoveTrainTo(train, target);
}

std::optional<GameWorld::TrainMoveData> GameWorld::MoveTrainTo(Train& train, int to) {
	auto [source, onPathTo] = map.GetEdgeVertices(train.lineIdx);
	double dist = GetDistAndFixSource(train, source, onPathTo);

#ifdef _PATHFINDING_DEBUG
	std::cout << std::endl;
	std::cout << "idx: " << train.idx;
	std::cout << "; source: " << source;
	std::cout << "; target: " << to;
#endif

	std::unordered_set<int> blackList;
	switch (map.GetPostType(to)) {
	case Post::PostTypes::MARKET:
		blackList = map.GetStorages();
		break;
	case Post::PostTypes::STORAGE:
		blackList = map.GetMarkets();
	}
	blackList.insert(pointBlackList.begin(), pointBlackList.end());
	auto blackPosts = blackList;
	for (auto [t, i] : trainsTargets) {
		if (i == to) {
			continue;
		}
		blackList.insert(i);
	}
	int next;
	if (auto nextOnPath = map.GetNextOnPath(source, to, blackList, edgesBlackList, dist, onPathTo)) {
		next = nextOnPath.value();
	}
	else if (auto nextOnPath = map.GetNextOnPath(source, to, blackPosts, {}, dist, onPathTo)) {
		next = nextOnPath.value();
	}
	else {
#ifdef _PATHFINDING_DEBUG
		std::cout << "; NO PATH ";
#endif
		return std::nullopt;
	}

#ifdef _PATHFINDING_DEBUG
	std::cout << "; via: " << next;
#endif

	if (train.position == 0 || train.position == map.GetEdgeLength(train.lineIdx)) {
		auto [first, second] = map.GetEdgeVertices(map.GetEdgeIdx(source, next));
		if (next == first) {
			return MoveTrainDir(train.idx, map.GetEdgeIdx(first, second), train.lineIdx, train.position, -1);
		}
		else {
			return MoveTrainDir(train.idx, map.GetEdgeIdx(first, second), train.lineIdx, train.position, 1);
		}
	}
	else {
		auto [first, second] = map.GetEdgeVertices(train.lineIdx);
		if (next == first) {
			return MoveTrainDir(train.idx, train.lineIdx, train.position, -1);
		}
		else if (next == second) {
			return MoveTrainDir(train.idx, train.lineIdx, train.position, 1);
		}
		else if (source == first) {
			return MoveTrainDir(train.idx, train.lineIdx, train.position, -1);
		}
		else if (source == second) {
			return MoveTrainDir(train.idx, train.lineIdx, train.position, 1);
		}
		else {
			throw std::runtime_error{ "wtf" };
		}
	}
}

GameWorld::TrainMoveData GameWorld::MoveTrainDir(int trainIdx, int lineIdx, double position, int dir) {
	uint64_t nextPosition = GetNextPosition(lineIdx, position, dir);
	if (takenPositions.count(nextPosition)) {
#ifdef _PATHFINDING_DEBUG
		std::cout << "; CAN'T MOVE ";
#endif
		return TrainMoveData{ lineIdx, 0, trainIdx };
	}
	uint64_t currentPosition = GetPosition(lineIdx, position);
	if (takenPositions.count(currentPosition)) {
		takenPositions.erase(currentPosition);
	}
	takenPositions.insert(nextPosition);
	return TrainMoveData{ lineIdx, dir, trainIdx };
}

GameWorld::TrainMoveData GameWorld::MoveTrainDir(int trainIdx, int lineIdx, int prevLineIdx, double position, int dir) {
	uint64_t nextPosition = GetNextPosition(prevLineIdx, lineIdx, position, dir);
	if (takenPositions.count(nextPosition)) {
#ifdef _PATHFINDING_DEBUG
		std::cout << "; CAN'T MOVE ";
#endif
		return TrainMoveData{ lineIdx, 0, trainIdx };
	}
	uint64_t currentPosition = GetPosition(prevLineIdx, position);
	if (takenPositions.count(currentPosition)) {
		takenPositions.erase(currentPosition);
	}
	takenPositions.insert(nextPosition);
	return TrainMoveData{ lineIdx, dir, trainIdx };
}

double GameWorld::GetDistAndFixSource(const Train& train, int& source, int& onPathTo) {
	auto [first, second] = map.GetEdgeVertices(train.lineIdx);
	double dist = train.position;
	if (train.position >= map.GetEdgeLength(train.lineIdx) / 2) {
		std::swap(source, onPathTo);
		dist = map.GetEdgeLength(train.lineIdx) - train.position;
	}
	if (first == source) {
		for (int i = 0; i < train.position; ++i) {
			if (takenPositions.count(GetPosition(train.lineIdx, i))) {
				std::swap(source, onPathTo);
				dist = map.GetEdgeLength(train.lineIdx) - train.position;
			}
		}
	}
	else {
		for (int i = map.GetEdgeLength(train.lineIdx) - 0.5; i > train.position; --i) {
			if (takenPositions.count(GetPosition(train.lineIdx, i))) {
				std::swap(source, onPathTo);
				dist = train.position;
			}
		}
	}
	return dist;
}

void GameWorld::UpdateTrains(const std::string& jsonData) {
	std::stringstream ss;
	ss << jsonData;
	Json::Document doc = Json::Load(ss);
	auto nodeMap = doc.GetRoot().AsMap();
	trainIdxConverter.clear();
	trains.clear();
	edgesBlackList.clear();
	pointBlackList.clear();
	takenPositions.clear();
	trains.reserve(nodeMap["trains"].AsArray().size());
	for (const auto& node : nodeMap["trains"].AsArray()) {
		auto trainMap = node.AsMap();
		trainIdxConverter[trainMap["idx"].AsInt()] = trains.size();
		trains.emplace_back( static_cast<size_t>(trainMap["idx"].AsInt()), static_cast<size_t>(trainMap["line_idx"].AsInt()), 
			trainMap["position"].AsDouble(), trainMap["speed"].AsDouble());
		Train& train = *trains.rbegin();
		train.capacity = trainMap["goods_capacity"].AsDouble();
		train.load = trainMap["goods"].AsDouble();
		train.owner = trainMap["player_idx"].AsString();
		train.cooldown = trainMap["cooldown"].AsInt();
		train.level = trainMap["level"].AsInt();


		if (train.owner == connection.GetPlayerIdx()) {
			if (train.level < 3) {
				train.nextLevelPrice = trainMap["next_level_price"].AsInt();
			}
			takenPositions.insert(GetPosition(train.lineIdx, train.position));
		}
		else {
			takenPositions.insert(GetPosition(train.lineIdx, train.position));
#ifdef NO_BUG_COLLISION
			if (train.speed != 0) {
				takenPositions.insert(GetNextPosition(train.lineIdx, train.position, train.speed));
			}
			else {
				takenPositions.insert(GetNextPosition(train.lineIdx, train.position, 1));
				takenPositions.insert(GetNextPosition(train.lineIdx, train.position, -1));
			}
#else
			takenPositions.insert(GetNextPosition(train.lineIdx, train.position, train.speed));
#endif
		}

		if (train.speed == -1.0) {
			edgesBlackList.insert(map.GetEdgeVertices(train.lineIdx));
		}
		else if (train.speed == 1.0) {
			std::pair<int, int> edge = map.GetEdgeVertices(train.lineIdx);
			std::swap(edge.first, edge.second);
			edgesBlackList.insert(edge);
		}
		else {
			if (train.position == 0) {
				pointBlackList.insert(map.GetEdgeVertices(train.lineIdx).first);
			}
			else if (train.position == map.GetEdgeLength(train.lineIdx)) {
				pointBlackList.insert(map.GetEdgeVertices(train.lineIdx).second);
			}
			else {
				std::pair<int, int> edge = map.GetEdgeVertices(train.lineIdx);
				edgesBlackList.insert(edge);
				std::swap(edge.first, edge.second);
				edgesBlackList.insert(edge);
			}
		}
	}
}

void GameWorld::DrawTrains(SdlWindow& window) {
	int k = 0;
	for (const auto& i : trains) {
		double from, to;
		std::pair<int, int> vertices = map.GetEdgeVertices(i.lineIdx);
		std::pair<double, double> a = map.GetPointCoord(vertices.first);
		std::pair<double, double> b = map.GetPointCoord(vertices.second);
		if (i.speed >= 0) {
			from = a.first;
			to = b.first;
		}
		else {
			from = b.first;
			to = a.first;
		}

		bool toMirror;
		if (from > to) {
			toMirror = true;
		}
		else {
			toMirror = false;
		}
		double edgeLength = map.GetEdgeLength(i.lineIdx);
		double x = a.first + (b.first - a.first) * (i.position / edgeLength);
		double y = a.second + (b.second - a.second) * (i.position / edgeLength);
		SDL_Texture* texture;
		switch (i.level) {
		case 1:
			texture = textureManager["assets\\train1.png"];
			break;
		case 2:
			texture = textureManager["assets\\train2.png"];
			break;
		case 3:
			texture = textureManager["assets\\train3.png"];
			break;
		default:
			texture = textureManager["assets\\train1.png"];
		}
		window.DrawTexture(x, y, 40, 40, texture, 0.0, toMirror);
	}
}

uint64_t GameWorld::GetPosition(int vertex) {
	uint64_t result = 0;
	result |= vertex;
	return result;
}

uint64_t GameWorld::GetPosition(int lineIdx, double position) {
	auto [first, second] = map.GetEdgeVertices(lineIdx);
	double lineLength = map.GetEdgeLength(lineIdx);
	uint64_t result = 0;
	if (position == 0.0) {
		result |= first;
	}
	else if (position == lineLength) {
		result |= second;
	}
	else {
		result |= lineIdx;
		int positionInt = position;
		result <<= 32;
		result |= positionInt;
	}
	return result;
}

uint64_t GameWorld::GetNextPosition(int lineIdx, double position, int speed) {
	if (speed == 0) {
		return GetPosition(lineIdx, position);
	}

	if (position == 0.0 && speed == -1) {
		return GetPosition(lineIdx, position);
	}

	if (position == map.GetEdgeLength(lineIdx) && speed == 1) {
		return GetPosition(lineIdx, position);
	}

	return GetPosition(lineIdx, position + speed);
}

uint64_t GameWorld::GetNextPosition(int prevLineIdx, int lineIdx, double position, int speed) {
	int source;
	if (position == 0) {
		source = map.GetEdgeVertices(prevLineIdx).first;
	}
	else {
		source = map.GetEdgeVertices(prevLineIdx).second;
	}
	auto [first, second] = map.GetEdgeVertices(lineIdx);
	if (source == first) {
		return GetNextPosition(lineIdx, 0.0, speed);
	}
	else {
		return GetNextPosition(lineIdx, map.GetEdgeLength(lineIdx), speed);
	}
}
