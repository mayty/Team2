#include "graph.h"
#include "json.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <queue>

constexpr double PI = 3.141592653589793238463;
constexpr double X_MIDDLE = 400;
constexpr double Y_MIDDLE = 300;
constexpr double R = std::min(X_MIDDLE - 30, Y_MIDDLE - 30);

Graph::Graph(const std::string& filename) {
	std::ifstream in(filename);
	ParseStructure(in);
}

Graph::Graph(const std::string& jsonStructureData, const std::string& jsonCoordinatesData) {
	{
		std::stringstream ss;
		ss << jsonStructureData;
		ParseStructure(ss);
	}
	{
		std::stringstream ss;
		ss << jsonCoordinatesData;
		ParseCoordinates(ss);
	}
	spTrees.resize(adjacencyList.size());
}

int Graph::TranslateVertexIdx(size_t idx) const {
	return idxConverter.at(idx);
}

int Graph::GetEdgeIdx(int from, int to) const {
	for (const auto& i : adjacencyList[from].edges) {
		if (i.to == to) {
			return i.idx;
		}
	}
	return 0;
}

double Graph::GetDistance(int from, int to) const {
	if (from == to) {
		return 0.0;
	}
	if (spTrees[from].empty()) {
		spTrees[from] = GenerateSpTree(from);
	}
	return spTrees[from][to].length;
}

std::optional<double> Graph::GetDistance(int from, int to, const std::unordered_set<int>& verticesBlackList, const std::unordered_set<edge>& edgesBlackList, int dist, int onPathTo) const {
	auto blackList = verticesBlackList;
	if (blackList.count(to)) {
		blackList.erase(to);
	}
	if (blackList.count(from)) {
		blackList.erase(from);
	}
	auto ans = GenerateSpTree(from, blackList, edgesBlackList);
	if (dist != 0 && edgesBlackList.count({ from, onPathTo }) == 0) {
		auto buf = GenerateSpTree(onPathTo, verticesBlackList, edgesBlackList);
		if (ans[to].length != -1) {
			ans[to].length += dist;
		}
		double edgeLen = 0;
		for (const auto& edge : adjacencyList[from].edges) {
			if (edge.to == onPathTo) {
				edgeLen = edge.length;
				break;
			}
		}
		if (buf[to].length != -1) {
			buf[to].length += edgeLen - dist;
		}
		if ((ans[to].length == -1) || ((buf[to].length != -1) && (buf[to].length < ans[to].length))) {
			ans = std::move(buf);
		}
	}
	if (ans[to].length == -1) {
		return std::nullopt;
	}
	return ans[to].length;
}

std::optional<int> Graph::GetNextOnPath(int from, int to, const std::unordered_set<int>& verticesBlackList, const std::unordered_set<edge>& edgesBlackList, int dist, int onPathTo) const {
	if (from == to) {
		return to;
	}
	auto blackList = verticesBlackList;
	if (blackList.count(to)) {
		blackList.erase(to);
	}
	if (blackList.count(from)) {
		blackList.erase(from);
	}
	auto ans = GenerateSpTree(from, blackList, edgesBlackList);
	if (dist != 0 && edgesBlackList.count({ from, onPathTo }) == 0) {
		auto buf = GenerateSpTree(onPathTo, verticesBlackList, edgesBlackList);
		if (ans[to].length != -1) {
			ans[to].length += dist;
		}
		double edgeLen = 0;
		for (const auto& edge : adjacencyList[from].edges) {
			if (edge.to == onPathTo) {
				edgeLen = edge.length;
				break;
			}
		}
		if (buf[to].length != -1) {
			buf[to].length += edgeLen - dist;
		}
		if ((ans[to].length == -1) || ((buf[to].length != -1) && (buf[to].length < ans[to].length))) {
			ans = std::move(buf);
		}
	}
	if (ans[to].length == -1) {
		return std::nullopt;
	}
	return GetNextOnPath(ans, from, to);
}

std::pair<int, int> Graph::GetEdgeVertices(int originalEdgeIdx) const {
	return edgesData.at(originalEdgeIdx);
}

double Graph::GetEdgeLength(int originalEdgeIdx) const {
	for (const auto& j : adjacencyList[GetEdgeVertices(originalEdgeIdx).first].edges) {
		if (j.idx == originalEdgeIdx) {
			return j.length;
		}
	}
	return 0.0;
}

std::pair<double, double> Graph::GetPointCoord(int localPointIdx) const {
	return std::pair<double, double>{adjacencyList[localPointIdx].point.x, adjacencyList[localPointIdx].point.y};
}

void Graph::AddEdge(size_t from, Vertex::Edge edge) {
	auto pos = std::find_if(begin(adjacencyList[from].edges), end(adjacencyList[from].edges), [edge](const Vertex::Edge& cur) {return cur.to < edge.to; });
	if (pos == end(adjacencyList[from].edges)) {
		adjacencyList[from].edges.push_back(edge);
	} else {
		adjacencyList[from].edges.insert(pos, edge);
	}
}

