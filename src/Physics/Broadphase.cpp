#include "Broadphase.h"
#include "Collider.h"
#include <algorithm>
#include <cmath>
#include <stack>

namespace Physics {

// ============================================================================
// Spatial Hash Grid Implementation
// ============================================================================

SpatialHashGrid::SpatialHashGrid(float cellSize)
    : cellSize(cellSize)
    , invCellSize(1.0f / cellSize)
{
}

void SpatialHashGrid::clear() {
    cells.clear();
    bodyToCells.clear();
}

SpatialHashGrid::CellKey SpatialHashGrid::worldToCell(const glm::vec3& pos) const {
    return std::make_tuple(
        static_cast<int>(std::floor(pos.x * invCellSize)),
        static_cast<int>(std::floor(pos.y * invCellSize)),
        static_cast<int>(std::floor(pos.z * invCellSize))
    );
}

void SpatialHashGrid::getCellsForAABB(const AABB& aabb, std::vector<CellKey>& cellKeys) const {
    cellKeys.clear();
    
    int minX = static_cast<int>(std::floor(aabb.min.x * invCellSize));
    int minY = static_cast<int>(std::floor(aabb.min.y * invCellSize));
    int minZ = static_cast<int>(std::floor(aabb.min.z * invCellSize));
    int maxX = static_cast<int>(std::floor(aabb.max.x * invCellSize));
    int maxY = static_cast<int>(std::floor(aabb.max.y * invCellSize));
    int maxZ = static_cast<int>(std::floor(aabb.max.z * invCellSize));
    
    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            for (int z = minZ; z <= maxZ; z++) {
                cellKeys.emplace_back(x, y, z);
            }
        }
    }
}

void SpatialHashGrid::insert(RigidBody* body) {
    AABB aabb = body->getWorldAABB();
    std::vector<CellKey> cellKeys;
    getCellsForAABB(aabb, cellKeys);
    
    for (const auto& key : cellKeys) {
        cells[key].push_back(body);
    }
    
    bodyToCells[body] = std::move(cellKeys);
}

void SpatialHashGrid::remove(RigidBody* body) {
    auto it = bodyToCells.find(body);
    if (it == bodyToCells.end()) return;
    
    for (const auto& key : it->second) {
        auto cellIt = cells.find(key);
        if (cellIt != cells.end()) {
            auto& bodies = cellIt->second;
            bodies.erase(std::remove(bodies.begin(), bodies.end(), body), bodies.end());
            if (bodies.empty()) {
                cells.erase(cellIt);
            }
        }
    }
    
    bodyToCells.erase(it);
}

void SpatialHashGrid::update(RigidBody* body) {
    remove(body);
    insert(body);
}

void SpatialHashGrid::query(const AABB& aabb, std::vector<RigidBody*>& results) const {
    std::vector<CellKey> cellKeys;
    getCellsForAABB(aabb, cellKeys);
    
    std::unordered_set<RigidBody*> seen;
    
    for (const auto& key : cellKeys) {
        auto it = cells.find(key);
        if (it != cells.end()) {
            for (RigidBody* body : it->second) {
                if (seen.insert(body).second) {
                    if (body->getWorldAABB().intersects(aabb)) {
                        results.push_back(body);
                    }
                }
            }
        }
    }
}

void SpatialHashGrid::getPotentialPairs(std::vector<CollisionPair>& pairs) const {
    std::unordered_set<CollisionPair, CollisionPairHash> pairSet;
    
    for (const auto& [key, bodies] : cells) {
        // Check all pairs within this cell
        for (size_t i = 0; i < bodies.size(); i++) {
            for (size_t j = i + 1; j < bodies.size(); j++) {
                RigidBody* bodyA = bodies[i];
                RigidBody* bodyB = bodies[j];
                
                if (!bodyA->canCollideWith(*bodyB)) continue;
                
                if (bodyA->getWorldAABB().intersects(bodyB->getWorldAABB())) {
                    CollisionPair pair{bodyA, bodyB};
                    if (pairSet.insert(pair).second) {
                        pairs.push_back(pair);
                    }
                }
            }
        }
    }
}

