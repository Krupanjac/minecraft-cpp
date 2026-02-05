// ============================================================================
// VxStruct Editor - Edit Actions
// Undo/redo system, clipboard, selection operations, rotation, fill.
// ============================================================================
#include "EditorApp.h"

// ============================================================================
// Undo / Redo
// ============================================================================

void VxStructEditor::pushAction(const EditorAction& action) {
    m_undoStack.push_back(action);
    if (m_undoStack.size() > MAX_UNDO) {
        m_undoStack.pop_front();
    }
    m_redoStack.clear();
}

void VxStructEditor::undo() {
    if (m_undoStack.empty()) return;

    EditorAction action = m_undoStack.back();
    m_undoStack.pop_back();

    switch (action.type) {
        case EditorAction::Type::PLACE_BLOCK:
            if (action.previousType == BlockType::AIR)
                m_structure.removeBlock(action.position);
            else
                m_structure.setBlock(action.position, action.previousType, action.previousMetadata);
            break;

        case EditorAction::Type::REMOVE_BLOCK:
            m_structure.setBlock(action.position, action.blockType, action.metadata);
            break;

        case EditorAction::Type::PLACE_MULTIPLE:
        case EditorAction::Type::PASTE:
        case EditorAction::Type::FILL_SELECTION:
            for (const auto& b : action.blocks) {
                m_structure.removeBlock(b.position);
            }
            for (const auto& b : action.previousBlocks) {
                if (b.type != BlockType::AIR)
                    m_structure.setBlock(b.position, b.type, b.metadata);
            }
            break;

        case EditorAction::Type::REMOVE_MULTIPLE:
            for (const auto& b : action.blocks) {
                m_structure.setBlock(b.position, b.type, b.metadata);
            }
            break;
    }

    m_redoStack.push_back(action);
    m_modified = true;
    rebuildBlockMesh();
}

void VxStructEditor::redo() {
    if (m_redoStack.empty()) return;

    EditorAction action = m_redoStack.back();
    m_redoStack.pop_back();

    switch (action.type) {
        case EditorAction::Type::PLACE_BLOCK:
            m_structure.setBlock(action.position, action.blockType, action.metadata);
            break;

        case EditorAction::Type::REMOVE_BLOCK:
            m_structure.removeBlock(action.position);
            break;

        case EditorAction::Type::PLACE_MULTIPLE:
        case EditorAction::Type::PASTE:
        case EditorAction::Type::FILL_SELECTION:
            for (const auto& b : action.blocks) {
                m_structure.setBlock(b.position, b.type, b.metadata);
            }
            break;

        case EditorAction::Type::REMOVE_MULTIPLE:
            for (const auto& b : action.blocks) {
                m_structure.removeBlock(b.position);
            }
            break;
    }

    m_undoStack.push_back(action);
    m_modified = true;
    rebuildBlockMesh();
}

// ============================================================================
// Block Operations with Undo
// ============================================================================

void VxStructEditor::placeBlockWithUndo(const glm::ivec3& pos, BlockType type, uint8_t metadata) {
    EditorAction action;
    action.type = EditorAction::Type::PLACE_BLOCK;
    action.position = pos;
    action.blockType = type;
    action.metadata = metadata;
    action.previousType = m_structure.getBlock(pos);
    action.previousMetadata = 0;
    pushAction(action);

    m_structure.setBlock(pos, type, metadata);
    m_modified = true;
    rebuildBlockMesh();
}

void VxStructEditor::removeBlockWithUndo(const glm::ivec3& pos) {
    BlockType existing = m_structure.getBlock(pos);
    if (existing == BlockType::AIR) return;

    EditorAction action;
    action.type = EditorAction::Type::REMOVE_BLOCK;
    action.position = pos;
    action.blockType = existing;
    action.metadata = 0;
    pushAction(action);

    m_structure.removeBlock(pos);
    m_modified = true;
    rebuildBlockMesh();
}

void VxStructEditor::deleteSelectedBlocks() {
    if (m_selectedBlocks.empty()) return;

    EditorAction action;
    action.type = EditorAction::Type::REMOVE_MULTIPLE;

    for (int64_t enc : m_selectedBlocks) {
        glm::ivec3 pos = decodePos(enc);
        BlockType type = m_structure.getBlock(pos);
        if (type != BlockType::AIR) {
            StructureBlock sb;
            sb.position = pos;
            sb.type = type;
            sb.metadata = 0;
            action.blocks.push_back(sb);
        }
    }

    if (action.blocks.empty()) return;

    pushAction(action);

    for (const auto& b : action.blocks) {
        m_structure.removeBlock(b.position);
    }

    m_selectedBlocks.clear();
    m_modified = true;
    rebuildBlockMesh();
}