std::vector<Graph::spData> Graph::GenerateSpTree(int origin, const std::unordered_set<int>& verticesBlackList, const std::unordered_set<edge>& edgesBlackList) const {
	std::vector <spData> ans(adjacencyList.size(), { -1, -1 });
	struct dijkstraData {
		int idx;
		int prev;
		double length;
	};
	auto comparator = [](const dijkstraData& lhs, const dijkstraData& rhs) {return lhs.length > rhs.length; };
	std::priority_queue<dijkstraData, std::vector<dijkstraData>, decltype(comparator)> dijkstra(comparator);
	dijkstra.push({ origin, -1, 0 });
	for (size_t i = 0; i < adjacencyList.size(); i++) {
		dijkstraData cur = { origin, -1, 0 };
		while (((ans[cur.idx].length != -1) || ((verticesBlackList.count(cur.idx) != 0) && (cur.idx != origin)) || 
			(edgesBlackList.count(std::make_pair(cur.prev, cur.idx)))) && (!dijkstra.empty())) {
			cur = dijkstra.top();
			dijkstra.pop();
		}
		if (dijkstra.empty()) {
			break;
		}
		ans[cur.idx] = { cur.prev, cur.length };
		for (const auto& edge : adjacencyList[cur.idx].edges) {
			if (ans[edge.to].length == -1) {
				dijkstra.push({ static_cast<int>(edge.to), cur.idx, ans[cur.idx].length + edge.length });
			}
		}
	}
	return ans;
}

int Graph::GetNextOnPath(const std::vector<spData>& spTree, int from, int to) const {
	int ans = to;
	while ((spTree[ans].prevVertex != from) && (spTree[ans].prevVertex != -1)) {
		ans = spTree[ans].prevVertex;
	}
	return ans;
}

void Graph::DrawEdges(SdlWindow& window) {
	for (int i = 0; i < adjacencyList.size(); ++i) {
		for (const auto& j : adjacencyList[i].edges) {
			if (j.to < i) {
				break;
			}
			window.SetDrawColor(255, 255, 255);
			window.DrawLine(std::round(adjacencyList[i].point.x), std::round(adjacencyList[i].point.y), std::round(adjacencyList[j.to].point.x), std::round(adjacencyList[j.to].point.y));
		}
	}
}

void Graph::ParseStructure(std::istream& input) {
	Json::Document document = Json::Load(input);
	auto nodeMap = document.GetRoot().AsMap();
	adjacencyList.reserve(nodeMap["points"].AsArray().size());
	double phi = 0;
	double phi_step = 2 * PI / nodeMap["points"].AsArray().size();
	for (const auto& vertexNode : nodeMap["points"].AsArray()) {
		auto vertexMap = vertexNode.AsMap();
		idxConverter[vertexMap["idx"].AsInt()] = adjacencyList.size();
		adjacencyList.push_back({ static_cast<size_t>(vertexMap["idx"].AsInt()), std::nullopt, std::list<Vertex::Edge>(),
			{X_MIDDLE + R * std::cos(phi), Y_MIDDLE + R * std::sin(phi)} });
		if (!vertexMap["post_idx"].IsNull()) {
			adjacencyList.back().postIdx = static_cast<size_t>(vertexMap["post_idx"].AsInt());
		}
		phi += phi_step;
	}
	for (const auto& edgeNode : nodeMap["lines"].AsArray()) {
		auto edgeMap = edgeNode.AsMap();
		size_t from = TranslateVertexIdx(edgeMap["points"].AsArray()[0].AsInt());
		Vertex::Edge edge(edgeMap["idx"].AsInt(), TranslateVertexIdx(edgeMap["points"].AsArray()[1].AsInt()), edgeMap["length"].AsDouble());
		edgesData[edge.idx] = { from, edge.to };
		AddEdge(from, edge);
		std::swap(from, edge.to);
		AddEdge(from, edge);
		maxLength = std::max(maxLength, edge.length);
	}
}

void Graph::ParseCoordinates(std::istream& input) {
	Json::Document document = Json::Load(input);
	auto nodeMap = document.GetRoot().AsMap();
	for (const auto& node : nodeMap["coordinates"].AsArray()) {
		auto coordMap = node.AsMap();
		size_t curIdx = TranslateVertexIdx(coordMap["idx"].AsInt());
		adjacencyList[curIdx].point.x = coordMap["x"].AsDouble();
		adjacencyList[curIdx].point.y = coordMap["y"].AsDouble();
	}
	auto sizeArray = nodeMap["size"].AsArray();
	width = sizeArray[0].AsDouble();
	height = sizeArray[1].AsDouble();
}