// ============================================================================
// AABB Tree Implementation
// ============================================================================

AABBTree::AABBTree()
    : root(NULL_NODE)
    , nodeCount(0)
    , freeList(NULL_NODE)
{
    nodes.reserve(256);
}

int AABBTree::allocateNode() {
    if (freeList != NULL_NODE) {
        int nodeIndex = freeList;
        freeList = nodes[nodeIndex].parent;
        nodes[nodeIndex] = Node();
        nodeCount++;
        return nodeIndex;
    }
    
    int nodeIndex = static_cast<int>(nodes.size());
    nodes.push_back(Node());
    nodes[nodeIndex].parent = NULL_NODE;
    nodes[nodeIndex].left = NULL_NODE;
    nodes[nodeIndex].right = NULL_NODE;
    nodes[nodeIndex].height = 0;
    nodes[nodeIndex].body = nullptr;
    nodeCount++;
    return nodeIndex;
}

void AABBTree::freeNode(int nodeIndex) {
    nodes[nodeIndex].parent = freeList;
    nodes[nodeIndex].body = nullptr;
    freeList = nodeIndex;
    nodeCount--;
}

int AABBTree::insert(RigidBody* body) {
    int leafIndex = allocateNode();
    
    // Fatten the AABB slightly
    AABB fatAABB = body->getWorldAABB();
    glm::vec3 margin(AABB_MARGIN);
    fatAABB.min -= margin;
    fatAABB.max += margin;
    
    nodes[leafIndex].aabb = fatAABB;
    nodes[leafIndex].body = body;
    nodes[leafIndex].left = NULL_NODE;
    nodes[leafIndex].right = NULL_NODE;
    nodes[leafIndex].height = 0;
    
    insertLeaf(leafIndex);
    
    return leafIndex;
}

void AABBTree::insertLeaf(int leafIndex) {
    if (root == NULL_NODE) {
        root = leafIndex;
        nodes[root].parent = NULL_NODE;
        return;
    }
    
    // Find best sibling
    int sibling = findBestSibling(leafIndex);
    
    // Create new parent
    int oldParent = nodes[sibling].parent;
    int newParent = allocateNode();
    
    nodes[newParent].parent = oldParent;
    nodes[newParent].aabb = nodes[leafIndex].aabb.merged(nodes[sibling].aabb);
    nodes[newParent].height = nodes[sibling].height + 1;
    
    if (oldParent != NULL_NODE) {
        if (nodes[oldParent].left == sibling) {
            nodes[oldParent].left = newParent;
        } else {
            nodes[oldParent].right = newParent;
        }
    } else {
        root = newParent;
    }
    
    nodes[newParent].left = sibling;
    nodes[newParent].right = leafIndex;
    nodes[sibling].parent = newParent;
    nodes[leafIndex].parent = newParent;
    
    // Walk up and fix heights and AABBs
    int index = nodes[leafIndex].parent;
    while (index != NULL_NODE) {
        index = balance(index);
        updateAABB(index);
        index = nodes[index].parent;
    }
}

int AABBTree::findBestSibling(int leafIndex) const {
    AABB leafAABB = nodes[leafIndex].aabb;
    int index = root;
    
    while (!nodes[index].isLeaf()) {
        int left = nodes[index].left;
        int right = nodes[index].right;
        
        float area = nodes[index].aabb.getVolume();
        AABB combinedAABB = nodes[index].aabb.merged(leafAABB);
        float combinedArea = combinedAABB.getVolume();
        
        // Cost of creating new parent
        float cost = 2.0f * combinedArea;
        
        // Inheritance cost
        float inheritanceCost = 2.0f * (combinedArea - area);
        
        // Cost of descending to left
        float costLeft;
        if (nodes[left].isLeaf()) {
            costLeft = leafAABB.merged(nodes[left].aabb).getVolume() + inheritanceCost;
        } else {
            float oldArea = nodes[left].aabb.getVolume();
            float newArea = leafAABB.merged(nodes[left].aabb).getVolume();
            costLeft = (newArea - oldArea) + inheritanceCost;
        }
        
        // Cost of descending to right
        float costRight;
        if (nodes[right].isLeaf()) {
            costRight = leafAABB.merged(nodes[right].aabb).getVolume() + inheritanceCost;
        } else {
            float oldArea = nodes[right].aabb.getVolume();
            float newArea = leafAABB.merged(nodes[right].aabb).getVolume();
            costRight = (newArea - oldArea) + inheritanceCost;
        }
        
        // Descend to best option
        if (cost < costLeft && cost < costRight) {
            break;
        }
        
        index = (costLeft < costRight) ? left : right;
    }
    
    return index;
}

