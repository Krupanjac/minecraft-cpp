// ============================================================================
// VxStruct Editor - File I/O & GLFW Callbacks
// New/Load/Save structures, GLFW event callbacks.
// ============================================================================
#include "EditorApp.h"

// ============================================================================
// File Operations
// ============================================================================

void VxStructEditor::newStructure() {
    m_structure.clear();
    m_structure.setName("New Structure");
    m_structure.setAuthor("VxStruct Editor");
    m_structure.setCategory(StructureCategory::MISC);
    m_currentFilePath.clear();
    m_modified = false;

    // Clear editor state
    m_undoStack.clear();
    m_redoStack.clear();
    m_selectedBlocks.clear();
    m_clipboard.clear();
    m_boxSelectActive = false;
    m_boxSelectHasStart = false;
    m_hasSelectionAnchor = false;

    strncpy_s(m_nameBuffer, "New Structure", sizeof(m_nameBuffer));
    strncpy_s(m_authorBuffer, "VxStruct Editor", sizeof(m_authorBuffer));
    m_categoryIndex = static_cast<int>(StructureCategory::MISC);
    m_requiresFlat = true;
    m_minGroundCoverage = 0.7f;

    rebuildBlockMesh();

    // Update window title
    glfwSetWindowTitle(m_window, "VxStruct Editor - New Structure");
}

void VxStructEditor::loadStructure() {
    std::string path = openFileDialog(
        "VxStruct Files (*.vxstruct)\0*.vxstruct\0JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0",
        "Open VxStruct File"
    );

    if (path.empty()) return;

    Structure newStruct;
    if (newStruct.loadFromFile(path)) {
        m_structure = newStruct;
        m_currentFilePath = path;
        m_modified = false;

        strncpy_s(m_nameBuffer, m_structure.getName().c_str(), sizeof(m_nameBuffer));
        strncpy_s(m_authorBuffer, m_structure.getAuthor().c_str(), sizeof(m_authorBuffer));
        m_categoryIndex = static_cast<int>(m_structure.getCategory());
        m_requiresFlat = m_structure.requiresFlat();
        m_minGroundCoverage = m_structure.getMinGroundCoverage();

        rebuildBlockMesh();

        std::string title = "VxStruct Editor - " + m_structure.getName();
        glfwSetWindowTitle(m_window, title.c_str());
        
        std::cout << "Loaded structure: " << m_structure.getName() 
                  << " (" << m_structure.getBlocks().size() << " blocks)" << std::endl;

        // Track in recent files
        m_settings.addRecentFile(path);
        m_settings.save(m_settingsFilePath);
    } else {
        std::cerr << "Failed to load structure: " << path << std::endl;
    }
}

void VxStructEditor::saveStructure() {
    if (m_currentFilePath.empty()) {
        saveStructureAs();
        return;
    }

    if (m_structure.saveToFile(m_currentFilePath)) {
        m_modified = false;
        std::string title = "VxStruct Editor - " + m_structure.getName();
        glfwSetWindowTitle(m_window, title.c_str());
        std::cout << "Saved: " << m_currentFilePath << std::endl;

        // Track in recent files
        m_settings.addRecentFile(m_currentFilePath);
        m_settings.save(m_settingsFilePath);
    }
}

void VxStructEditor::saveStructureAs() {
    std::string path = saveFileDialog(
        "VxStruct Files (*.vxstruct)\0*.vxstruct\0JSON Files (*.json)\0*.json\0",
        "Save VxStruct File",
        "vxstruct"
    );

    if (path.empty()) return;

    m_currentFilePath = path;
    saveStructure();
}

void VxStructEditor::exportStructure() {
    // Future: export to other formats
    saveStructureAs();
}

void VxStructEditor::openRecentFile(const std::string& path) {
    Structure newStruct;
    if (newStruct.loadFromFile(path)) {
        m_structure = newStruct;
        m_currentFilePath = path;
        m_modified = false;

        strncpy_s(m_nameBuffer, m_structure.getName().c_str(), sizeof(m_nameBuffer));
        strncpy_s(m_authorBuffer, m_structure.getAuthor().c_str(), sizeof(m_authorBuffer));
        m_categoryIndex = static_cast<int>(m_structure.getCategory());
        m_requiresFlat = m_structure.requiresFlat();
        m_minGroundCoverage = m_structure.getMinGroundCoverage();

        // Clear editor state
        m_undoStack.clear();
        m_redoStack.clear();
        m_selectedBlocks.clear();
        m_clipboard.clear();
        m_boxSelectActive = false;
        m_boxSelectHasStart = false;
        m_hasSelectionAnchor = false;

        rebuildBlockMesh();

        std::string title = "VxStruct Editor - " + m_structure.getName();
        glfwSetWindowTitle(m_window, title.c_str());

        // Update recent files
        m_settings.addRecentFile(path);
        m_settings.save(m_settingsFilePath);

        std::cout << "Opened recent: " << path << std::endl;
    } else {
        std::cerr << "Failed to open recent file: " << path << std::endl;
        // Remove invalid entry from recent files
        m_settings.recentFiles.erase(
            std::remove(m_settings.recentFiles.begin(), m_settings.recentFiles.end(), path),
            m_settings.recentFiles.end()
        );
        m_settings.save(m_settingsFilePath);
    }
}