// ============================================================================
// Clipboard
// ============================================================================

void VxStructEditor::copySelection() {
    if (m_selectedBlocks.empty()) return;

    m_clipboard.clear();
    glm::ivec3 minPos(INT_MAX);

    for (int64_t enc : m_selectedBlocks) {
        glm::ivec3 pos = decodePos(enc);
        BlockType type = m_structure.getBlock(pos);
        if (type != BlockType::AIR) {
            StructureBlock sb;
            sb.position = pos;
            sb.type = type;
            sb.metadata = 0;
            m_clipboard.push_back(sb);
            minPos = glm::min(minPos, pos);
        }
    }

    m_clipboardOrigin = minPos;
}

void VxStructEditor::cutSelection() {
    if (m_selectedBlocks.empty()) return;

    // Copy to clipboard first
    copySelection();

    // Then delete the selected blocks (with undo support)
    deleteSelectedBlocks();
}

void VxStructEditor::moveSelection() {
    if (m_clipboard.empty() || !m_hasHover) return;

    // Compute offset from clipboard origin to current hover position
    glm::ivec3 offset = m_hoverPlacePos - m_clipboardOrigin;
    if (offset == glm::ivec3(0)) return; // No movement

    // Build undo action that captures both the removal and placement
    EditorAction action;
    action.type = EditorAction::Type::PASTE;

    // Record what will be removed (the clipboard source blocks)
    // and what's currently at the destination
    std::vector<StructureBlock> toPlace;
    for (const auto& cb : m_clipboard) {
        glm::ivec3 newPos = cb.position + offset;

        // Record previous state at destination
        BlockType prevType = m_structure.getBlock(newPos);
        StructureBlock prev;
        prev.position = newPos;
        prev.type = prevType;
        prev.metadata = 0;
        action.previousBlocks.push_back(prev);

        // New block at destination
        StructureBlock nb;
        nb.position = newPos;
        nb.type = cb.type;
        nb.metadata = cb.metadata;
        action.blocks.push_back(nb);
    }

    pushAction(action);

    // Place blocks at new positions
    for (const auto& b : action.blocks) {
        m_structure.setBlock(b.position, b.type, b.metadata);
    }

    // Update clipboard origin so subsequent moves are relative
    m_clipboardOrigin = m_hoverPlacePos;

    // Update selection to new positions
    m_selectedBlocks.clear();
    for (const auto& b : action.blocks) {
        m_selectedBlocks.insert(encodePos(b.position));
    }

    // Update clipboard block positions for future moves
    for (auto& cb : m_clipboard) {
        cb.position = cb.position + offset;
    }

    m_modified = true;
    rebuildBlockMesh();
}

void VxStructEditor::pasteClipboard() {
    if (m_clipboard.empty()) return;

    glm::ivec3 offset = m_hoverPlacePos - m_clipboardOrigin;

    EditorAction action;
    action.type = EditorAction::Type::PASTE;

    for (const auto& cb : m_clipboard) {
        glm::ivec3 newPos = cb.position + offset;

        BlockType prevType = m_structure.getBlock(newPos);
        StructureBlock prev;
        prev.position = newPos;
        prev.type = prevType;
        prev.metadata = 0;
        action.previousBlocks.push_back(prev);

        StructureBlock nb;
        nb.position = newPos;
        nb.type = cb.type;
        nb.metadata = cb.metadata;
        action.blocks.push_back(nb);
    }

    pushAction(action);

    for (const auto& b : action.blocks) {
        m_structure.setBlock(b.position, b.type, b.metadata);
    }

    m_modified = true;
    rebuildBlockMesh();
}

void VxStructEditor::duplicateSelection() {
    copySelection();
    pasteClipboard();
}

// ============================================================================
// Selection Operations
// ============================================================================

void VxStructEditor::selectAll() {
    m_selectedBlocks.clear();
    for (const auto& block : m_structure.getBlocks()) {
        m_selectedBlocks.insert(encodePos(block.position));
    }
}

void VxStructEditor::deselectAll() {
    m_selectedBlocks.clear();
    m_hasSelectionAnchor = false;
}

void VxStructEditor::invertSelection() {
    std::set<int64_t> newSel;
    for (const auto& block : m_structure.getBlocks()) {
        int64_t enc = encodePos(block.position);
        if (m_selectedBlocks.count(enc) == 0) {
            newSel.insert(enc);
        }
    }
    m_selectedBlocks = newSel;
}

