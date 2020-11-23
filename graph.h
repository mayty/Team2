#pragma once
#include <vector>
#include <list>
#include <mutex>
#include <optional>
#include <atomic>
#include <map>
#include "SDL_window.h"

class Graph { // class for working with graphs
protected:
    struct Vertex {
        struct Edge {
            Edge(size_t idx, size_t to, double length) :idx{ idx }, to { to }, length{ length } {}
            size_t idx;
            size_t to;
            double length;
        };
        struct Point {
            double x;
            double y;
        };
        size_t originalIdx;
        std::optional<size_t> postIdx;
        std::list<Edge> edges;
        Point point;
    };
    std::vector<Vertex> adjacencyList;
    double maxLength = 0;
    std::mutex writeLock;
    std::map<size_t, size_t> idxConverter;
    double width;
    double height;
public:
    explicit Graph(const std::string& filename); // creates graph with points in circular layout from file with json data
    Graph(const std::string& jsonStructureData, const std::string& jsonCoordinatesData);
    int TranslateVertexIdx(size_t idx) const;
    int TranslateEdgeIdx(size_t idx) const;
    std::pair<int, int> GetEdgeVertices(int originalEdgeIdx); // returns local from-to idx pair
    std::pair<double, double> GetPointCoord(int localPointIdx); // returns x-y pair
    virtual void Draw(SdlWindow& window); // draws current graph
    void DrawEdges(SdlWindow& window);
    double ApplyForce(); // applies forces to vertices
    virtual ~Graph() = default;
private:
    void ParseStructure(std::istream& input);
    void ParseCoordinates(std::istream& input);
    void AddEdge(size_t from, Vertex::Edge edge);
};