void VxStructEditor::autoSave() {
    if (!m_settings.autoSaveEnabled) return;
    if (!m_modified) return;
    if (m_currentFilePath.empty()) return;

    double now = glfwGetTime();
    if (now - m_lastAutoSaveTime < m_settings.autoSaveIntervalSec) return;

    if (m_structure.saveToFile(m_currentFilePath)) {
        m_modified = false;
        m_lastAutoSaveTime = now;
        std::string title = "VxStruct Editor - " + m_structure.getName();
        glfwSetWindowTitle(m_window, title.c_str());
        std::cout << "Auto-saved: " << m_currentFilePath << std::endl;
    }
}

void VxStructEditor::updateTextureMode() {
    if (m_settings.usePBRTextures && m_pbrAlbedoArray) {
        m_textureMode = 2; // PBR
    } else if (m_atlasTexture) {
        m_textureMode = 1; // Atlas
    } else {
        m_textureMode = 0; // Color only
    }
    // Rebuild mesh with new texture mode UVs
    rebuildBlockMesh();
}

// ============================================================================
// GLFW Callbacks
// ============================================================================

void VxStructEditor::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* editor = static_cast<VxStructEditor*>(glfwGetWindowUserPointer(window));
    editor->m_windowWidth = width;
    editor->m_windowHeight = height;
}

void VxStructEditor::scrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    auto* editor = static_cast<VxStructEditor*>(glfwGetWindowUserPointer(window));
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    editor->m_camera.distance -= (float)yoffset * editor->m_camera.distance * 0.1f;
    editor->m_camera.distance = glm::clamp(editor->m_camera.distance, 2.0f, 200.0f);
}

void VxStructEditor::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto* editor = static_cast<VxStructEditor*>(glfwGetWindowUserPointer(window));
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        if (!editor->m_hasHover) return;

        switch (editor->m_currentTool) {
            case EditorTool::PLACE: {
                editor->placeBlockWithUndo(editor->m_hoverPlacePos, editor->m_selectedBlock);
                break;
            }
            case EditorTool::ERASE: {
                if (editor->m_hoverHit.hit) {
                    editor->removeBlockWithUndo(editor->m_hoverHit.blockPos);
                }
                break;
            }
            case EditorTool::PICK: {
                if (editor->m_hoverHit.hit) {
                    editor->m_selectedBlock = editor->m_structure.getBlock(editor->m_hoverHit.blockPos);
                    editor->m_currentTool = EditorTool::PLACE;
                }
                break;
            }
            case EditorTool::MARKER: {
                editor->m_structure.addMarker(editor->m_hoverPlacePos, 
                                               editor->m_markerType, 
                                               editor->m_markerData);
                editor->m_modified = true;
                break;
            }
            case EditorTool::SELECT: {
                if (editor->m_hoverHit.hit) {
                    glm::ivec3 clickedPos = editor->m_hoverHit.blockPos;
                    bool shiftHeld = (mods & GLFW_MOD_SHIFT) != 0;

                    if (shiftHeld && editor->m_hasSelectionAnchor) {
                        // Shift + Click with existing anchor: range select
                        editor->selectRange(editor->m_selectionAnchor, clickedPos);
                    } else if (shiftHeld) {
                        // Shift + Click without anchor: toggle single block
                        int64_t enc = encodePos(clickedPos);
                        if (editor->m_selectedBlocks.count(enc))
                            editor->m_selectedBlocks.erase(enc);
                        else
                            editor->m_selectedBlocks.insert(enc);
                        // Set this as the anchor for future range selects
                        editor->m_selectionAnchor = clickedPos;
                        editor->m_hasSelectionAnchor = true;
                    } else {
                        // Plain click: select single block, set anchor
                        bool wasSelected = editor->m_selectedBlocks.count(encodePos(clickedPos)) > 0;
                        editor->m_selectedBlocks.clear();
                        if (!wasSelected) {
                            editor->m_selectedBlocks.insert(encodePos(clickedPos));
                        }
                        // Always update anchor on plain click
                        editor->m_selectionAnchor = clickedPos;
                        editor->m_hasSelectionAnchor = true;
                    }
                } else {
                    // Clicked empty space: deselect all
                    if (!(mods & GLFW_MOD_SHIFT)) {
                        editor->m_selectedBlocks.clear();
                        editor->m_hasSelectionAnchor = false;
                    }
                }
                break;
            }
        }
    }

    // Right click: quick erase with undo
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        if (editor->m_hoverHit.hit) {
            editor->removeBlockWithUndo(editor->m_hoverHit.blockPos);
        }
    }
}

