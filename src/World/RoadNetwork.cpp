#include "RoadNetwork.h"
#include "WorldGenerator.h"
#include "Chunk.h"
#include "Structure.h"
#include "../Util/Config.h"
#include <algorithm>
#include <queue>
#include <cmath>
#include <numeric>

// ============================================================================
// Settlement Discovery
// ============================================================================
// Must replicate the exact same jittered-grid logic as getBiome() to find
// settlement centers deterministically from any world position.

std::vector<SettlementNode> RoadNetwork::findSettlementsNear(
    float worldX, float worldZ, float radius,
    const WorldGenerator& worldGen
) const {
    std::vector<SettlementNode> settlements;
    unsigned int seed = worldGen.getSeed();
    
    // --- Helper lambdas matching getBiome() exactly ---
    auto settlementJitter = [&](float gridCX, float gridCZ, float jitterAmount, float gridSize) -> std::pair<float, float> {
        unsigned int hx = static_cast<unsigned int>(static_cast<int>(std::floor(gridCX / gridSize))) * 374761393u;
        unsigned int hz = static_cast<unsigned int>(static_cast<int>(std::floor(gridCZ / gridSize))) * 668265263u;
        unsigned int h1 = (seed ^ hx ^ hz) * 2654435761u;
        unsigned int h2 = (seed ^ hz ^ (hx * 2246822519u)) * 3266489917u;
        float offsetX = ((h1 & 0xFFFF) / 32768.0f - 1.0f) * jitterAmount;
        float offsetZ = ((h2 & 0xFFFF) / 32768.0f - 1.0f) * jitterAmount;
        return {gridCX + offsetX, gridCZ + offsetZ};
    };
    auto shouldSpawn = [&](float gridCX, float gridCZ, float gridSize, float spawnChance) -> bool {
        unsigned int hx = static_cast<unsigned int>(static_cast<int>(std::floor(gridCX / gridSize))) * 374761393u;
        unsigned int hz = static_cast<unsigned int>(static_cast<int>(std::floor(gridCZ / gridSize))) * 668265263u;
        unsigned int h = (seed ^ hx ^ hz ^ 987654321u) * 2246822519u;
        float roll = (h & 0xFFFF) / 65536.0f;
        return roll < spawnChance;
    };
    
    const float CITY_GRID = 500.0f;
    const float CITY_JITTER = 120.0f;
    const float VILLAGE_GRID = 180.0f;
    const float VILLAGE_JITTER = 50.0f;
    
    // Deduplicate by packed position
    std::unordered_set<int64_t> seen;
    
    // Scan city grid cells in range
    int cityMinGX = static_cast<int>(std::floor((worldX - radius) / CITY_GRID));
    int cityMaxGX = static_cast<int>(std::floor((worldX + radius) / CITY_GRID));
    int cityMinGZ = static_cast<int>(std::floor((worldZ - radius) / CITY_GRID));
    int cityMaxGZ = static_cast<int>(std::floor((worldZ + radius) / CITY_GRID));
    
    for (int gx = cityMinGX; gx <= cityMaxGX; gx++) {
        for (int gz = cityMinGZ; gz <= cityMaxGZ; gz++) {
            float gridCX = gx * CITY_GRID + CITY_GRID / 2.0f;
            float gridCZ = gz * CITY_GRID + CITY_GRID / 2.0f;
            
            if (!shouldSpawn(gridCX, gridCZ, CITY_GRID, 0.70f)) continue;
            
            auto [cx, cz] = settlementJitter(gridCX, gridCZ, CITY_JITTER, CITY_GRID);
            
            // Mountain check (matching getBiome)
            float mtCenter = worldGen.getMountainFactor(cx, cz);
            if (mtCenter >= 0.15f) continue;
            float edgeR = 160.0f * 0.7f;
            float mtMax = std::max({
                worldGen.getMountainFactor(cx + edgeR, cz),
                worldGen.getMountainFactor(cx - edgeR, cz),
                worldGen.getMountainFactor(cx, cz + edgeR),
                worldGen.getMountainFactor(cx, cz - edgeR)
            });
            if (mtMax >= 0.30f) continue;
            
            float dx = cx - worldX, dz = cz - worldZ;
            if (dx * dx + dz * dz > radius * radius) continue;
            
            int64_t key = packSettlementPos(cx, cz);
            if (seen.count(key)) continue;
            seen.insert(key);
            
            int groundY = worldGen.getSurfaceHeight(static_cast<int>(cx), static_cast<int>(cz));
            settlements.push_back({cx, cz, true, groundY});
        }
    }
    
    // Scan village grid cells in range
    int villMinGX = static_cast<int>(std::floor((worldX - radius) / VILLAGE_GRID));
    int villMaxGX = static_cast<int>(std::floor((worldX + radius) / VILLAGE_GRID));
    int villMinGZ = static_cast<int>(std::floor((worldZ - radius) / VILLAGE_GRID));
    int villMaxGZ = static_cast<int>(std::floor((worldZ + radius) / VILLAGE_GRID));
    
    for (int gx = villMinGX; gx <= villMaxGX; gx++) {
        for (int gz = villMinGZ; gz <= villMaxGZ; gz++) {
            float gridCX = gx * VILLAGE_GRID + VILLAGE_GRID / 2.0f;
            float gridCZ = gz * VILLAGE_GRID + VILLAGE_GRID / 2.0f;
            
            if (!shouldSpawn(gridCX, gridCZ, VILLAGE_GRID, 0.75f)) continue;
            
            auto [vx, vz] = settlementJitter(gridCX, gridCZ, VILLAGE_JITTER, VILLAGE_GRID);
            
            // Mountain check (village: center only, < 0.35)
            if (worldGen.getMountainFactor(vx, vz) >= 0.35f) continue;
            
            // Village must not overlap city (matching getBiome)
            float nearCityGridX = std::floor(vx / CITY_GRID) * CITY_GRID + CITY_GRID / 2.0f;
            float nearCityGridZ = std::floor(vz / CITY_GRID) * CITY_GRID + CITY_GRID / 2.0f;
            if (shouldSpawn(nearCityGridX, nearCityGridZ, CITY_GRID, 0.70f)) {
                auto [ncx, ncz] = settlementJitter(nearCityGridX, nearCityGridZ, CITY_JITTER, CITY_GRID);
                float cdx = vx - ncx, cdz = vz - ncz;
                if (std::sqrt(cdx * cdx + cdz * cdz) < 160.0f + 10.0f) continue;
            }
            
            float dx = vx - worldX, dz = vz - worldZ;
            if (dx * dx + dz * dz > radius * radius) continue;
            
            int64_t key = packSettlementPos(vx, vz);
            if (seen.count(key)) continue;
            seen.insert(key);
            
            int groundY = worldGen.getSurfaceHeight(static_cast<int>(vx), static_cast<int>(vz));
            settlements.push_back({vx, vz, false, groundY});
        }
    }
    
    return settlements;
}