void AABBTree::remove(int nodeIndex) {
    removeLeaf(nodeIndex);
    freeNode(nodeIndex);
}

void AABBTree::removeLeaf(int leafIndex) {
    if (leafIndex == root) {
        root = NULL_NODE;
        return;
    }
    
    int parent = nodes[leafIndex].parent;
    int grandParent = nodes[parent].parent;
    int sibling = (nodes[parent].left == leafIndex) ? nodes[parent].right : nodes[parent].left;
    
    if (grandParent != NULL_NODE) {
        // Connect sibling to grandparent
        if (nodes[grandParent].left == parent) {
            nodes[grandParent].left = sibling;
        } else {
            nodes[grandParent].right = sibling;
        }
        nodes[sibling].parent = grandParent;
        freeNode(parent);
        
        // Walk up and fix heights and AABBs
        int index = grandParent;
        while (index != NULL_NODE) {
            index = balance(index);
            updateAABB(index);
            index = nodes[index].parent;
        }
    } else {
        root = sibling;
        nodes[sibling].parent = NULL_NODE;
        freeNode(parent);
    }
}

void AABBTree::update(int nodeIndex, const AABB& newAABB) {
    // Check if new AABB is still within fattened AABB
    if (nodes[nodeIndex].aabb.contains(newAABB.min) && 
        nodes[nodeIndex].aabb.contains(newAABB.max)) {
        return; // No need to update
    }
    
    RigidBody* body = nodes[nodeIndex].body;
    removeLeaf(nodeIndex);
    
    // Fatten new AABB
    AABB fatAABB = newAABB;
    glm::vec3 margin(AABB_MARGIN);
    fatAABB.min -= margin;
    fatAABB.max += margin;
    
    nodes[nodeIndex].aabb = fatAABB;
    
    insertLeaf(nodeIndex);
}

int AABBTree::balance(int nodeIndex) {
    if (nodes[nodeIndex].isLeaf() || nodes[nodeIndex].height < 2) {
        return nodeIndex;
    }
    
    int left = nodes[nodeIndex].left;
    int right = nodes[nodeIndex].right;
    
    int balance = nodes[right].height - nodes[left].height;
    
    // Rotate right subtree up
    if (balance > 1) {
        int rightLeft = nodes[right].left;
        int rightRight = nodes[right].right;
        
        // Swap node and right
        nodes[right].left = nodeIndex;
        nodes[right].parent = nodes[nodeIndex].parent;
        nodes[nodeIndex].parent = right;
        
        if (nodes[right].parent != NULL_NODE) {
            if (nodes[nodes[right].parent].left == nodeIndex) {
                nodes[nodes[right].parent].left = right;
            } else {
                nodes[nodes[right].parent].right = right;
            }
        } else {
            root = right;
        }
        
        // Rotate
        if (nodes[rightLeft].height > nodes[rightRight].height) {
            nodes[right].right = rightLeft;
            nodes[nodeIndex].right = rightRight;
            nodes[rightRight].parent = nodeIndex;
            
            updateAABB(nodeIndex);
            updateAABB(right);
        } else {
            nodes[right].right = rightRight;
            nodes[nodeIndex].right = rightLeft;
            nodes[rightLeft].parent = nodeIndex;
            
            updateAABB(nodeIndex);
            updateAABB(right);
        }
        
        return right;
    }
    
    // Rotate left subtree up
    if (balance < -1) {
        int leftLeft = nodes[left].left;
        int leftRight = nodes[left].right;
        
        nodes[left].left = nodeIndex;
        nodes[left].parent = nodes[nodeIndex].parent;
        nodes[nodeIndex].parent = left;
        
        if (nodes[left].parent != NULL_NODE) {
            if (nodes[nodes[left].parent].left == nodeIndex) {
                nodes[nodes[left].parent].left = left;
            } else {
                nodes[nodes[left].parent].right = left;
            }
        } else {
            root = left;
        }
        
        if (nodes[leftLeft].height > nodes[leftRight].height) {
            nodes[left].right = leftLeft;
            nodes[nodeIndex].left = leftRight;
            nodes[leftRight].parent = nodeIndex;
            
            updateAABB(nodeIndex);
            updateAABB(left);
        } else {
            nodes[left].right = leftRight;
            nodes[nodeIndex].left = leftLeft;
            nodes[leftLeft].parent = nodeIndex;
            
            updateAABB(nodeIndex);
            updateAABB(left);
        }
        
        return left;
    }
    
    return nodeIndex;
}

