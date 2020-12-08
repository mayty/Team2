#include "GameWorld.h"
#include <sstream>
#include "json.h"
#include <unordered_set>


GameWorld::GameWorld(const std::string& playerName, TextureManager& textureManager) : connection{ playerName },	textureManager{textureManager},
		map{ connection.GetMapStaticObjects(), connection.GetMapCoordinates(), connection.GetMapDynamicObjects(), textureManager } {
	UpdateTrains(connection.GetMapDynamicObjects());
}
 
void GameWorld::Update() {
	std::string dynamicObjects = connection.GetMapDynamicObjects();
	map.Update(dynamicObjects);
	UpdateTrains(dynamicObjects);
}

void GameWorld::Draw(SdlWindow& window) {
	map.Draw(window);
	DrawTrains(window);
}

void GameWorld::MoveTrains() {
	for (auto& i : trains) {
		if (i.cooldown != 0) {
			continue;
		}
		if (i.owner != connection.GetPlayerIdx()) {
			continue;
		}
		MoveTrain(i);
		break;
	}
}

void GameWorld::MoveTrain(Train& train) {
	int to;
	int from;
	double edgeLength = map.GetEdgeLength(train.trueLineIdx);
	auto [first, second] = map.GetEdgeVertices(train.trueLineIdx);

	if (train.lineIdx != train.trueLineIdx) {
		auto [firstOld, secondOld] = map.GetEdgeVertices(train.lineIdx);
		if (first == firstOld) {
			train.truePosition = 0.0;
		}
		else if (first == secondOld) {
			train.truePosition = 0.0;
		}
		else if (second == firstOld) {
			train.truePosition = edgeLength;
		}
		else if (second == secondOld) {
			train.truePosition = edgeLength;
		}
	}

	if (train.truePosition >= edgeLength / 2) {
		from = second;
	}
	else {
		from = first;
	}

	if (train.load == 0) {
		if (trainsTargets.find(train.idx) != trainsTargets.end()) {
			takenPosts.erase(trainsTargets[train.idx]);
		}
		to = map.GetBestMarket(from, map.TranslateVertexIdx(connection.GetHomeIdx()), train.capacity - train.load, 0, takenPosts);
		takenPosts.insert(to);
		trainsTargets[train.idx] = to;
	}
	else {
		if (trainsTargets.find(train.idx) != trainsTargets.end()) {
			takenPosts.erase(trainsTargets[train.idx]);
			trainsTargets.erase(train.idx);
		}
		to = map.TranslateVertexIdx(connection.GetHomeIdx());
	}
	MoveTrainTo(train, to);
}

void GameWorld::MoveTrainTo(Train& train, int to) {
	auto [first, second] = map.GetEdgeVertices(train.trueLineIdx);
	if (train.truePosition == 0.0) {
		std::cout << "from: " << first;
		std::cout << "; to: " << to;
		to = *map.GetNextOnPath(first, to, map.GetStorages());
		std::cout << "; via: " << to << std::endl;
		if (to == first) {
			connection.MoveTrain(train.trueLineIdx, 0, train.idx);
		}
		else if (to == second) {
			connection.MoveTrain(train.trueLineIdx, 1, train.idx);
		}
		else {
			connection.MoveTrain(map.GetEdgeIdx(first, to), -1, train.idx);
			train.trueLineIdx = map.GetEdgeIdx(first, to);
			//MoveTrain(train);
		}
	}
	else if (train.truePosition == map.GetEdgeLength(train.trueLineIdx)) {
		std::cout << "from: " << second;
		std::cout << "; to: " << to;
		to = *map.GetNextOnPath(first, to, map.GetStorages());
		std::cout << "; via: " << to << std::endl;
		if (to == second) {
			connection.MoveTrain(train.trueLineIdx, 0, train.idx);
		}
		else if (to == first) {
			connection.MoveTrain(train.trueLineIdx, -1, train.idx);
		}
		else {
			connection.MoveTrain(map.GetEdgeIdx(second, to), 1, train.idx);
			train.trueLineIdx = map.GetEdgeIdx(second, to);
			//MoveTrain(train);
		}
	}
	else {
		if (train.speed > 0) {
			connection.MoveTrain(train.trueLineIdx, 1, train.idx);
		}
		else if (train.speed < 0){
			connection.MoveTrain(train.trueLineIdx, -1, train.idx);
		}
	}
}

void GameWorld::MakeMove() {
	takenPosts.clear();
	for (const auto& [idx, target] : trainsTargets) {
		takenPosts.insert(target);
	}
	takenPosts.insert(4);
	takenPosts.insert(16);
	MoveTrains();
	connection.EndTurn();
}

void GameWorld::UpdateTrains(const std::string& jsonData) {
	std::stringstream ss;
	ss << jsonData;
	Json::Document doc = Json::Load(ss);
	auto nodeMap = doc.GetRoot().AsMap();
	trainIdxConverter.clear();
	trains.clear();
	edgesBlackList.clear();
	trains.reserve(nodeMap["trains"].AsArray().size());
	allTrainsUpgraded = true;
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
				allTrainsUpgraded = false;
			}
		}
		else {

		}

		if (train.speed == 1.0) {
			edgesBlackList.insert(map.GetEdgeVertices(train.lineIdx));
		}
		else if (train.speed == -1.0) {
			std::pair<int, int> edge = map.GetEdgeVertices(train.lineIdx);
			std::swap(edge.first, edge.second);
			edgesBlackList.insert(edge);
		}
		else {
			std::pair<int, int> edge = map.GetEdgeVertices(train.lineIdx);
			edgesBlackList.insert(edge);
			std::swap(edge.first, edge.second);
			edgesBlackList.insert(edge);
		}
	}
}

void GameWorld::DrawTrains(SdlWindow& window) {
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
		if (i.speed != 0) {
			texture = textureManager["assets\\train.png"];
		}
		else {
			texture = textureManager["assets\\train_no_smoke.png"];
		}
		window.DrawTexture(x, y, 40, 40, texture, 0.0, toMirror);
	}
}