// ============================================================================
// Road Graph (MST-like connectivity)
// ============================================================================
// Connect settlements using a modified MST: each node gets up to MAX_CONNECTIONS
// edges, preferring shorter distances. Cities always connect to nearest city too.

std::vector<std::pair<int,int>> RoadNetwork::buildRoadGraph(
    const std::vector<SettlementNode>& settlements
) const {
    int n = static_cast<int>(settlements.size());
    if (n < 2) return {};
    
    // Compute all pairwise distances
    struct Edge {
        int a, b;
        float dist;
    };
    std::vector<Edge> edges;
    edges.reserve(n * (n - 1) / 2);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            float dx = settlements[i].x - settlements[j].x;
            float dz = settlements[i].z - settlements[j].z;
            float d = std::sqrt(dx * dx + dz * dz);
            // Only connect settlements within reasonable distance
            // Max road length: ~500 blocks (don't connect very distant ones)
            if (d < 500.0f) {
                edges.push_back({i, j, d});
            }
        }
    }
    
    // Sort edges by distance (shortest first)
    std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
        return a.dist < b.dist;
    });
    
    // Kruskal-like MST with degree limit
    std::vector<int> degree(n, 0);
    std::vector<int> parent(n);
    std::iota(parent.begin(), parent.end(), 0);
    
    // Union-Find
    std::function<int(int)> find = [&](int x) -> int {
        return parent[x] == x ? x : parent[x] = find(parent[x]);
    };
    
    std::vector<std::pair<int,int>> result;
    
    // First pass: build MST (connect all components, respecting degree limit)
    for (auto& e : edges) {
        if (degree[e.a] >= MAX_CONNECTIONS || degree[e.b] >= MAX_CONNECTIONS) continue;
        int fa = find(e.a), fb = find(e.b);
        if (fa == fb) continue; // Already connected
        parent[fa] = fb;
        result.push_back({e.a, e.b});
        degree[e.a]++;
        degree[e.b]++;
    }
    
    // Second pass: add extra edges for better connectivity if degree allows
    // Especially connect cities to their nearest other city
    for (auto& e : edges) {
        if (degree[e.a] >= MAX_CONNECTIONS || degree[e.b] >= MAX_CONNECTIONS) continue;
        // Check if already connected
        bool alreadyConnected = false;
        for (auto& r : result) {
            if ((r.first == e.a && r.second == e.b) || (r.first == e.b && r.second == e.a)) {
                alreadyConnected = true;
                break;
            }
        }
        if (alreadyConnected) continue;
        
        // Add if both are cities, or if distance is very short
        bool bothCities = settlements[e.a].isCity && settlements[e.b].isCity;
        if (bothCities || e.dist < 200.0f) {
            result.push_back({e.a, e.b});
            degree[e.a]++;
            degree[e.b]++;
        }
    }
    
    return result;
}