void VxStructEditor::keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int mods) {
    auto* editor = static_cast<VxStructEditor*>(glfwGetWindowUserPointer(window));
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return;

    if (action != GLFW_PRESS) return;

    bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    bool shift = (mods & GLFW_MOD_SHIFT) != 0;

    // Tool selection (no modifiers)
    if (!ctrl && !shift) {
        switch (key) {
            case GLFW_KEY_Q: editor->m_currentTool = EditorTool::SELECT; break;
            case GLFW_KEY_1: editor->m_currentTool = EditorTool::PLACE; break;
            case GLFW_KEY_2: editor->m_currentTool = EditorTool::ERASE; break;
            case GLFW_KEY_3: editor->m_currentTool = EditorTool::PICK; break;
            case GLFW_KEY_4: editor->m_currentTool = EditorTool::MARKER; break;
            case GLFW_KEY_G: editor->m_showGrid = !editor->m_showGrid; break;
            case GLFW_KEY_W: editor->m_showWireframe = !editor->m_showWireframe; break;
            case GLFW_KEY_X: editor->m_showAxes = !editor->m_showAxes; break;
            case GLFW_KEY_DELETE: editor->deleteSelectedBlocks(); break;
            case GLFW_KEY_F1: editor->m_showHelpWindow = !editor->m_showHelpWindow; break;
            case GLFW_KEY_HOME: {
                // Reset camera
                editor->m_camera.distance = 20.0f;
                editor->m_camera.yaw = -45.0f;
                editor->m_camera.pitch = 35.0f;
                editor->m_camera.target = glm::vec3(0.0f);
                break;
            }
            // Numpad views
            case GLFW_KEY_KP_1: // Front
                editor->m_camera.yaw = 0.0f;
                editor->m_camera.pitch = 0.0f;
                break;
            case GLFW_KEY_KP_3: // Right
                editor->m_camera.yaw = -90.0f;
                editor->m_camera.pitch = 0.0f;
                break;
            case GLFW_KEY_KP_7: // Top
                editor->m_camera.yaw = 0.0f;
                editor->m_camera.pitch = 89.9f;
                break;
            case GLFW_KEY_KP_DECIMAL: { // Focus on selection
                if (!editor->m_selectedBlocks.empty()) {
                    glm::vec3 center(0.0f);
                    for (int64_t enc : editor->m_selectedBlocks) {
                        center += glm::vec3(decodePos(enc));
                    }
                    center /= (float)editor->m_selectedBlocks.size();
                    editor->m_camera.target = center;
                }
                break;
            }
            default: break;
        }
    }

    // Ctrl + key shortcuts
    if (ctrl && !shift) {
        switch (key) {
            case GLFW_KEY_N: editor->newStructure(); break;
            case GLFW_KEY_O: editor->loadStructure(); break;
            case GLFW_KEY_S: editor->saveStructure(); break;
            case GLFW_KEY_Z: editor->undo(); break;
            case GLFW_KEY_Y: editor->redo(); break;
            case GLFW_KEY_C: editor->copySelection(); break;
            case GLFW_KEY_X: editor->cutSelection(); break;
            case GLFW_KEY_V: editor->pasteClipboard(); break;
            case GLFW_KEY_M: editor->moveSelection(); break;
            case GLFW_KEY_D: editor->duplicateSelection(); break;
            case GLFW_KEY_A: editor->selectAll(); break;
            case GLFW_KEY_I: editor->invertSelection(); break;
            case GLFW_KEY_R: editor->rotateStructure(); break;
            case GLFW_KEY_F: editor->fillSelection(editor->m_selectedBlock); break;
            case GLFW_KEY_COMMA: editor->m_showSettingsWindow = !editor->m_showSettingsWindow; break;
            // Ctrl + Numpad views (opposite directions)
            case GLFW_KEY_KP_1: // Back
                editor->m_camera.yaw = 180.0f;
                editor->m_camera.pitch = 0.0f;
                break;
            case GLFW_KEY_KP_3: // Left
                editor->m_camera.yaw = 90.0f;
                editor->m_camera.pitch = 0.0f;
                break;
            case GLFW_KEY_KP_7: // Bottom
                editor->m_camera.yaw = 0.0f;
                editor->m_camera.pitch = -89.9f;
                break;
            default: break;
        }
    }

    // Ctrl + Shift shortcuts
    if (ctrl && shift) {
        switch (key) {
            case GLFW_KEY_S: editor->saveStructureAs(); break;
            case GLFW_KEY_A: editor->deselectAll(); break;
            case GLFW_KEY_R: editor->rotateSelection(); break;
            default: break;
        }
    }
}
