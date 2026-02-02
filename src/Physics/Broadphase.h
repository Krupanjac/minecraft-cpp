#pragma once

#include "PhysicsTypes.h"
#include "RigidBody.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace Physics {

// ============================================================================
// Collision Pair - Represents a potential collision between two bodies
// ============================================================================
struct CollisionPair {
    RigidBody* bodyA;
    RigidBody* bodyB;
    
    bool operator==(const CollisionPair& other) const {
        return (bodyA == other.bodyA && bodyB == other.bodyB) ||
               (bodyA == other.bodyB && bodyB == other.bodyA);
    }
};

struct CollisionPairHash {
    size_t operator()(const CollisionPair& pair) const {
        // Order-independent hash
        auto h1 = std::hash<void*>{}(pair.bodyA);
        auto h2 = std::hash<void*>{}(pair.bodyB);
        return h1 ^ h2;
    }
};

// ============================================================================
// Spatial Hash Grid - Fast broadphase for dynamic worlds
// ============================================================================
class SpatialHashGrid {
public:
    explicit SpatialHashGrid(float cellSize = 4.0f);
    
    void clear();
    void insert(RigidBody* body);
    void remove(RigidBody* body);
    void update(RigidBody* body);
    
    // Query all bodies that might collide with the given AABB
    void query(const AABB& aabb, std::vector<RigidBody*>& results) const;
    
    // Get all potential collision pairs
    void getPotentialPairs(std::vector<CollisionPair>& pairs) const;
    
    void setCellSize(float size) { cellSize = size; invCellSize = 1.0f / size; }
    float getCellSize() const { return cellSize; }
    
private:
    using CellKey = std::tuple<int, int, int>;
    
    struct CellKeyHash {
        size_t operator()(const CellKey& key) const {
            auto h1 = std::hash<int>{}(std::get<0>(key));
            auto h2 = std::hash<int>{}(std::get<1>(key));
            auto h3 = std::hash<int>{}(std::get<2>(key));
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
    
    float cellSize;
    float invCellSize;
    
    std::unordered_map<CellKey, std::vector<RigidBody*>, CellKeyHash> cells;
    std::unordered_map<RigidBody*, std::vector<CellKey>> bodyToCells;
    
    void getCellsForAABB(const AABB& aabb, std::vector<CellKey>& cellKeys) const;
    CellKey worldToCell(const glm::vec3& pos) const;
};

// ============================================================================
// AABB Tree (BVH) - Efficient for large numbers of static/slow objects
// ============================================================================
class AABBTree {
public:
    static constexpr int NULL_NODE = -1;
    
    AABBTree();
    ~AABBTree() = default;
    
    // Insert a body and return its node index
    int insert(RigidBody* body);
    
    // Remove a body from the tree
    void remove(int nodeIndex);
    
    // Update a body's AABB (after movement)
    void update(int nodeIndex, const AABB& newAABB);
    
    // Query all bodies whose AABBs intersect with the given AABB
    void query(const AABB& aabb, std::vector<RigidBody*>& results) const;
    
    // Raycast through the tree
    bool raycast(const Ray& ray, RaycastHit& hit, 
                 std::function<bool(RigidBody*, RaycastHit&)> hitCallback) const;
    
    // Get all potential collision pairs
    void getPotentialPairs(std::vector<CollisionPair>& pairs) const;
    
    // Debug info
    int getNodeCount() const { return nodeCount; }
    int getHeight() const;
    
private:
    struct Node {
        AABB aabb;           // Fattened AABB for the node
        RigidBody* body;     // Only for leaf nodes
        int parent;
        int left;
        int right;
        int height;          // For balancing
        bool isLeaf() const { return left == NULL_NODE; }
    };
    
    std::vector<Node> nodes;
    int root;
    int nodeCount;
    int freeList;
    
    // AABB fattening margin for reducing updates
    static constexpr float AABB_MARGIN = 0.1f;
    static constexpr float AABB_MULTIPLIER = 2.0f;
    
    int allocateNode();
    void freeNode(int nodeIndex);
    
    void insertLeaf(int leafIndex);
    void removeLeaf(int leafIndex);
    
    int balance(int nodeIndex);
    void updateAABB(int nodeIndex);
    
    int computeHeight(int nodeIndex) const;
    
    // Find best sibling for new leaf
    int findBestSibling(int leafIndex) const;
    
    void queryRecursive(int nodeIndex, const AABB& aabb, std::vector<RigidBody*>& results) const;
    void raycastRecursive(int nodeIndex, const Ray& ray, float& maxDist, RaycastHit& hit,
                          std::function<bool(RigidBody*, RaycastHit&)>& hitCallback) const;
    void getPairsRecursive(int nodeA, int nodeB, std::vector<CollisionPair>& pairs) const;
};

// ============================================================================
// Broadphase - Main interface combining different algorithms
// ============================================================================
class Broadphase {
public:
    enum class Algorithm {
        SpatialHash,    // Good for dynamic worlds with uniform distribution
        AABBTree,       // Good for large worlds with varying density
        Hybrid          // Use both: spatial hash for dynamic, tree for static
    };
    
    explicit Broadphase(Algorithm algorithm = Algorithm::Hybrid);
    
    void setAlgorithm(Algorithm algo) { algorithm = algo; }
    Algorithm getAlgorithm() const { return algorithm; }
    
    // Add/remove bodies
    void add(RigidBody* body);
    void remove(RigidBody* body);
    
    // Update a body's position in the broadphase
    void update(RigidBody* body);
    
    // Rebuild the entire structure (call after major changes)
    void rebuild(const std::vector<RigidBody*>& bodies);
    
    // Get potential collision pairs for narrow phase
    void getPotentialPairs(std::vector<CollisionPair>& pairs);
    
    // Query all bodies intersecting an AABB
    void query(const AABB& aabb, std::vector<RigidBody*>& results);
    
    // Raycast through all bodies
    bool raycast(const Ray& ray, RaycastHit& hit,
                 std::function<bool(RigidBody*, RaycastHit&)> hitCallback);
    
    // Configuration
    void setSpatialHashCellSize(float size);
    
private:
    Algorithm algorithm;
    SpatialHashGrid spatialHash;
    AABBTree aabbTree;
    
    // For hybrid mode: track which structure each body is in
    std::unordered_map<RigidBody*, int> bodyToTreeNode;
    std::unordered_set<RigidBody*> dynamicBodies;
};

} // namespace Physics