void AABBTree::updateAABB(int nodeIndex) {
    int left = nodes[nodeIndex].left;
    int right = nodes[nodeIndex].right;
    
    nodes[nodeIndex].aabb = nodes[left].aabb.merged(nodes[right].aabb);
    nodes[nodeIndex].height = 1 + std::max(nodes[left].height, nodes[right].height);
}

void AABBTree::query(const AABB& aabb, std::vector<RigidBody*>& results) const {
    if (root == NULL_NODE) return;
    queryRecursive(root, aabb, results);
}

void AABBTree::queryRecursive(int nodeIndex, const AABB& aabb, std::vector<RigidBody*>& results) const {
    if (!nodes[nodeIndex].aabb.intersects(aabb)) {
        return;
    }
    
    if (nodes[nodeIndex].isLeaf()) {
        results.push_back(nodes[nodeIndex].body);
    } else {
        queryRecursive(nodes[nodeIndex].left, aabb, results);
        queryRecursive(nodes[nodeIndex].right, aabb, results);
    }
}

bool AABBTree::raycast(const Ray& ray, RaycastHit& hit,
                        std::function<bool(RigidBody*, RaycastHit&)> hitCallback) const {
    if (root == NULL_NODE) return false;
    
    float maxDist = ray.maxDistance;
    raycastRecursive(root, ray, maxDist, hit, hitCallback);
    return hit.hit;
}

void AABBTree::raycastRecursive(int nodeIndex, const Ray& ray, float& maxDist, RaycastHit& hit,
                                 std::function<bool(RigidBody*, RaycastHit&)>& hitCallback) const {
    // Quick AABB test
    RaycastHit aabbHit;
    BoxCollider boxCol(nodes[nodeIndex].aabb.getExtents());
    boxCol.setOffset(nodes[nodeIndex].aabb.getCenter());
    
    if (!boxCol.raycast(ray, aabbHit) || aabbHit.distance > maxDist) {
        return;
    }
    
    if (nodes[nodeIndex].isLeaf()) {
        RaycastHit bodyHit;
        if (hitCallback(nodes[nodeIndex].body, bodyHit)) {
            if (bodyHit.distance < maxDist) {
                maxDist = bodyHit.distance;
                hit = bodyHit;
            }
        }
    } else {
        // Recurse to children, closest first
        int left = nodes[nodeIndex].left;
        int right = nodes[nodeIndex].right;
        
        glm::vec3 leftCenter = nodes[left].aabb.getCenter();
        glm::vec3 rightCenter = nodes[right].aabb.getCenter();
        
        float distLeft = glm::dot(leftCenter - ray.origin, ray.direction);
        float distRight = glm::dot(rightCenter - ray.origin, ray.direction);
        
        if (distLeft < distRight) {
            raycastRecursive(left, ray, maxDist, hit, hitCallback);
            raycastRecursive(right, ray, maxDist, hit, hitCallback);
        } else {
            raycastRecursive(right, ray, maxDist, hit, hitCallback);
            raycastRecursive(left, ray, maxDist, hit, hitCallback);
        }
    }
}

void AABBTree::getPotentialPairs(std::vector<CollisionPair>& pairs) const {
    if (root == NULL_NODE) return;
    getPairsRecursive(root, root, pairs);
}