void VxStructEditor::selectRange(const glm::ivec3& from, const glm::ivec3& to) {
    // Compute the axis-aligned bounding box between 'from' and 'to'
    glm::ivec3 minP = glm::min(from, to);
    glm::ivec3 maxP = glm::max(from, to);

    // Select every existing block whose position falls inside the AABB
    const auto& blocks = m_structure.getBlocks();
    for (const auto& block : blocks) {
        const glm::ivec3& p = block.position;
        if (p.x >= minP.x && p.x <= maxP.x &&
            p.y >= minP.y && p.y <= maxP.y &&
            p.z >= minP.z && p.z <= maxP.z) {
            m_selectedBlocks.insert(encodePos(p));
        }
    }
}

// ============================================================================
// Rotation
// ============================================================================

void VxStructEditor::rotateStructure() {
    const auto& blocks = m_structure.getBlocks();
    if (blocks.empty()) return;

    glm::ivec3 minP(INT_MAX), maxP(INT_MIN);
    for (const auto& b : blocks) {
        minP = glm::min(minP, b.position);
        maxP = glm::max(maxP, b.position);
    }
    glm::vec3 center = glm::vec3(minP + maxP) * 0.5f;

    std::vector<StructureBlock> oldBlocks = blocks;

    m_structure.clear();

    std::vector<StructureBlock> newBlocks;
    for (const auto& b : oldBlocks) {
        glm::vec3 rel = glm::vec3(b.position) - center;
        glm::vec3 rot(rel.z, rel.y, -rel.x);
        glm::ivec3 newPos = glm::ivec3(glm::round(rot + center));

        StructureBlock nb;
        nb.position = newPos;
        nb.type = b.type;
        nb.metadata = b.metadata;
        newBlocks.push_back(nb);
    }

    EditorAction combined;
    combined.type = EditorAction::Type::PLACE_MULTIPLE;
    combined.blocks = newBlocks;
    combined.previousBlocks = oldBlocks;
    pushAction(combined);

    for (const auto& b : newBlocks) {
        m_structure.setBlock(b.position, b.type, b.metadata);
    }

    m_selectedBlocks.clear();
    m_modified = true;
    rebuildBlockMesh();
}

void VxStructEditor::rotateSelection() {
    if (m_selectedBlocks.empty()) return;

    std::vector<StructureBlock> selBlocks;
    glm::ivec3 minP(INT_MAX), maxP(INT_MIN);
    for (int64_t enc : m_selectedBlocks) {
        glm::ivec3 pos = decodePos(enc);
        BlockType type = m_structure.getBlock(pos);
        if (type != BlockType::AIR) {
            StructureBlock sb;
            sb.position = pos;
            sb.type = type;
            sb.metadata = 0;
            selBlocks.push_back(sb);
            minP = glm::min(minP, pos);
            maxP = glm::max(maxP, pos);
        }
    }
    if (selBlocks.empty()) return;

    glm::vec3 center = glm::vec3(minP + maxP) * 0.5f;

    EditorAction action;
    action.type = EditorAction::Type::PLACE_MULTIPLE;
    action.previousBlocks = selBlocks;

    for (const auto& b : selBlocks) {
        m_structure.removeBlock(b.position);
    }

    std::set<int64_t> newSelection;
    for (const auto& b : selBlocks) {
        glm::vec3 rel = glm::vec3(b.position) - center;
        glm::vec3 rot(rel.z, rel.y, -rel.x);
        glm::ivec3 newPos = glm::ivec3(glm::round(rot + center));

        StructureBlock nb;
        nb.position = newPos;
        nb.type = b.type;
        nb.metadata = b.metadata;
        action.blocks.push_back(nb);

        m_structure.setBlock(newPos, nb.type, nb.metadata);
        newSelection.insert(encodePos(newPos));
    }

    pushAction(action);
    m_selectedBlocks = newSelection;
    m_modified = true;
    rebuildBlockMesh();
}

// ============================================================================
// Fill Selection
// ============================================================================

void VxStructEditor::fillSelection(BlockType type) {
    if (m_selectedBlocks.empty()) return;

    EditorAction action;
    action.type = EditorAction::Type::FILL_SELECTION;

    for (int64_t enc : m_selectedBlocks) {
        glm::ivec3 pos = decodePos(enc);

        BlockType prevType = m_structure.getBlock(pos);
        StructureBlock prev;
        prev.position = pos;
        prev.type = prevType;
        prev.metadata = 0;
        action.previousBlocks.push_back(prev);

        StructureBlock nb;
        nb.position = pos;
        nb.type = type;
        nb.metadata = 0;
        action.blocks.push_back(nb);
    }

    pushAction(action);

    for (const auto& b : action.blocks) {
        m_structure.setBlock(b.position, b.type, b.metadata);
    }

    m_modified = true;
    rebuildBlockMesh();
}
