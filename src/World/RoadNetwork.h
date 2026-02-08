#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <mutex>
#include <cmath>
#include <cstdint>

// Forward declarations
class WorldGenerator;

// A settlement node in the road graph
struct SettlementNode {
    float x, z;        // World position (center)
    bool isCity;        // true = city, false = village
    int groundY;        // Surface height at center
};

// A single waypoint on a road path (coarse grid)
struct RoadWaypoint {
    int x, z;           // World x,z position
    int y;              // Surface Y at this point
    bool isBridge;      // Over water
    bool isTunnel;      // Through mountain
};

// A road segment connecting two settlements
struct RoadSegment {
    int fromIdx, toIdx;                // Settlement indices
    std::vector<RoadWaypoint> path;    // Waypoints from A* (every ROAD_STEP blocks)
};

class RoadNetwork {
public:
    static constexpr int ROAD_STEP = 4;          // A* grid resolution (blocks)
    static constexpr float SEARCH_RADIUS = 900.0f; // How far to look for settlements to connect
    static constexpr int MAX_CONNECTIONS = 3;     // Max roads per settlement
    static constexpr int ROAD_HALF_WIDTH = 3;     // Half-width of road (total = 2*3+1 = 7, matches vxstruct)
    static constexpr int CITY_ROAD_HALF_WIDTH = 3; // Same width for city connections (total = 7)
    static constexpr float CITY_RADIUS = 160.0f;
    static constexpr float VILLAGE_RADIUS = 45.0f;
    
    RoadNetwork() = default;
    
    // Given a chunk position, find all road segments that might cross it
    // and place road blocks. Called from WorldGenerator::generate()
    void placeRoadsInChunk(
        int chunkBaseX, int chunkBaseZ, int chunkBaseY,
        const WorldGenerator& worldGen,
        std::shared_ptr<class Chunk> chunk
    );

private:
    // Cache of computed road segments: key = packed settlement pair
    struct PairKey {
        int64_t a, b;
        bool operator==(const PairKey& o) const { return a == o.a && b == o.b; }
    };
    struct PairKeyHash {
        size_t operator()(const PairKey& k) const {
            return std::hash<int64_t>()(k.a) ^ (std::hash<int64_t>()(k.b) << 16);
        }
    };
    
    std::unordered_map<PairKey, RoadSegment, PairKeyHash> m_roadCache;
    std::mutex m_cacheMutex;
    
    // Find all settlements within radius of given world position
    std::vector<SettlementNode> findSettlementsNear(
        float worldX, float worldZ, float radius,
        const WorldGenerator& worldGen
    ) const;
    
    // Build connectivity graph (MST-like) from settlements
    std::vector<std::pair<int,int>> buildRoadGraph(
        const std::vector<SettlementNode>& settlements
    ) const;
    
    // A* pathfinding between two world positions on coarse grid
    // startY/endY are the road surface heights at the settlement edges
    std::vector<RoadWaypoint> findRoadPath(
        float fromX, float fromZ, int startY,
        float toX, float toZ, int endY,
        const WorldGenerator& worldGen
    ) const;
    
    // Compute the exit point on settlement edge towards another settlement
    static void computeEdgePoint(
        const SettlementNode& settlement,
        float targetX, float targetZ,
        float& edgeX, float& edgeZ
    );
    
    // Pack settlement position into a unique int64
    static int64_t packSettlementPos(float x, float z) {
        int ix = static_cast<int>(std::round(x));
        int iz = static_cast<int>(std::round(z));
        return (static_cast<int64_t>(ix) << 32) | (static_cast<int64_t>(iz) & 0xFFFFFFFF);
    }
};