void AABBTree::getPairsRecursive(int nodeA, int nodeB, std::vector<CollisionPair>& pairs) const {
    if (nodeA == NULL_NODE || nodeB == NULL_NODE) return;
    
    // Don't check node against itself for leaf nodes
    if (nodeA == nodeB) {
        if (!nodes[nodeA].isLeaf()) {
            getPairsRecursive(nodes[nodeA].left, nodes[nodeA].left, pairs);
            getPairsRecursive(nodes[nodeA].left, nodes[nodeA].right, pairs);
            getPairsRecursive(nodes[nodeA].right, nodes[nodeA].right, pairs);
        }
        return;
    }
    
    if (!nodes[nodeA].aabb.intersects(nodes[nodeB].aabb)) {
        return;
    }
    
    bool leafA = nodes[nodeA].isLeaf();
    bool leafB = nodes[nodeB].isLeaf();
    
    if (leafA && leafB) {
        RigidBody* bodyA = nodes[nodeA].body;
        RigidBody* bodyB = nodes[nodeB].body;
        
        if (bodyA->canCollideWith(*bodyB)) {
            pairs.push_back({bodyA, bodyB});
        }
    } else if (leafA) {
        getPairsRecursive(nodeA, nodes[nodeB].left, pairs);
        getPairsRecursive(nodeA, nodes[nodeB].right, pairs);
    } else if (leafB) {
        getPairsRecursive(nodes[nodeA].left, nodeB, pairs);
        getPairsRecursive(nodes[nodeA].right, nodeB, pairs);
    } else {
        getPairsRecursive(nodes[nodeA].left, nodes[nodeB].left, pairs);
        getPairsRecursive(nodes[nodeA].left, nodes[nodeB].right, pairs);
        getPairsRecursive(nodes[nodeA].right, nodes[nodeB].left, pairs);
        getPairsRecursive(nodes[nodeA].right, nodes[nodeB].right, pairs);
    }
}

int AABBTree::getHeight() const {
    return computeHeight(root);
}

int AABBTree::computeHeight(int nodeIndex) const {
    if (nodeIndex == NULL_NODE) return 0;
    return nodes[nodeIndex].height;
}

// ============================================================================
// Broadphase Implementation
// ============================================================================

Broadphase::Broadphase(Algorithm algorithm)
    : algorithm(algorithm)
    , spatialHash(4.0f)
{
}

void Broadphase::add(RigidBody* body) {
    switch (algorithm) {
        case Algorithm::SpatialHash:
            spatialHash.insert(body);
            break;
            
        case Algorithm::AABBTree:
            bodyToTreeNode[body] = aabbTree.insert(body);
            break;
            
        case Algorithm::Hybrid:
            if (body->isDynamic()) {
                spatialHash.insert(body);
                dynamicBodies.insert(body);
            } else {
                bodyToTreeNode[body] = aabbTree.insert(body);
            }
            break;
    }
}

void Broadphase::remove(RigidBody* body) {
    switch (algorithm) {
        case Algorithm::SpatialHash:
            spatialHash.remove(body);
            break;
            
        case Algorithm::AABBTree:
            if (auto it = bodyToTreeNode.find(body); it != bodyToTreeNode.end()) {
                aabbTree.remove(it->second);
                bodyToTreeNode.erase(it);
            }
            break;
            
        case Algorithm::Hybrid:
            if (dynamicBodies.count(body)) {
                spatialHash.remove(body);
                dynamicBodies.erase(body);
            } else if (auto it = bodyToTreeNode.find(body); it != bodyToTreeNode.end()) {
                aabbTree.remove(it->second);
                bodyToTreeNode.erase(it);
            }
            break;
    }
}

void Broadphase::update(RigidBody* body) {
    switch (algorithm) {
        case Algorithm::SpatialHash:
            spatialHash.update(body);
            break;
            
        case Algorithm::AABBTree:
            if (auto it = bodyToTreeNode.find(body); it != bodyToTreeNode.end()) {
                aabbTree.update(it->second, body->getWorldAABB());
            }
            break;
            
        case Algorithm::Hybrid:
            if (dynamicBodies.count(body)) {
                spatialHash.update(body);
            } else if (auto it = bodyToTreeNode.find(body); it != bodyToTreeNode.end()) {
                aabbTree.update(it->second, body->getWorldAABB());
            }
            break;
    }
}