// ============================================================================
// Edge Point Computation
// ============================================================================
// Find the point on the settlement boundary closest to targetX/Z.
// Aligns to cardinal direction (N/S/E/W) so roads connect cleanly to the
// internal grid roads.

void RoadNetwork::computeEdgePoint(
    const SettlementNode& settlement,
    float targetX, float targetZ,
    float& edgeX, float& edgeZ
) {
    // Use a slightly reduced radius so the A* path extends ~10 blocks
    // INTO the settlement, overlapping with internal roads for clean connection
    float radius = (settlement.isCity ? CITY_RADIUS : VILLAGE_RADIUS) - 10.0f;
    
    float dx = targetX - settlement.x;
    float dz = targetZ - settlement.z;
    
    // Pick primary cardinal direction (whichever axis has larger delta)
    if (std::abs(dx) >= std::abs(dz)) {
        // Exit on east or west edge
        edgeX = settlement.x + (dx > 0 ? radius : -radius);
        edgeZ = settlement.z; // Centered on main E-W road
        
        // For cities, snap Z to nearest road grid line so it aligns with
        // the internal modulo-24 grid
        if (settlement.isCity) {
            int relZ = static_cast<int>(std::round(edgeZ - settlement.z));
            int roadSpacing = 24;
            int mod = ((relZ % roadSpacing) + roadSpacing) % roadSpacing;
            // Snap to nearest road center (road occupies mod 0,1,2)
            if (mod > roadSpacing / 2) mod -= roadSpacing;
            if (mod >= 0 && mod < 3) {
                relZ -= mod; relZ += 1; // center of 3-wide road
            } else {
                int nearest = static_cast<int>(std::round(static_cast<float>(relZ) / roadSpacing)) * roadSpacing + 1;
                relZ = nearest;
            }
            edgeZ = settlement.z + relZ;
        }
    } else {
        // Exit on north or south edge
        edgeX = settlement.x; // Centered on main N-S road
        edgeZ = settlement.z + (dz > 0 ? radius : -radius);
        
        // For cities, snap X to nearest road grid line
        if (settlement.isCity) {
            int relX = static_cast<int>(std::round(edgeX - settlement.x));
            int roadSpacing = 24;
            int mod = ((relX % roadSpacing) + roadSpacing) % roadSpacing;
            if (mod > roadSpacing / 2) mod -= roadSpacing;
            if (mod >= 0 && mod < 3) {
                relX -= mod; relX += 1;
            } else {
                int nearest = static_cast<int>(std::round(static_cast<float>(relX) / roadSpacing)) * roadSpacing + 1;
                relX = nearest;
            }
            edgeX = settlement.x + relX;
        }
    }
}

// ============================================================================
// A* Pathfinding (coarse grid)
// ============================================================================
// Finds optimal road path between settlement edge points.
// startY/endY come from the settlement's unified ground level.

