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
    const float VILLAGE_GRID = 350.0f;
    const float VILLAGE_JITTER = 80.0f;
    
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
            
            if (!shouldSpawn(gridCX, gridCZ, VILLAGE_GRID, 0.45f)) continue;
            
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
            // Max road length: ~800 blocks (villages at 350 grid, cities at 500)
            if (d < 800.0f) {
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
    // Use a reduced radius so the A* path extends well INTO the settlement,
    // overlapping with internal roads for a guaranteed clean connection
    float radius = (settlement.isCity ? CITY_RADIUS : VILLAGE_RADIUS) - 20.0f;
    
    float dx = targetX - settlement.x;
    float dz = targetZ - settlement.z;
    
    // Pick primary cardinal direction (whichever axis has larger delta)
    if (std::abs(dx) >= std::abs(dz)) {
        // Exit on east or west edge
        edgeX = settlement.x + (dx > 0 ? radius : -radius);
        edgeZ = settlement.z; // Centered on main E-W road
        
        // For cities, snap Z to nearest road grid line so it aligns with
        // the internal modulo-24 grid. Roads are 7-wide (mod 0..6), center at mod 3.
        if (settlement.isCity) {
            int relZ = static_cast<int>(std::round(edgeZ - settlement.z));
            int roadSpacing = 24;
            int mod = ((relZ % roadSpacing) + roadSpacing) % roadSpacing;
            // Snap to nearest road center (road occupies mod 0..6, center = 3)
            if (mod <= 6) {
                relZ -= mod; relZ += 3; // center of 7-wide road
            } else if (mod >= roadSpacing - 6) {
                // Closer to next road's start
                relZ += (roadSpacing - mod) + 3;
            } else {
                // Between roads - snap to nearest road center
                int nearest = static_cast<int>(std::round(static_cast<float>(relZ) / roadSpacing)) * roadSpacing + 3;
                relZ = nearest;
            }
            edgeZ = settlement.z + relZ;
        }
    } else {
        // Exit on north or south edge
        edgeX = settlement.x; // Centered on main N-S road
        edgeZ = settlement.z + (dz > 0 ? radius : -radius);
        
        // For cities, snap X to nearest road grid line (7-wide, center at mod 3)
        if (settlement.isCity) {
            int relX = static_cast<int>(std::round(edgeX - settlement.x));
            int roadSpacing = 24;
            int mod = ((relX % roadSpacing) + roadSpacing) % roadSpacing;
            if (mod <= 6) {
                relX -= mod; relX += 3;
            } else if (mod >= roadSpacing - 6) {
                relX += (roadSpacing - mod) + 3;
            } else {
                int nearest = static_cast<int>(std::round(static_cast<float>(relX) / roadSpacing)) * roadSpacing + 3;
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
        // Manhattan distance — appropriate for cardinal-only movement
        return static_cast<float>(std::abs(x - endX) + std::abs(z - endZ));
    };
    
    auto cmp = [](const Node& a, const Node& b) { return a.f > b.f; };
    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> open(cmp);
    std::unordered_map<NodeKey, Node, NodeKeyHash> closed;
    
    open.push({startX, startZ, 0.0f, heuristic(startX, startZ), startX, startZ});
    
    // 4-directional movement only (cardinal) — no diagonals,
    // so roads are always axis-aligned for clean vxstruct placement
    constexpr int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    
    int maxIterations = 15000; // Safety limit (larger for longer roads)
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
            
            // Base movement cost (all steps are cardinal, same distance)
            float stepDist = static_cast<float>(ROAD_STEP);
            
            // Turn penalty — discourage direction changes to prevent staircase
            // patterns and create long straight segments with clean 90° turns
            float turnCost = 0.0f;
            {
                int fromDX = current.x - current.parentX;
                int fromDZ = current.z - current.parentZ;
                if (fromDX != 0 || fromDZ != 0) { // Not start node
                    int fromSignX = (fromDX > 0) ? 1 : (fromDX < 0) ? -1 : 0;
                    int fromSignZ = (fromDZ > 0) ? 1 : (fromDZ < 0) ? -1 : 0;
                    if (dir[0] != fromSignX || dir[1] != fromSignZ) {
                        turnCost = 40.0f;
                    }
                }
            }
            
            // Height change penalty (lower — roads follow terrain directly now,
            // no need to avoid hills since we carve through them)
            float heightDiff = std::abs(nh - currentHeight);
            float heightCost = heightDiff * 0.5f;
            
            // Water/river crossing penalty (bridge cost)
            float waterCost = 0.0f;
            if (nRiver > 0.25f) waterCost = 20.0f; // Bridge is possible
            
            // Mountain penalty — low since roads carve through terrain
            float mountainCost = 0.0f;
            if (nMountain > 0.5f) mountainCost = 6.0f;
            else if (nMountain > 0.3f) mountainCost = 2.0f;
            
            // Ocean/deep water = impassable (only rivers are crossable)
            if (nh < static_cast<float>(SEA_LEVEL) - 2.0f && nRiver < 0.25f) continue;
            
            // Shallow water penalty (discourage but don't block)
            if (nh < static_cast<float>(SEA_LEVEL) + 1.0f && nRiver < 0.25f) waterCost += 50.0f;
            
            float totalCost = stepDist + turnCost + heightCost + waterCost + mountainCost;
            float newG = current.g + totalCost;
            
            open.push({nx, nz, newG, newG + heuristic(nx, nz), current.x, current.z});
        }
    }
    
    // ====================================================================
    // Path reconstruction — from A* result or straight-line fallback.
    // Both go through Phase 2 smoothing below.
    // ====================================================================
    std::vector<RoadWaypoint> path;
    
    if (found) {
        // Normal A* path reconstruction from closed set
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
        path.push_back({startX, startZ, startY, false, false});
        std::reverse(path.begin(), path.end());
    } else {
        // Fallback: straight line path (terrain-following heights).
        // Always produces a connected path — never gives up.
        float fdx = toX - fromX;
        float fdz = toZ - fromZ;
        float fdist = std::sqrt(fdx * fdx + fdz * fdz);
        int fsteps = static_cast<int>(fdist / ROAD_STEP);
        if (fsteps < 1) fsteps = 1;
        
        for (int i = 0; i <= fsteps; i++) {
            float t = static_cast<float>(i) / static_cast<float>(fsteps);
            int wx = static_cast<int>(std::round(fromX + fdx * t));
            int wz = static_cast<int>(std::round(fromZ + fdz * t));
            wx = (wx / ROAD_STEP) * ROAD_STEP;
            wz = (wz / ROAD_STEP) * ROAD_STEP;
            int terrainY = worldGen.getSurfaceHeight(wx, wz);
            float rm = worldGen.getRiverMask(static_cast<float>(wx), static_cast<float>(wz));
            bool bridge = (rm > 0.25f);
            if (path.empty() || path.back().x != wx || path.back().z != wz) {
                path.push_back({wx, wz, terrainY, bridge, false});
            }
        }
    }
    
    if (path.empty()) return path;
    
    // Force first and last waypoints to settlement heights
    if (!path.empty()) {
        path.front().y = startY;
        path.back().y = endY;
    }
    
    // ====================================================================
    // Phase 2: SMART height profile with tunnels & bridges.
    // Road follows terrain on flat/gentle ground, but goes FLAT through
    // mountains (tunnel) and FLAT over water (bridge at land level).
    // Tunnels: road locks height when terrain rises too high above it.
    // Bridges: road locks height at last land level when crossing water.
    // ====================================================================
    if (path.size() >= 2) {
        // First pass: gather terrain heights, river mask, mountain factor
        std::vector<int> terrainHeights(path.size());
        std::vector<float> riverMasks(path.size());
        std::vector<float> mountainFactors(path.size());
        for (size_t i = 0; i < path.size(); i++) {
            terrainHeights[i] = worldGen.getSurfaceHeight(path[i].x, path[i].z);
            riverMasks[i] = worldGen.getRiverMask(
                static_cast<float>(path[i].x), static_cast<float>(path[i].z));
            mountainFactors[i] = worldGen.getMountainFactor(
                static_cast<float>(path[i].x), static_cast<float>(path[i].z));
        }
        
        // Second pass (forward): compute road heights.
        // Track a "reference height" = last known good ground level.
        // When entering a mountain or water stretch, lock road Y to reference.
        constexpr int TUNNEL_THRESHOLD = 3;  // Terrain must rise >3 above reference to trigger tunnel
        constexpr int MAX_SLOPE = 1;         // Max Y change per waypoint step for very flat roads
        
        // Initialize reference height from start
        int refHeight = startY;
        path[0].y = startY;
        path[0].isBridge = (riverMasks[0] > 0.25f);
        path[0].isTunnel = false;
        
        for (size_t i = 1; i < path.size(); i++) {
            int terrY = terrainHeights[i];
            bool isWater = (riverMasks[i] > 0.25f);
            bool isMountainous = (terrY > refHeight + TUNNEL_THRESHOLD);
            
            if (isWater) {
                // Bridge: stay at reference height (last solid land level)
                path[i].y = refHeight;
                path[i].isBridge = true;
                path[i].isTunnel = false;
            } else if (isMountainous) {
                // Tunnel: terrain is way above us, stay flat at reference
                path[i].y = refHeight;
                path[i].isBridge = false;
                path[i].isTunnel = true;
            } else {
                // Normal terrain following with gentle slope limit
                int targetY = terrY;
                int diff = targetY - refHeight;
                if (diff > MAX_SLOPE) targetY = refHeight + MAX_SLOPE;
                else if (diff < -MAX_SLOPE) targetY = refHeight - MAX_SLOPE;
                
                path[i].y = targetY;
                path[i].isBridge = false;
                path[i].isTunnel = false;
                refHeight = targetY; // Update reference to current ground
            }
        }
        
        // Third pass (backward from end): same logic to ensure the road
        // doesn't end up too high/low arriving at the destination.
        // Take the LOWER of forward and backward passes to avoid floating.
        refHeight = endY;
        for (int i = static_cast<int>(path.size()) - 2; i >= 0; i--) {
            int terrY = terrainHeights[i];
            bool isWater = (riverMasks[i] > 0.25f);
            bool isMountainous = (terrY > refHeight + TUNNEL_THRESHOLD);
            
            int backwardY;
            if (isWater || isMountainous) {
                backwardY = refHeight;
            } else {
                int targetY = terrY;
                int diff = targetY - refHeight;
                if (diff > MAX_SLOPE) targetY = refHeight + MAX_SLOPE;
                else if (diff < -MAX_SLOPE) targetY = refHeight - MAX_SLOPE;
                backwardY = targetY;
                refHeight = targetY;
            }
            
            // Take the lower of both passes — avoids road floating above terrain
            // but also avoids sinking below tunnel entry height
            if (!path[i].isBridge && !path[i].isTunnel) {
                path[i].y = std::min(path[i].y, backwardY);
            }
            // Recheck tunnel/bridge status with final height
            if (!isWater && terrainHeights[i] > path[i].y + TUNNEL_THRESHOLD) {
                path[i].isTunnel = true;
            }
        }
        
        // Force endpoints to settlement heights
        path.front().y = startY;
        path.back().y = endY;
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
            // match the settlement internal road heights at the connection.
            // Both cities and villages use unified flat road heights (groundY),
            // so always use settlement center groundY.
            int edgeFromY = from.groundY;
            int edgeToY = to.groundY;
            
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
        
        // Determine road width - ALL inter-settlement highways use 7-wide roads
        int halfWidth = CITY_ROAD_HALF_WIDTH;
        
        // Load vxstruct road/bridge for material lookup (same as city internal roads)
        auto& registry = StructureRegistry::instance();
        auto roadStraightStruct = registry.getStructure("road_straight");
        auto bridgeStraightStruct = registry.getStructure("bridge_straight");
        
        // Helper: get block type + metadata from a vxstruct at a given cross-section position
        auto getVxBlock = [](const std::shared_ptr<Structure>& structure, int crossPos, int alongPos) -> std::pair<BlockType, uint8_t> {
            if (!structure) return {BlockType::COBBLESTONE, uint8_t(0)};
            glm::ivec3 size = structure->getSize();
            int cx = std::clamp(crossPos, 0, size.x - 1);
            int az = ((alongPos % size.z) + size.z) % size.z;
            glm::ivec3 pos(cx, 0, az);
            BlockType bt = structure->getBlock(pos);
            uint8_t meta = structure->getBlockMetadata(pos);
            if (bt == BlockType::AIR) return {BlockType::COBBLESTONE, uint8_t(0)};
            return {bt, meta};
        };
        
        // Helper: check if a block type is vegetation/tree that should be cleared
        auto isVegetation = [](BlockType bt) -> bool {
            return bt == BlockType::LOG || bt == BlockType::LEAVES ||
                   bt == BlockType::OAK_LOG || bt == BlockType::SPRUCE_LOG ||
                   bt == BlockType::BIRCH_LOG || bt == BlockType::JUNGLE_LOG ||
                   bt == BlockType::OAK_LEAVES || bt == BlockType::SPRUCE_LEAVES ||
                   bt == BlockType::BIRCH_LEAVES || bt == BlockType::JUNGLE_LEAVES ||
                   bt == BlockType::ACACIA_LEAVES || bt == BlockType::DARK_OAK_LEAVES ||
                   bt == BlockType::TALL_GRASS || bt == BlockType::ROSE ||
                   bt == BlockType::WOOD || bt == BlockType::SUGAR_CANE;
        };
        
        // Helper: check if block is non-solid / clearable (not bedrock, ores, etc.)
        auto isClearable = [](BlockType bt) -> bool {
            return bt != BlockType::AIR && bt != BlockType::BEDROCK &&
                   bt != BlockType::WATER;
        };
        
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
            
            // Per-segment direction for vxstruct face rotation
            // With cardinal-only A*, each segment is purely N-S or E-W
            bool segmentIsEW = std::abs(dx) > std::abs(dz);
            
            // Normal vector (perpendicular to road direction)
            float nx = -dz / segLen;
            float nz = dx / segLen;
            
            int steps = static_cast<int>(segLen) + 1;
            for (int s = 0; s <= steps; s++) {
                float t = static_cast<float>(s) / static_cast<float>(steps);
                float centerXf = wp0.x + dx * t;
                float centerZf = wp0.z + dz * t;
                // Road Y from waypoints — already computed in Phase 2 with
                // tunnel/bridge flattening
                int roadCenterY = static_cast<int>(std::round(wp0.y + (wp1.y - wp0.y) * t));
                
                // Interpolate bridge status from waypoints
                bool wpIsBridge = (t < 0.5f) ? wp0.isBridge : wp1.isBridge;
                
                // Place road surface across width
                for (int w = -halfWidth; w <= halfWidth; w++) {
                    int worldBX = static_cast<int>(std::round(centerXf + nx * w));
                    int worldBZ = static_cast<int>(std::round(centerZf + nz * w));
                    
                    int localBX = worldBX - chunkBaseX;
                    int localBZ = worldBZ - chunkBaseZ;
                    
                    if (localBX < 0 || localBX >= CHUNK_SIZE) continue;
                    if (localBZ < 0 || localBZ >= CHUNK_SIZE) continue;
                    
                    // Get ACTUAL terrain height at this exact block position
                    int naturalY = worldGen.getSurfaceHeight(worldBX, worldBZ);
                    bool isEdge = (std::abs(w) == halfWidth);
                    
                    // ALWAYS use Phase-2 computed road height — never drop to terrain
                    // This ensures the road stays flat and connected
                    bool doBridge = wpIsBridge;
                    int roadY = roadCenterY;
                    bool doTunnel = !doBridge && (naturalY > roadY); // ANY terrain above road = carve
                    bool doFill = !doBridge && !doTunnel && (naturalY < roadY - 1); // road above terrain = fill support
                    int localRoadY = roadY - chunkBaseY;
                    if (localRoadY < 0 || localRoadY >= CHUNK_HEIGHT) continue;
                    
                    // Don't overwrite existing settlement road blocks
                    Block existingBlock = chunk->getBlock(localBX, localRoadY, localBZ);
                    BlockType existingType = existingBlock.getType();
                    if (existingType == BlockType::COBBLESTONE ||
                        existingType == BlockType::STONE_BRICKS ||
                        existingType == BlockType::GRAVEL ||
                        existingType == BlockType::STONE ||
                        existingType == BlockType::GLAZED_TERRACOTTA) {
                        continue;
                    }
                    
                    // Get vxstruct material for this position
                    // crossPos = w + halfWidth (0..6), alongPos = s (along road direction)
                    int crossPos = w + halfWidth;
                    std::pair<BlockType, uint8_t> matPair;
                    if (doBridge) {
                        matPair = getVxBlock(bridgeStraightStruct, crossPos, s);
                    } else {
                        matPair = getVxBlock(roadStraightStruct, crossPos, s);
                    }
                    // Apply E-W face rotation (+1) if this segment goes east-west
                    if (segmentIsEW) {
                        matPair.second = (matPair.second + 1) & 0x03;
                    }
                    
                    // === TUNNEL (terrain above road — ALWAYS carve) ===
                    if (doTunnel) {
                        // Place road surface
                        chunk->setBlock(localBX, localRoadY, localBZ, Block(matPair.first, matPair.second));
                        
                        // Solid floor below road (2 blocks)
                        for (int fd = 1; fd <= 2; fd++) {
                            int localFY = localRoadY - fd;
                            if (localFY >= 0 && localFY < CHUNK_HEIGHT) {
                                Block below = chunk->getBlock(localBX, localFY, localBZ);
                                BlockType bt = below.getType();
                                if (bt == BlockType::AIR || bt == BlockType::WATER) {
                                    chunk->setBlock(localBX, localFY, localBZ, Block(BlockType::STONE));
                                }
                            }
                        }
                        
                        // Carve headroom (4 blocks above road)
                        int terrainAbove = naturalY - roadY; // how deep we are
                        bool deepTunnel = (terrainAbove > 5); // true tunnel inside mountain
                        for (int cy = 1; cy <= 4; cy++) {
                            int clearY = localRoadY + cy;
                            if (clearY >= 0 && clearY < CHUNK_HEIGHT) {
                                if (isEdge && deepTunnel) {
                                    // Deep tunnel: stone walls on edges
                                    chunk->setBlock(localBX, clearY, localBZ, Block(BlockType::STONE_BRICKS));
                                } else {
                                    chunk->setBlock(localBX, clearY, localBZ, Block(BlockType::AIR));
                                }
                            }
                        }
                        
                        // Ceiling for deep tunnels
                        if (deepTunnel) {
                            int ceilY = localRoadY + 5;
                            if (ceilY >= 0 && ceilY < CHUNK_HEIGHT) {
                                chunk->setBlock(localBX, ceilY, localBZ, Block(BlockType::STONE_BRICKS));
                            }
                        }
                        
                        // ALWAYS clear above the tunnel/cut up to 15 blocks
                        // Don't break on AIR — trees have gaps between trunk and canopy
                        int clearStart = deepTunnel ? 6 : 5;
                        for (int cy = clearStart; cy <= 15; cy++) {
                            int clearY = localRoadY + cy;
                            if (clearY >= 0 && clearY < CHUNK_HEIGHT) {
                                Block above = chunk->getBlock(localBX, clearY, localBZ);
                                BlockType atype = above.getType();
                                if (isVegetation(atype) || atype == BlockType::DIRT || atype == BlockType::GRASS || atype == BlockType::SNOW) {
                                    chunk->setBlock(localBX, clearY, localBZ, Block(BlockType::AIR));
                                }
                            }
                        }
                        
                        continue;
                    }
                    
                    // === BRIDGE (over water — stays at land level) ===
                    if (doBridge) {
                        chunk->setBlock(localBX, localRoadY, localBZ, Block(matPair.first, matPair.second));
                        
                        // Guardrails on edges
                        if (isEdge) {
                            int guardrailY = localRoadY + 1;
                            if (guardrailY >= 0 && guardrailY < CHUNK_HEIGHT) {
                                chunk->setBlock(localBX, guardrailY, localBZ, Block(BlockType::STONE_BRICKS));
                            }
                        }
                        
                        // Support below bridge: fill down to terrain or use pillars
                        if (naturalY >= roadY - 6) {
                            // Near terrain — solid fill
                            for (int fy = localRoadY - 1; fy >= 0; fy--) {
                                Block below = chunk->getBlock(localBX, fy, localBZ);
                                BlockType bt = below.getType();
                                if (bt == BlockType::AIR || bt == BlockType::WATER) {
                                    chunk->setBlock(localBX, fy, localBZ, Block(BlockType::COBBLESTONE));
                                } else break;
                            }
                        } else {
                            // Deep water — edge pillars every 4 blocks
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
                        }
                        
                        // Clear headroom above bridge
                        for (int cy = (isEdge ? 2 : 1); cy <= 6; cy++) {
                            int clearY = localRoadY + cy;
                            if (clearY >= 0 && clearY < CHUNK_HEIGHT) {
                                Block above = chunk->getBlock(localBX, clearY, localBZ);
                                if (above.getType() != BlockType::AIR && above.getType() != BlockType::WATER) {
                                    chunk->setBlock(localBX, clearY, localBZ, Block(BlockType::AIR));
                                }
                            }
                        }
                        continue;
                    }
                    
                    // === FILL (road above terrain — need support) ===
                    if (doFill) {
                        chunk->setBlock(localBX, localRoadY, localBZ, Block(matPair.first, matPair.second));
                        
                        // Fill from road down to terrain with solid blocks
                        for (int fy = localRoadY - 1; fy >= 0; fy--) {
                            int worldFY = fy + chunkBaseY;
                            if (worldFY < naturalY - 2) break; // deep enough
                            Block below = chunk->getBlock(localBX, fy, localBZ);
                            BlockType bt = below.getType();
                            if (bt == BlockType::AIR || isVegetation(bt)) {
                                chunk->setBlock(localBX, fy, localBZ, Block(BlockType::COBBLESTONE));
                            } else if (bt != BlockType::WATER) {
                                break; // hit solid ground, done
                            } else {
                                chunk->setBlock(localBX, fy, localBZ, Block(BlockType::COBBLESTONE));
                            }
                        }
                        
                        // Guardrails on edges when road is elevated (>2 above terrain)
                        if (isEdge && (roadY - naturalY) > 2) {
                            int guardrailY = localRoadY + 1;
                            if (guardrailY >= 0 && guardrailY < CHUNK_HEIGHT) {
                                chunk->setBlock(localBX, guardrailY, localBZ, Block(BlockType::STONE_BRICKS));
                            }
                        }
                        
                        // Clear above for headroom + trees
                        for (int cy = (isEdge && (roadY - naturalY) > 2 ? 2 : 1); cy <= 15; cy++) {
                            int clearY = localRoadY + cy;
                            if (clearY >= 0 && clearY < CHUNK_HEIGHT) {
                                Block above = chunk->getBlock(localBX, clearY, localBZ);
                                BlockType atype = above.getType();
                                if (cy <= 4) {
                                    if (isClearable(atype)) {
                                        chunk->setBlock(localBX, clearY, localBZ, Block(BlockType::AIR));
                                    }
                                } else if (isVegetation(atype) || atype == BlockType::DIRT || atype == BlockType::GRASS || atype == BlockType::SNOW) {
                                    chunk->setBlock(localBX, clearY, localBZ, Block(BlockType::AIR));
                                }
                            }
                        }
                        continue;
                    }
                    
                    // === NORMAL ROAD — terrain at or near road level ===
                    chunk->setBlock(localBX, localRoadY, localBZ, Block(matPair.first, matPair.second));
                    
                    // Ensure solid ground below road (replace air/water with dirt)
                    for (int fd = 1; fd <= 2; fd++) {
                        int localFY = localRoadY - fd;
                        if (localFY >= 0 && localFY < CHUNK_HEIGHT) {
                            Block below = chunk->getBlock(localBX, localFY, localBZ);
                            BlockType bt = below.getType();
                            if (bt == BlockType::AIR || bt == BlockType::WATER) {
                                chunk->setBlock(localBX, localFY, localBZ, Block(BlockType::DIRT));
                            }
                        }
                    }
                    
                    // Clear above road for headroom — force carve terrain + remove ALL trees
                    // Don't break on AIR — trees have gaps between trunk and canopy
                    for (int cy = 1; cy <= 15; cy++) {
                        int clearY = localRoadY + cy;
                        if (clearY >= 0 && clearY < CHUNK_HEIGHT) {
                            Block above = chunk->getBlock(localBX, clearY, localBZ);
                            BlockType atype = above.getType();
                            if (cy <= 4) {
                                // First 4 blocks: always clear everything solid
                                if (isClearable(atype)) {
                                    chunk->setBlock(localBX, clearY, localBZ, Block(BlockType::AIR));
                                }
                            } else {
                                // Above 4: remove vegetation, dirt, grass, snow (no break on AIR!)
                                if (isVegetation(atype) || atype == BlockType::DIRT || atype == BlockType::GRASS || atype == BlockType::SNOW) {
                                    chunk->setBlock(localBX, clearY, localBZ, Block(BlockType::AIR));
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