void Broadphase::rebuild(const std::vector<RigidBody*>& bodies) {
    // Clear everything
    spatialHash.clear();
    bodyToTreeNode.clear();
    dynamicBodies.clear();
    aabbTree = AABBTree();
    
    // Re-add all bodies
    for (RigidBody* body : bodies) {
        add(body);
    }
}

void Broadphase::getPotentialPairs(std::vector<CollisionPair>& pairs) {
    pairs.clear();
    
    switch (algorithm) {
        case Algorithm::SpatialHash:
            spatialHash.getPotentialPairs(pairs);
            break;
            
        case Algorithm::AABBTree:
            aabbTree.getPotentialPairs(pairs);
            break;
            
        case Algorithm::Hybrid: {
            // Get pairs from spatial hash (dynamic-dynamic)
            spatialHash.getPotentialPairs(pairs);
            
            // Get pairs from tree (static-static)
            aabbTree.getPotentialPairs(pairs);
            
            // Check dynamic vs static
            for (RigidBody* dynamicBody : dynamicBodies) {
                std::vector<RigidBody*> nearbyStatic;
                aabbTree.query(dynamicBody->getWorldAABB(), nearbyStatic);
                
                for (RigidBody* staticBody : nearbyStatic) {
                    if (dynamicBody->canCollideWith(*staticBody)) {
                        pairs.push_back({dynamicBody, staticBody});
                    }
                }
            }
            break;
        }
    }
}

void Broadphase::query(const AABB& aabb, std::vector<RigidBody*>& results) {
    results.clear();
    
    switch (algorithm) {
        case Algorithm::SpatialHash:
            spatialHash.query(aabb, results);
            break;
            
        case Algorithm::AABBTree:
            aabbTree.query(aabb, results);
            break;
            
        case Algorithm::Hybrid:
            spatialHash.query(aabb, results);
            aabbTree.query(aabb, results);
            break;
    }
}

bool Broadphase::raycast(const Ray& ray, RaycastHit& hit,
                          std::function<bool(RigidBody*, RaycastHit&)> hitCallback) {
    hit.hit = false;
    hit.distance = ray.maxDistance;
    
    switch (algorithm) {
        case Algorithm::SpatialHash: {
            // Query cells along ray path (simplified)
            AABB rayAABB(
                glm::min(ray.origin, ray.getPoint(ray.maxDistance)),
                glm::max(ray.origin, ray.getPoint(ray.maxDistance))
            );
            
            std::vector<RigidBody*> candidates;
            spatialHash.query(rayAABB, candidates);
            
            for (RigidBody* body : candidates) {
                RaycastHit bodyHit;
                if (hitCallback(body, bodyHit) && bodyHit.distance < hit.distance) {
                    hit = bodyHit;
                }
            }
            break;
        }
        
        case Algorithm::AABBTree:
            aabbTree.raycast(ray, hit, hitCallback);
            break;
            
        case Algorithm::Hybrid: {
            // Check tree first
            aabbTree.raycast(ray, hit, hitCallback);
            
            // Then check dynamic bodies
            AABB rayAABB(
                glm::min(ray.origin, ray.getPoint(hit.hit ? hit.distance : ray.maxDistance)),
                glm::max(ray.origin, ray.getPoint(hit.hit ? hit.distance : ray.maxDistance))
            );
            
            std::vector<RigidBody*> candidates;
            spatialHash.query(rayAABB, candidates);
            
            for (RigidBody* body : candidates) {
                RaycastHit bodyHit;
                if (hitCallback(body, bodyHit) && bodyHit.distance < hit.distance) {
                    hit = bodyHit;
                }
            }
            break;
        }
    }
    
    return hit.hit;
}

void Broadphase::setSpatialHashCellSize(float size) {
    spatialHash.setCellSize(size);
}

} // namespace Physics