std::vector<RoadWaypoint> RoadNetwork::findRoadPath(
    float fromX, float fromZ, int startY,
    float toX, float toZ, int endY,
    const WorldGenerator& worldGen
) const {
    struct Node {
        int x, z;
        float g, f;       // g = cost from start, f = g + heuristic
        int parentX, parentZ;
    };
    
    struct NodeKey {
        int x, z;
        bool operator==(const NodeKey& o) const { return x == o.x && z == o.z; }
    };
    struct NodeKeyHash {
        size_t operator()(const NodeKey& k) const {
            return std::hash<int>()(k.x) ^ (std::hash<int>()(k.z) << 16);
        }
    };
    
    // Snap to ROAD_STEP grid
    int startX = static_cast<int>(std::round(fromX / ROAD_STEP)) * ROAD_STEP;
    int startZ = static_cast<int>(std::round(fromZ / ROAD_STEP)) * ROAD_STEP;
    int endX = static_cast<int>(std::round(toX / ROAD_STEP)) * ROAD_STEP;
    int endZ = static_cast<int>(std::round(toZ / ROAD_STEP)) * ROAD_STEP;
    
    if (startX == endX && startZ == endZ) return {};
    
    auto heuristic = [&](int x, int z) -> float {
        float dx = static_cast<float>(x - endX);
        float dz = static_cast<float>(z - endZ);
        return std::sqrt(dx * dx + dz * dz);
    };
    
    auto cmp = [](const Node& a, const Node& b) { return a.f > b.f; };
    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> open(cmp);
    std::unordered_map<NodeKey, Node, NodeKeyHash> closed;
    
    open.push({startX, startZ, 0.0f, heuristic(startX, startZ), startX, startZ});
    
    // 8-directional movement
    constexpr int dirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    
    int maxIterations = 8000; // Safety limit
    int iterations = 0;
    
    bool found = false;
    
    while (!open.empty() && iterations < maxIterations) {
        iterations++;
        Node current = open.top();
        open.pop();
        
        NodeKey ck = {current.x, current.z};
        if (closed.count(ck)) continue;
        closed[ck] = current;
        
        // Reached goal?
        if (current.x == endX && current.z == endZ) {
            found = true;
            break;
        }
        
        float currentHeight = worldGen.getHeight(static_cast<float>(current.x), static_cast<float>(current.z));
        
        for (auto& dir : dirs) {
            int nx = current.x + dir[0] * ROAD_STEP;
            int nz = current.z + dir[1] * ROAD_STEP;
            
            NodeKey nk = {nx, nz};
            if (closed.count(nk)) continue;
            
            float nh = worldGen.getHeight(static_cast<float>(nx), static_cast<float>(nz));
            float nRiver = worldGen.getRiverMask(static_cast<float>(nx), static_cast<float>(nz));
            float nMountain = worldGen.getMountainFactor(static_cast<float>(nx), static_cast<float>(nz));
            
            // Base movement cost
            float stepDist = (dir[0] != 0 && dir[1] != 0) ? ROAD_STEP * 1.414f : static_cast<float>(ROAD_STEP);
            
            // Height change penalty (moderate — roads flatten via profile,
            // but prefer gentler terrain)
            float heightDiff = std::abs(nh - currentHeight);
            float heightCost = heightDiff * 0.8f;
            
            // Water/river crossing penalty (bridge cost)
            float waterCost = 0.0f;
            if (nRiver > 0.25f) waterCost = 20.0f; // Bridge is possible
            
            // Mountain penalty — REDUCED so roads tunnel through hills
            // rather than always routing around them
            float mountainCost = 0.0f;
            if (nMountain > 0.5f) mountainCost = 12.0f;
            else if (nMountain > 0.3f) mountainCost = 4.0f;
            
            // Ocean = impassable
            if (nh < static_cast<float>(SEA_LEVEL) - 5.0f && nRiver < 0.25f) continue;
            
            float totalCost = stepDist + heightCost + waterCost + mountainCost;
            float newG = current.g + totalCost;
            
            open.push({nx, nz, newG, newG + heuristic(nx, nz), current.x, current.z});
        }
    }
    
    if (!found) {
        // Fallback: straight line path if A* fails (too far or blocked)
        std::vector<RoadWaypoint> fallback;
        float dx = toX - fromX;
        float dz = toZ - fromZ;
        float dist = std::sqrt(dx * dx + dz * dz);
        int steps = static_cast<int>(dist / ROAD_STEP);
        if (steps < 1) steps = 1;
        
        for (int i = 0; i <= steps; i++) {
            float t = static_cast<float>(i) / static_cast<float>(steps);
            int wx = static_cast<int>(std::round(fromX + dx * t));
            int wz = static_cast<int>(std::round(fromZ + dz * t));
            wx = (wx / ROAD_STEP) * ROAD_STEP;
            wz = (wz / ROAD_STEP) * ROAD_STEP;
            int roadY = static_cast<int>(std::round(
                static_cast<float>(startY) + (static_cast<float>(endY) - static_cast<float>(startY)) * t));
            int terrainY = worldGen.getSurfaceHeight(wx, wz);
            float rm = worldGen.getRiverMask(static_cast<float>(wx), static_cast<float>(wz));
            bool bridge = (rm > 0.25f) || (terrainY < roadY - 2);
            bool tunnel = (terrainY > roadY + 2);
            if (fallback.empty() || fallback.back().x != wx || fallback.back().z != wz) {
                fallback.push_back({wx, wz, roadY, bridge, tunnel});
            }
        }
        return fallback;
    }
    
    // Reconstruct path
    std::vector<RoadWaypoint> path;
    NodeKey cur = {endX, endZ};
    while (!(cur.x == startX && cur.z == startZ)) {
        auto it = closed.find(cur);
        if (it == closed.end()) break;
        
        float h = worldGen.getHeight(static_cast<float>(cur.x), static_cast<float>(cur.z));
        float rm = worldGen.getRiverMask(static_cast<float>(cur.x), static_cast<float>(cur.z));
        float mf = worldGen.getMountainFactor(static_cast<float>(cur.x), static_cast<float>(cur.z));
        
        path.push_back({cur.x, cur.z, static_cast<int>(h), rm > 0.25f, mf > 0.5f});
        cur = {it->second.parentX, it->second.parentZ};
    }
    // Add start
    path.push_back({startX, startZ, startY, false, false});
    
    std::reverse(path.begin(), path.end());
    
    // Force first and last waypoints to settlement heights
    if (!path.empty()) {
        path.front().y = startY;
        path.back().y = endY;
    }
    
    // ====================================================================
    // Phase 2: Compute a FLAT road height profile
    // Linearly interpolate between settlement base heights (startY → endY)
    // with a max slope constraint. Tag bridge/tunnel per waypoint.
    // ====================================================================
    if (path.size() >= 2) {
        float startH = static_cast<float>(startY);
        float endH   = static_cast<float>(endY);
        
        // Cumulative horizontal distance along the path
        std::vector<float> cumDist(path.size(), 0.0f);
        for (size_t i = 1; i < path.size(); i++) {
            float ddx = static_cast<float>(path[i].x - path[i-1].x);
            float ddz = static_cast<float>(path[i].z - path[i-1].z);
            cumDist[i] = cumDist[i-1] + std::sqrt(ddx*ddx + ddz*ddz);
        }
        float totalDist = cumDist.back();
        
        // Linear interpolation from start to end height
        std::vector<float> idealY(path.size());
        for (size_t i = 0; i < path.size(); i++) {
            float t = (totalDist > 0.0f) ? cumDist[i] / totalDist : 0.0f;
            idealY[i] = startH + (endH - startH) * t;
        }
        
        // Max slope constraint: 1 block rise per 8 blocks horizontal
        constexpr float MAX_GRADE = 1.0f / 8.0f;
        
        // Forward pass — clamp slope
        for (size_t i = 1; i < path.size(); i++) {
            float segDist = cumDist[i] - cumDist[i-1];
            float maxChange = std::max(segDist * MAX_GRADE, 0.5f);
            idealY[i] = std::max(idealY[i], idealY[i-1] - maxChange);
            idealY[i] = std::min(idealY[i], idealY[i-1] + maxChange);
        }
        // Backward pass — clamp slope
        for (int i = static_cast<int>(path.size()) - 2; i >= 0; i--) {
            float segDist = cumDist[i+1] - cumDist[i];
            float maxChange = std::max(segDist * MAX_GRADE, 0.5f);
            idealY[i] = std::max(idealY[i], idealY[i+1] - maxChange);
            idealY[i] = std::min(idealY[i], idealY[i+1] + maxChange);
        }
        
        // Apply flat heights and determine bridge/tunnel from terrain comparison
        for (size_t i = 0; i < path.size(); i++) {
            int terrainY = worldGen.getSurfaceHeight(path[i].x, path[i].z);
            path[i].y = static_cast<int>(std::round(idealY[i]));
            
            float rm = worldGen.getRiverMask(
                static_cast<float>(path[i].x), static_cast<float>(path[i].z));
            
            // Tunnel when terrain is significantly above road level
            path[i].isTunnel = (terrainY > path[i].y + 2);
            // Bridge when over river OR road is above terrain (valley)
            path[i].isBridge = (rm > 0.25f) || (terrainY < path[i].y - 2);
        }
    }
    
    return path;
}

// ============================================================================
// Road Placement in Chunks
// ============================================================================
// Called per chunk during generation. Discovers nearby settlements, builds graph,
// computes paths (cached), and places road blocks.

void RoadNetwork::placeRoadsInChunk(
    int chunkBaseX, int chunkBaseZ, int chunkBaseY,
    const WorldGenerator& worldGen,
    std::shared_ptr<Chunk> chunk
) {
    // Center of chunk for settlement search
    float chunkCenterX = chunkBaseX + CHUNK_SIZE / 2.0f;
    float chunkCenterZ = chunkBaseZ + CHUNK_SIZE / 2.0f;
    
    // Chunk bounds
    int cMinX = chunkBaseX;
    int cMaxX = chunkBaseX + CHUNK_SIZE - 1;
    int cMinZ = chunkBaseZ;
    int cMaxZ = chunkBaseZ + CHUNK_SIZE - 1;
    
    // Find settlements in a wide area (road segments can be long)
    auto settlements = findSettlementsNear(chunkCenterX, chunkCenterZ, SEARCH_RADIUS, worldGen);
    if (settlements.size() < 2) return;
    
    // Build road graph
    auto connections = buildRoadGraph(settlements);
    if (connections.empty()) return;
    
    // For each road segment, compute path and check if it crosses this chunk
    for (auto& [fromIdx, toIdx] : connections) {
        auto& from = settlements[fromIdx];
        auto& to = settlements[toIdx];
        
        // Quick AABB rejection: does the bounding box of from->to intersect chunk?
        float roadMinX = std::min(from.x, to.x) - 20.0f; // margin for curving
        float roadMaxX = std::max(from.x, to.x) + 20.0f;
        float roadMinZ = std::min(from.z, to.z) - 20.0f;
        float roadMaxZ = std::max(from.z, to.z) + 20.0f;
        
        if (roadMaxX < cMinX || roadMinX > cMaxX) continue;
        if (roadMaxZ < cMinZ || roadMinZ > cMaxZ) continue;
        
        // Get or compute road path
        int64_t keyA = packSettlementPos(from.x, from.z);
        int64_t keyB = packSettlementPos(to.x, to.z);
        if (keyA > keyB) std::swap(keyA, keyB); // Normalize order
        PairKey pkey = {keyA, keyB};
        
        std::vector<RoadWaypoint>* pathPtr = nullptr;
        
        {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            auto it = m_roadCache.find(pkey);
            if (it != m_roadCache.end()) {
                pathPtr = &it->second.path;
            }
        }
        
        if (!pathPtr) {
            // Compute edge positions for this road segment
            float edgeFromX, edgeFromZ, edgeToX, edgeToZ;
            computeEdgePoint(from, to.x, to.z, edgeFromX, edgeFromZ);
            computeEdgePoint(to, from.x, from.z, edgeToX, edgeToZ);
            
            // Use surface height AT THE EDGE points (not center) so roads
            // match the settlement internal road heights at the connection
            int edgeFromY = worldGen.getSurfaceHeight(
                static_cast<int>(std::round(edgeFromX)),
                static_cast<int>(std::round(edgeFromZ)));
            int edgeToY = worldGen.getSurfaceHeight(
                static_cast<int>(std::round(edgeToX)),
                static_cast<int>(std::round(edgeToZ)));
            
            // For cities, internal roads are at a unified cityY (center height)
            // so snap to that instead
            if (from.isCity) edgeFromY = from.groundY;
            if (to.isCity) edgeToY = to.groundY;
            
            // Compute path between edges
            auto path = findRoadPath(edgeFromX, edgeFromZ, edgeFromY,
                                     edgeToX, edgeToZ, edgeToY, worldGen);
            if (path.empty()) continue;
            
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            auto& seg = m_roadCache[pkey];
            seg.fromIdx = fromIdx;
            seg.toIdx = toIdx;
            seg.path = std::move(path);
            pathPtr = &m_roadCache[pkey].path;
        }
        
        if (!pathPtr || pathPtr->empty()) continue;
        
        // Determine road width based on whether cities are involved
        bool isCityRoad = from.isCity || to.isCity;
        int halfWidth = isCityRoad ? CITY_ROAD_HALF_WIDTH : ROAD_HALF_WIDTH;
        
        // Now place road blocks for path segments that cross this chunk
        for (size_t i = 0; i + 1 < pathPtr->size(); i++) {
            auto& wp0 = (*pathPtr)[i];
            auto& wp1 = (*pathPtr)[i + 1];
            
            // Segment AABB check against chunk
            int segMinX = std::min(wp0.x, wp1.x) - halfWidth - 1;
            int segMaxX = std::max(wp0.x, wp1.x) + halfWidth + 1;
            int segMinZ = std::min(wp0.z, wp1.z) - halfWidth - 1;
            int segMaxZ = std::max(wp0.z, wp1.z) + halfWidth + 1;
            
            if (segMaxX < cMinX || segMinX > cMaxX) continue;
            if (segMaxZ < cMinZ || segMinZ > cMaxZ) continue;
            
            // Interpolate between waypoints and place road surface
            float dx = static_cast<float>(wp1.x - wp0.x);
            float dz = static_cast<float>(wp1.z - wp0.z);
            float segLen = std::sqrt(dx * dx + dz * dz);
            if (segLen < 0.5f) continue;
            
            // Normal vector (perpendicular to road direction)
            float nx = -dz / segLen;
            float nz = dx / segLen;
            
            int steps = static_cast<int>(segLen) + 1;
            for (int s = 0; s <= steps; s++) {
                float t = static_cast<float>(s) / static_cast<float>(steps);
                float centerXf = wp0.x + dx * t;
                float centerZf = wp0.z + dz * t;
                int roadY = static_cast<int>(std::round(wp0.y + (wp1.y - wp0.y) * t));
                
                // Place road surface across width
                for (int w = -halfWidth; w <= halfWidth; w++) {
                    int worldBX = static_cast<int>(std::round(centerXf + nx * w));
                    int worldBZ = static_cast<int>(std::round(centerZf + nz * w));
                    
                    int localBX = worldBX - chunkBaseX;
                    int localBZ = worldBZ - chunkBaseZ;
                    
                    if (localBX < 0 || localBX >= CHUNK_SIZE) continue;
                    if (localBZ < 0 || localBZ >= CHUNK_SIZE) continue;
                    
                    // Don't overwrite existing road/path blocks from settlement
                    // internal roads (which are placed first). This naturally
                    // connects inter-settlement roads to the settlement edge.
                    int localRoadY = roadY - chunkBaseY;
                    if (localRoadY < 0 || localRoadY >= CHUNK_HEIGHT) continue;
                    
                    Block existingBlock = chunk->getBlock(localBX, localRoadY, localBZ);
                    BlockType existingType = existingBlock.getType();
                    if (existingType == BlockType::COBBLESTONE ||
                        existingType == BlockType::STONE_BRICKS ||
                        existingType == BlockType::GRAVEL ||
                        existingType == BlockType::STONE ||
                        existingType == BlockType::DIRT) {
                        continue; // Already a road surface — don't overwrite
                    }
                    
                    // Also check ±1 Y in case of slight height mismatch at edges
                    bool nearbyRoad = false;
                    for (int dy = -1; dy <= 1; dy++) {
                        int checkY = localRoadY + dy;
                        if (checkY < 0 || checkY >= CHUNK_HEIGHT || dy == 0) continue;
                        Block nearBlock = chunk->getBlock(localBX, checkY, localBZ);
                        BlockType nearType = nearBlock.getType();
                        if (nearType == BlockType::COBBLESTONE ||
                            nearType == BlockType::STONE_BRICKS ||
                            nearType == BlockType::GRAVEL ||
                            nearType == BlockType::STONE) {
                            nearbyRoad = true;
                            break;
                        }
                    }
                    if (nearbyRoad) continue;
                    
                    // Per-block terrain comparison to decide bridge/tunnel/normal
                    int naturalY = worldGen.getSurfaceHeight(worldBX, worldBZ);
                    float rm = worldGen.getRiverMask(static_cast<float>(worldBX), static_cast<float>(worldBZ));
                    int heightDiff = naturalY - roadY;  // positive = terrain above road
                    
                    bool doBridge = (rm > 0.25f) || (heightDiff < -2);
                    bool doTunnel = (heightDiff > 2);
                    bool isEdge = (std::abs(w) == halfWidth);
                    
                    // === BRIDGE (road above terrain / over river) ===
                    if (doBridge) {
                        // Bridge deck
                        chunk->setBlock(localBX, localRoadY, localBZ, Block(
                            isCityRoad ? BlockType::STONE_BRICKS : BlockType::COBBLESTONE));
                        
                        // Railings on edges
                        if (isEdge) {
                            int railY = localRoadY + 1;
                            if (railY >= 0 && railY < CHUNK_HEIGHT) {
                                chunk->setBlock(localBX, railY, localBZ, Block(BlockType::COBBLESTONE));
                            }
                        }
                        
                        // Support pillars only every 4 blocks along road (intermittent)
                        bool isPillarPos = (s % 4 == 0);
                        if (isEdge && isPillarPos) {
                            for (int py = localRoadY - 1; py >= 0; py--) {
                                Block below = chunk->getBlock(localBX, py, localBZ);
                                BlockType bt = below.getType();
                                if (bt == BlockType::AIR || bt == BlockType::WATER) {
                                    chunk->setBlock(localBX, py, localBZ, Block(BlockType::STONE_BRICKS));
                                } else break;
                            }
                        }
                        
                        // Clear headroom above bridge
                        for (int cy = 1; cy <= 5; cy++) {
                            int clearY = localRoadY + cy;
                            if (isEdge && cy == 1) continue; // Railing
                            if (clearY >= 0 && clearY < CHUNK_HEIGHT) {
                                Block above = chunk->getBlock(localBX, clearY, localBZ);
                                if (above.getType() != BlockType::AIR && above.getType() != BlockType::WATER) {
                                    chunk->setBlock(localBX, clearY, localBZ, Block(BlockType::AIR));
                                }
                            }
                        }
                        continue;
                    }
                    
                    // === TUNNEL (terrain above road) ===
                    if (doTunnel) {
                        // Road surface
                        chunk->setBlock(localBX, localRoadY, localBZ, Block(
                            isCityRoad ? BlockType::STONE_BRICKS : BlockType::COBBLESTONE));
                        
                        // Carve air above road (tunnel clearance = 4 blocks)
                        for (int cy = 1; cy <= 4; cy++) {
                            int clearY = localRoadY + cy;
                            if (clearY >= 0 && clearY < CHUNK_HEIGHT) {
                                chunk->setBlock(localBX, clearY, localBZ, Block(BlockType::AIR));
                            }
                        }
                        
                        // Tunnel ceiling (reinforce with stone bricks)
                        int ceilingY = localRoadY + 5;
                        if (ceilingY >= 0 && ceilingY < CHUNK_HEIGHT) {
                            chunk->setBlock(localBX, ceilingY, localBZ, Block(BlockType::STONE_BRICKS));
                        }
                        
                        // Tunnel walls on edges
                        if (isEdge) {
                            for (int wy = 1; wy <= 4; wy++) {
                                int wallY = localRoadY + wy;
                                if (wallY >= 0 && wallY < CHUNK_HEIGHT) {
                                    chunk->setBlock(localBX, wallY, localBZ, Block(BlockType::STONE_BRICKS));
                                }
                            }
                        }
                        
                        // Foundation below road
                        int belowY = localRoadY - 1;
                        if (belowY >= 0 && belowY < CHUNK_HEIGHT) {
                            chunk->setBlock(localBX, belowY, localBZ, Block(BlockType::COBBLESTONE));
                        }
                        continue;
                    }
                    
                    // === NORMAL ROAD (terrain close to road level) ===
                    // Carve terrain down or fill up to reach road level
                    if (naturalY > roadY) {
                        for (int cy = roadY + 1; cy <= naturalY + 2; cy++) {
                            int localCY = cy - chunkBaseY;
                            if (localCY >= 0 && localCY < CHUNK_HEIGHT) {
                                chunk->setBlock(localBX, localCY, localBZ, Block(BlockType::AIR));
                            }
                        }
                    } else if (naturalY < roadY) {
                        for (int fy = naturalY + 1; fy < roadY; fy++) {
                            int localFY = fy - chunkBaseY;
                            if (localFY >= 0 && localFY < CHUNK_HEIGHT) {
                                chunk->setBlock(localBX, localFY, localBZ, Block(BlockType::DIRT));
                            }
                        }
                    }
                    
                    // Road surface material (matching vxstruct road styles)
                    BlockType roadSurface;
                    if (isCityRoad) {
                        // City road: stone_bricks edges, cobblestone fill, gravel center
                        roadSurface = isEdge ? BlockType::STONE_BRICKS : 
                                     (w == 0 ? BlockType::GRAVEL : BlockType::COBBLESTONE);
                    } else {
                        // Village road: gravel edges, dirt center (matching v_road_straight)
                        roadSurface = isEdge ? BlockType::GRAVEL : BlockType::DIRT;
                    }
                    chunk->setBlock(localBX, localRoadY, localBZ, Block(roadSurface));
                    
                    // Foundation (support below road)
                    for (int fd = 1; fd <= 3; fd++) {
                        int localFY = localRoadY - fd;
                        if (localFY >= 0 && localFY < CHUNK_HEIGHT) {
                            Block below = chunk->getBlock(localBX, localFY, localBZ);
                            BlockType bt = below.getType();
                            if (bt == BlockType::AIR || bt == BlockType::WATER) {
                                chunk->setBlock(localBX, localFY, localBZ, Block(BlockType::DIRT));
                            }
                        }
                    }
                    
                    // Clear above road for headroom
                    for (int cy = 1; cy <= 4; cy++) {
                        int clearY = localRoadY + cy;
                        if (clearY >= 0 && clearY < CHUNK_HEIGHT) {
                            Block above = chunk->getBlock(localBX, clearY, localBZ);
                            BlockType atype = above.getType();
                            if (atype != BlockType::AIR && atype != BlockType::WATER) {
                                chunk->setBlock(localBX, clearY, localBZ, Block(BlockType::AIR));
                            }
                        }
                    }
                }
            }
        }
    }
}
