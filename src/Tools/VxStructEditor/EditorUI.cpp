// ============================================================================
// VxStruct Editor - UI Panels
// All ImGui UI rendering: menus, toolbar, palette, properties, help, about.
// ============================================================================
#include "EditorApp.h"

// ============================================================================
// UI Layout
// ============================================================================

void VxStructEditor::renderUI() {
    renderMenuBar();

    ImGui::SetNextWindowPos(ImVec2(0, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, (float)m_windowHeight - 20), ImGuiCond_FirstUseEver);
    renderToolbar();

    ImGui::SetNextWindowPos(ImVec2(0, 380), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, (float)m_windowHeight - 380), ImGuiCond_FirstUseEver);
    renderBlockPalette();

    ImGui::SetNextWindowPos(ImVec2((float)m_windowWidth - 300, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 350), ImGuiCond_FirstUseEver);
    renderProperties();

    ImGui::SetNextWindowPos(ImVec2((float)m_windowWidth - 300, 370), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, (float)m_windowHeight - 370), ImGuiCond_FirstUseEver);
    renderStructureInfo();

    renderMarkerPanel();
    renderSelectionPanel();

    if (m_showHelpWindow) renderHelpWindow();
    if (m_showAboutWindow) renderAboutWindow();
    if (m_showSettingsWindow) renderSettingsWindow();
}

// ============================================================================
// Menu Bar
// ============================================================================

void VxStructEditor::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) newStructure();
            if (ImGui::MenuItem("Open...", "Ctrl+O")) loadStructure();
            if (ImGui::BeginMenu("Open Recent")) {
                if (m_settings.recentFiles.empty()) {
                    ImGui::MenuItem("(No recent files)", nullptr, false, false);
                } else {
                    for (size_t i = 0; i < m_settings.recentFiles.size(); i++) {
                        const auto& path = m_settings.recentFiles[i];
                        // Show just the filename for display
                        std::string display = path;
                        auto slash = display.find_last_of("/\\");
                        if (slash != std::string::npos) display = display.substr(slash + 1);
                        ImGui::PushID((int)i);
                        if (ImGui::MenuItem(display.c_str())) {
                            openRecentFile(path);
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("%s", path.c_str());
                        }
                        ImGui::PopID();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Clear Recent Files")) {
                        m_settings.recentFiles.clear();
                        m_settings.save(m_settingsFilePath);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S")) saveStructure();
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) saveStructureAs();
            ImGui::Separator();
            if (ImGui::MenuItem("Settings...", "Ctrl+,")) m_showSettingsWindow = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) glfwSetWindowShouldClose(m_window, true);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !m_undoStack.empty())) undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !m_redoStack.empty())) redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, !m_selectedBlocks.empty())) copySelection();
            if (ImGui::MenuItem("Cut", "Ctrl+X", false, !m_selectedBlocks.empty())) cutSelection();
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, !m_clipboard.empty())) pasteClipboard();
            if (ImGui::MenuItem("Move to Cursor", "Ctrl+M", false, !m_clipboard.empty() && m_hasHover)) moveSelection();
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, !m_selectedBlocks.empty())) duplicateSelection();
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "Ctrl+A")) selectAll();
            if (ImGui::MenuItem("Deselect All", "Ctrl+Shift+A")) deselectAll();
            if (ImGui::MenuItem("Invert Selection", "Ctrl+I")) invertSelection();
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Selected", "Delete", false, !m_selectedBlocks.empty())) deleteSelectedBlocks();
            if (ImGui::MenuItem("Fill Selection", "Ctrl+F", false, !m_selectedBlocks.empty())) fillSelection(m_selectedBlock);
            ImGui::Separator();
            if (ImGui::BeginMenu("Rotation Axis")) {
                bool isX = (m_rotationAxis == RotationAxis::X);
                bool isY = (m_rotationAxis == RotationAxis::Y);
                bool isZ = (m_rotationAxis == RotationAxis::Z);
                if (ImGui::MenuItem("X Axis", nullptr, isX)) m_rotationAxis = RotationAxis::X;
                if (ImGui::MenuItem("Y Axis", nullptr, isY)) m_rotationAxis = RotationAxis::Y;
                if (ImGui::MenuItem("Z Axis", nullptr, isZ)) m_rotationAxis = RotationAxis::Z;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Rotation Pivot")) {
                bool isCenter = (m_rotationPivot == RotationPivot::BOUNDING_CENTER);
                bool isOrigin = (m_rotationPivot == RotationPivot::ORIGIN);
                bool isCustom = (m_rotationPivot == RotationPivot::CUSTOM);
                if (ImGui::MenuItem("Bounding Center", nullptr, isCenter)) m_rotationPivot = RotationPivot::BOUNDING_CENTER;
                if (ImGui::MenuItem("World Origin (0,0,0)", nullptr, isOrigin)) m_rotationPivot = RotationPivot::ORIGIN;
                if (ImGui::MenuItem("Custom Pivot", nullptr, isCustom)) m_rotationPivot = RotationPivot::CUSTOM;
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Rotate Block Faces 90 CW", "R", false, (m_hasHover && m_hoverHit.hit) || !m_selectedBlocks.empty()))
            {
                if (!m_selectedBlocks.empty()) rotateSelectedBlocksFaces();
                else if (m_hasHover && m_hoverHit.hit) rotateBlockFaces(m_hoverHit.blockPos);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Rotate Structure 90", "Ctrl+R")) rotateStructure();
            if (ImGui::MenuItem("Rotate Selection 90", "Ctrl+Shift+R", false, !m_selectedBlocks.empty())) rotateSelection();
            ImGui::Separator();
            if (ImGui::BeginMenu("Move Selection", !m_selectedBlocks.empty())) {
                if (ImGui::MenuItem("Move +X (1 block)")) moveSelectionByOffset(glm::ivec3(1, 0, 0));
                if (ImGui::MenuItem("Move -X (1 block)")) moveSelectionByOffset(glm::ivec3(-1, 0, 0));
                if (ImGui::MenuItem("Move +Y (1 block)")) moveSelectionByOffset(glm::ivec3(0, 1, 0));
                if (ImGui::MenuItem("Move -Y (1 block)")) moveSelectionByOffset(glm::ivec3(0, -1, 0));
                if (ImGui::MenuItem("Move +Z (1 block)")) moveSelectionByOffset(glm::ivec3(0, 0, 1));
                if (ImGui::MenuItem("Move -Z (1 block)")) moveSelectionByOffset(glm::ivec3(0, 0, -1));
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear All Blocks")) {
                EditorAction action;
                action.type = EditorAction::Type::REMOVE_MULTIPLE;
                action.previousBlocks = m_structure.getBlocks();
                pushAction(action);
                m_structure.clear();
                m_selectedBlocks.clear();
                m_modified = true;
                rebuildBlockMesh();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::Checkbox("Show Grid", &m_showGrid);
            ImGui::Checkbox("Show Wireframe", &m_showWireframe);
            ImGui::Checkbox("Show Axes", &m_showAxes);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Camera", "Home")) {
                m_camera = OrbitCamera();
            }
            if (ImGui::MenuItem("Focus Selection", "Numpad .")) {
                if (!m_structure.getBlocks().empty()) {
                    glm::vec3 center = glm::vec3(m_structure.getMinBounds() + m_structure.getMaxBounds()) * 0.5f;
                    m_camera.target = center;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Front View", "Numpad 1")) { m_camera.yaw = 0; m_camera.pitch = 0; }
            if (ImGui::MenuItem("Back View", "Ctrl+Numpad 1")) { m_camera.yaw = 180; m_camera.pitch = 0; }
            if (ImGui::MenuItem("Right View", "Numpad 3")) { m_camera.yaw = 90; m_camera.pitch = 0; }
            if (ImGui::MenuItem("Left View", "Ctrl+Numpad 3")) { m_camera.yaw = -90; m_camera.pitch = 0; }
            if (ImGui::MenuItem("Top View", "Numpad 7")) { m_camera.yaw = 0; m_camera.pitch = 89; }
            if (ImGui::MenuItem("Bottom View", "Ctrl+Numpad 7")) { m_camera.yaw = 0; m_camera.pitch = -89; }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Keyboard Shortcuts", "F1")) m_showHelpWindow = true;
            ImGui::Separator();
            if (ImGui::MenuItem("About VxStruct Editor")) m_showAboutWindow = true;
            ImGui::EndMenu();
        }

        float statusX = ImGui::GetWindowWidth() - 450;
        ImGui::SameLine(statusX);
        ImGui::TextDisabled("Blocks: %d | Selection: %d | Undo: %d",
                           (int)m_structure.getBlocks().size(),
                           (int)m_selectedBlocks.size(),
                           (int)m_undoStack.size());

        ImGui::EndMainMenuBar();
    }
}

// ============================================================================
// Toolbar
// ============================================================================

void VxStructEditor::renderToolbar() {
    ImGui::Begin("Tools");

    ImGui::Text("Current Tool:");
    ImGui::Separator();

    bool isPlace  = (m_currentTool == EditorTool::PLACE);
    bool isErase  = (m_currentTool == EditorTool::ERASE);
    bool isPick   = (m_currentTool == EditorTool::PICK);
    bool isMarker = (m_currentTool == EditorTool::MARKER);
    bool isSelect = (m_currentTool == EditorTool::SELECT);

    auto toolButton = [](const char* label, bool active, ImVec4 activeColor) -> bool {
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
        bool pressed = ImGui::Button(label, ImVec2(130, 28));
        if (active) ImGui::PopStyleColor();
        return pressed;
    };

    if (toolButton("Select [Q]", isSelect, ImVec4(0.1f, 0.5f, 0.8f, 1.0f))) m_currentTool = EditorTool::SELECT;
    if (toolButton("Place  [1]", isPlace,  ImVec4(0.2f, 0.6f, 0.3f, 1.0f))) m_currentTool = EditorTool::PLACE;
    if (toolButton("Erase  [2]", isErase,  ImVec4(0.7f, 0.2f, 0.2f, 1.0f))) m_currentTool = EditorTool::ERASE;
    if (toolButton("Pick   [3]", isPick,   ImVec4(0.7f, 0.7f, 0.2f, 1.0f))) m_currentTool = EditorTool::PICK;
    if (toolButton("Marker [4]", isMarker, ImVec4(0.2f, 0.5f, 0.7f, 1.0f))) m_currentTool = EditorTool::MARKER;

    ImGui::Separator();

    if (!m_selectedBlocks.empty()) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Selected: %d blocks", (int)m_selectedBlocks.size());
        if (ImGui::Button("Deselect All", ImVec2(130, 22))) deselectAll();
        if (ImGui::Button("Delete Selected", ImVec2(130, 22))) deleteSelectedBlocks();
        if (ImGui::Button("Fill Selected", ImVec2(130, 22))) fillSelection(m_selectedBlock);
        ImGui::Separator();
    }

    ImGui::Text("Layer Filter:");
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputInt("Y Layer", &m_currentLayer)) {
        if (m_currentLayer < -1) m_currentLayer = -1;
        rebuildBlockMesh();
    }
    ImGui::SameLine();
    if (ImGui::Button("All##layers")) {
        m_currentLayer = -1;
        rebuildBlockMesh();
    }

    ImGui::Separator();

    ImGui::Text("History:");
    ImGui::Text("  Undo: %d  Redo: %d", (int)m_undoStack.size(), (int)m_redoStack.size());
    if (ImGui::Button("Undo##btn", ImVec2(62, 22)) && !m_undoStack.empty()) undo();
    ImGui::SameLine();
    if (ImGui::Button("Redo##btn", ImVec2(62, 22)) && !m_redoStack.empty()) redo();

    ImGui::Separator();
    ImGui::Text("Camera:");
    ImGui::Text("  Distance: %.1f", m_camera.distance);
    ImGui::Text("  Yaw: %.1f  Pitch: %.1f", m_camera.yaw, m_camera.pitch);

    ImGui::End();
}

// ============================================================================
// Block Palette
// ============================================================================

void VxStructEditor::renderBlockPalette() {
    ImGui::Begin("Block Palette");

    static char filterBuf[64] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", "Search blocks...", filterBuf, sizeof(filterBuf));
    std::string filter = filterBuf;

    static const char* categories[] = {"All", "Natural", "Wood", "Nature", "Building", "Ore", "Mineral", "Wool", "Functional", "Road", "Liquid"};

    if (ImGui::BeginTabBar("Categories")) {
        for (const char* cat : categories) {
            if (ImGui::BeginTabItem(cat)) {
                m_selectedCategory = cat;
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    const auto& palette = getBlockPalette();
    int buttonsPerRow = std::max(1, (int)(ImGui::GetContentRegionAvail().x / 38.0f));
    int col = 0;

    for (const auto& info : palette) {
        if (m_selectedCategory != "All" && info.category != m_selectedCategory) continue;

        if (!filter.empty()) {
            std::string name = info.name;
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            std::string f = filter;
            std::transform(f.begin(), f.end(), f.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            if (name.find(f) == std::string::npos) continue;
        }

        bool isSelected = (m_selectedBlock == info.type);

        ImGui::PushID(static_cast<int>(info.type));

        bool useTexture = (m_atlasTexture != 0 && m_settings.showTexturesInPalette);
        // Note: Palette always uses atlas thumbnails; 3D blocks use atlas or PBR per settings

        if (isSelected) {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        }

        bool clicked = false;
        if (useTexture) {
            int atlasIdx = getBlockTextureIndex(info.type, 1); // side face
            float cs = 1.0f / 16.0f;
            int atlasCol = atlasIdx % 16;
            int atlasRow = atlasIdx / 16;
            // Atlas loaded with stbi flip=true: V=0 is image bottom, V=1 is image top
            // ImGui uv0=top-left on screen, uv1=bottom-right on screen
            ImVec2 uv0(atlasCol * cs, 1.0f - atlasRow * cs);
            ImVec2 uv1((atlasCol + 1) * cs, 1.0f - (atlasRow + 1) * cs);

            // Use tinted background to show selection state
            ImGui::PushStyleColor(ImGuiCol_Button, isSelected ? ImVec4(0.3f, 0.5f, 0.8f, 1.0f) : ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.4f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.3f, 1.0f));

            // Draw button with texture
            ImVec2 btnSize(32, 32);
            clicked = ImGui::Button("##block", btnSize);

            // Draw the texture overlay on the button
            ImVec2 rectMin = ImGui::GetItemRectMin();
            ImVec2 rectMax = ImGui::GetItemRectMax();
            // Inset by 2 pixels for a small border
            ImVec2 imgMin(rectMin.x + 2, rectMin.y + 2);
            ImVec2 imgMax(rectMax.x - 2, rectMax.y - 2);
            ImGui::GetWindowDrawList()->AddImage(
                (ImTextureID)(intptr_t)m_atlasTexture,
                imgMin, imgMax, uv0, uv1
            );

            ImGui::PopStyleColor(3);
        } else {
            ImVec4 buttonColor(info.color.r, info.color.g, info.color.b, 1.0f);
            ImVec4 hoverColor(
                std::min(1.0f, info.color.r + 0.2f),
                std::min(1.0f, info.color.g + 0.2f),
                std::min(1.0f, info.color.b + 0.2f),
                1.0f
            );
            ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonColor);

            clicked = ImGui::Button("##block", ImVec2(32, 32));

            ImGui::PopStyleColor(3);
        }

        if (clicked) {
            m_selectedBlock = info.type;
            m_currentTool = EditorTool::PLACE;
        }

        if (isSelected) {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", info.name);
        }

        ImGui::PopID();

        col++;
        if (col < buttonsPerRow) ImGui::SameLine();
        else col = 0;
    }

    ImGui::Separator();
    ImGui::Text("Selected: %s", getBlockName(m_selectedBlock));

    ImGui::End();
}

// ============================================================================
// Properties Panel
// ============================================================================

void VxStructEditor::renderProperties() {
    ImGui::Begin("Structure Properties");

    ImGui::Text("Name:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##name", m_nameBuffer, sizeof(m_nameBuffer))) {
        m_structure.setName(m_nameBuffer);
        m_modified = true;
    }

    ImGui::Text("Author:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##author", m_authorBuffer, sizeof(m_authorBuffer))) {
        m_structure.setAuthor(m_authorBuffer);
        m_modified = true;
    }

    ImGui::Text("Category:");
    ImGui::SetNextItemWidth(-1);
    static const char* catNames[] = {
        "Village House", "Village Building", "Village Farm", "Village Well",
        "Village Path", "Village Decoration", "City Building", "City Skyscraper",
        "City Road", "City Park", "City Decoration",
        "Tree Oak", "Tree Birch", "Tree Spruce", "Tree Spruce Snowy", "Tree Jungle",
        "Misc"
    };
    if (ImGui::Combo("##category", &m_categoryIndex, catNames, IM_ARRAYSIZE(catNames))) {
        m_structure.setCategory(static_cast<StructureCategory>(m_categoryIndex));
        m_modified = true;
    }

    ImGui::Separator();

    ImGui::Checkbox("Requires Flat Terrain", &m_requiresFlat);
    m_structure.setRequiresFlat(m_requiresFlat);

    ImGui::Text("Min Ground Coverage:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##coverage", &m_minGroundCoverage, 0.0f, 1.0f, "%.2f")) {
        m_structure.setMinGroundCoverage(m_minGroundCoverage);
        m_modified = true;
    }

    ImGui::Separator();
    ImGui::Text("Tags:");

    const auto& tags = m_structure.getTags();
    for (size_t i = 0; i < tags.size(); i++) {
        ImGui::PushID((int)i);
        ImGui::BulletText("%s", tags[i].c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            m_modified = true;
        }
        ImGui::PopID();
    }

    ImGui::SetNextItemWidth(-60);
    ImGui::InputText("##newtag", m_tagBuffer, sizeof(m_tagBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Add") && m_tagBuffer[0] != '\0') {
        m_structure.addTag(m_tagBuffer);
        m_tagBuffer[0] = '\0';
        m_modified = true;
    }

    ImGui::End();
}

// ============================================================================
// Structure Info Panel
// ============================================================================

void VxStructEditor::renderStructureInfo() {
    ImGui::Begin("Structure Info");

    glm::ivec3 size = m_structure.getSize();
    glm::ivec3 minB = m_structure.getMinBounds();
    glm::ivec3 maxB = m_structure.getMaxBounds();

    ImGui::Text("Blocks: %d", (int)m_structure.getBlocks().size());
    ImGui::Text("Markers: %d", (int)m_structure.getMarkers().size());
    ImGui::Separator();
    ImGui::Text("Size: %d x %d x %d", size.x, size.y, size.z);
    ImGui::Text("Bounds: (%d,%d,%d) -> (%d,%d,%d)",
                minB.x, minB.y, minB.z, maxB.x, maxB.y, maxB.z);

    ImGui::Separator();
    if (m_hasHover) {
        if (m_hoverHit.hit) {
            BlockType bt = m_structure.getBlock(m_hoverHit.blockPos);
            ImGui::Text("Hover Block: (%d, %d, %d)",
                        m_hoverHit.blockPos.x, m_hoverHit.blockPos.y, m_hoverHit.blockPos.z);
            ImGui::Text("Block Type: %s", getBlockName(bt));
            uint8_t meta = m_structure.getBlockMetadata(m_hoverHit.blockPos);
            int faceRot = getBlockFaceRotation(meta);
            const char* rotLabels[] = {"0", "90", "180", "270"};
            ImGui::Text("Face Rotation: %s deg", rotLabels[faceRot & 3]);
        }
        ImGui::Text("Place Pos: (%d, %d, %d)",
                    m_hoverPlacePos.x, m_hoverPlacePos.y, m_hoverPlacePos.z);
    } else {
        ImGui::TextDisabled("No hover target");
    }

    ImGui::Separator();
    ImGui::Text("File: %s", m_currentFilePath.empty() ? "(unsaved)" : m_currentFilePath.c_str());
    if (m_modified) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(modified)");
    }

    ImGui::Separator();
    ImGui::TextWrapped("Controls:");
    ImGui::BulletText("Left Click: Place/Erase/Pick");
    ImGui::BulletText("Middle Mouse: Orbit camera");
    ImGui::BulletText("Shift+Middle: Pan camera");
    ImGui::BulletText("Scroll: Zoom");
    ImGui::BulletText("Q/1-4: Select tool");

    ImGui::End();
}

// ============================================================================
// Marker Panel
// ============================================================================

void VxStructEditor::renderMarkerPanel() {
    if (m_currentTool != EditorTool::MARKER) return;

    ImGui::Begin("Marker Settings");

    static const char* markerTypes[] = {"door", "spawn", "chest", "bed", "villager", "custom"};
    static int markerTypeIdx = 0;

    ImGui::Text("Marker Type:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##markertype", &markerTypeIdx, markerTypes, IM_ARRAYSIZE(markerTypes))) {
        m_markerType = markerTypes[markerTypeIdx];
    }

    ImGui::Text("Marker Data (optional):");
    static char dataBuf[256] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##markerdata", dataBuf, sizeof(dataBuf));
    m_markerData = dataBuf;

    ImGui::Separator();
    ImGui::Text("Existing Markers:");
    const auto& markers = m_structure.getMarkers();
    for (size_t i = 0; i < markers.size(); i++) {
        ImGui::PushID((int)i);
        ImGui::Text("(%d,%d,%d) [%s]",
                    markers[i].position.x, markers[i].position.y, markers[i].position.z,
                    markers[i].type.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Del")) {
            m_structure.removeMarker(markers[i].position);
            m_modified = true;
        }
        ImGui::PopID();
    }

    ImGui::End();
}

// ============================================================================
// Selection Panel
// ============================================================================

void VxStructEditor::renderSelectionPanel() {
    if (m_currentTool != EditorTool::SELECT && m_selectedBlocks.empty()) return;

    ImGui::Begin("Selection", nullptr);

    ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "%d blocks selected", (int)m_selectedBlocks.size());
    ImGui::Separator();

    if (ImGui::Button("Select All (Ctrl+A)", ImVec2(-1, 0))) selectAll();
    if (ImGui::Button("Deselect (Ctrl+Shift+A)", ImVec2(-1, 0))) deselectAll();
    if (ImGui::Button("Invert (Ctrl+I)", ImVec2(-1, 0))) invertSelection();

    ImGui::Separator();
    ImGui::Text("Operations:");

    bool hasSelection = !m_selectedBlocks.empty();
    if (!hasSelection) ImGui::BeginDisabled();
    if (ImGui::Button("Copy (Ctrl+C)", ImVec2(-1, 0))) copySelection();
    if (ImGui::Button("Cut (Ctrl+X)", ImVec2(-1, 0))) cutSelection();
    if (ImGui::Button("Duplicate (Ctrl+D)", ImVec2(-1, 0))) duplicateSelection();
    if (ImGui::Button("Delete (Del)", ImVec2(-1, 0))) deleteSelectedBlocks();

    // ---- Face Rotation Section (in-place texture rotation) ----
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.8f, 1.0f), "Face Rotation [R]:");
    ImGui::TextWrapped("Rotates block textures in-place (which face points where).");
    if (ImGui::Button("Rotate Faces 90 CW (R)", ImVec2(-1, 0))) rotateSelectedBlocksFaces();

    // ---- Spatial Rotation Section ----
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "Spatial Rotation (90 deg):");
    ImGui::TextWrapped("Moves blocks to new positions around a pivot.");

    ImGui::Text("Axis:");
    int rotAxisInt = static_cast<int>(m_rotationAxis);
    ImGui::RadioButton("X##rotaxis", &rotAxisInt, 0); ImGui::SameLine();
    ImGui::RadioButton("Y##rotaxis", &rotAxisInt, 1); ImGui::SameLine();
    ImGui::RadioButton("Z##rotaxis", &rotAxisInt, 2);
    m_rotationAxis = static_cast<RotationAxis>(rotAxisInt);

    ImGui::Text("Pivot:");
    int pivotInt = static_cast<int>(m_rotationPivot);
    ImGui::RadioButton("Center##pivot", &pivotInt, 0); ImGui::SameLine();
    ImGui::RadioButton("Origin##pivot", &pivotInt, 1); ImGui::SameLine();
    ImGui::RadioButton("Custom##pivot", &pivotInt, 2);
    m_rotationPivot = static_cast<RotationPivot>(pivotInt);

    if (m_rotationPivot == RotationPivot::CUSTOM) {
        ImGui::SetNextItemWidth(-1);
        ImGui::DragInt3("##custompivot", &m_customPivot.x, 1.0f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Custom rotation pivot (X, Y, Z)");
    }

    if (ImGui::Button("Rotate Selection (Ctrl+Shift+R)", ImVec2(-1, 0))) rotateSelection();
    if (ImGui::Button("Rotate Entire Structure (Ctrl+R)", ImVec2(-1, 0))) rotateStructure();

    // ---- Block-Based Move Section ----
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.5f, 1.0f), "Move Selection (block-based):");

    // Cumulative offset display (Blender-style)
    if (m_hasMoveOrigin && m_cumulativeMoveOffset != glm::ivec3(0)) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Total Offset: (%d, %d, %d)",
            m_cumulativeMoveOffset.x, m_cumulativeMoveOffset.y, m_cumulativeMoveOffset.z);
        if (ImGui::SmallButton("Reset Tracking")) {
            m_hasMoveOrigin = false;
            m_cumulativeMoveOffset = glm::ivec3(0);
        }
    }

    // Draggable X/Y/Z offsets (Blender-style DragInt)
    int dragX = 0, dragY = 0, dragZ = 0;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.4f, 0.1f, 0.1f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.6f, 0.15f, 0.15f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.8f, 0.2f, 0.2f, 0.9f));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragInt("##dragMoveX", &dragX, 0.3f, 0, 0, "X: %d (drag)")) {
        if (dragX != 0) moveSelectionByOffset(glm::ivec3(dragX, 0, 0));
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) dragX = 0;
    ImGui::PopStyleColor(3);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.35f, 0.1f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.15f, 0.5f, 0.15f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.2f, 0.7f, 0.2f, 0.9f));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragInt("##dragMoveY", &dragY, 0.3f, 0, 0, "Y: %d (drag)")) {
        if (dragY != 0) moveSelectionByOffset(glm::ivec3(0, dragY, 0));
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) dragY = 0;
    ImGui::PopStyleColor(3);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.15f, 0.4f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.15f, 0.2f, 0.6f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.2f, 0.3f, 0.8f, 0.9f));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragInt("##dragMoveZ", &dragZ, 0.3f, 0, 0, "Z: %d (drag)")) {
        if (dragZ != 0) moveSelectionByOffset(glm::ivec3(0, 0, dragZ));
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) dragZ = 0;
    ImGui::PopStyleColor(3);

    ImGui::Spacing();

    // +/- buttons for precise 1-block movement
    ImGui::Text("Step Move:");
    if (ImGui::Button("-X", ImVec2(38, 22))) moveSelectionByOffset(glm::ivec3(-1, 0, 0));
    ImGui::SameLine();
    if (ImGui::Button("+X", ImVec2(38, 22))) moveSelectionByOffset(glm::ivec3(1, 0, 0));
    ImGui::SameLine();
    if (ImGui::Button("-Y", ImVec2(38, 22))) moveSelectionByOffset(glm::ivec3(0, -1, 0));
    ImGui::SameLine();
    if (ImGui::Button("+Y", ImVec2(38, 22))) moveSelectionByOffset(glm::ivec3(0, 1, 0));
    ImGui::SameLine();
    if (ImGui::Button("-Z", ImVec2(38, 22))) moveSelectionByOffset(glm::ivec3(0, 0, -1));
    ImGui::SameLine();
    if (ImGui::Button("+Z", ImVec2(38, 22))) moveSelectionByOffset(glm::ivec3(0, 0, 1));

    // Custom offset input + apply
    ImGui::Text("Custom Offset:");
    ImGui::SetNextItemWidth(-1);
    int customOff[3] = { m_moveOffsetX, m_moveOffsetY, m_moveOffsetZ };
    if (ImGui::InputInt3("##customOffset", customOff)) {
        m_moveOffsetX = customOff[0];
        m_moveOffsetY = customOff[1];
        m_moveOffsetZ = customOff[2];
    }
    if (ImGui::Button("Apply Offset", ImVec2(-1, 0))) {
        glm::ivec3 offset(m_moveOffsetX, m_moveOffsetY, m_moveOffsetZ);
        moveSelectionByOffset(offset);
        m_moveOffsetX = m_moveOffsetY = m_moveOffsetZ = 0;
    }

    ImGui::Separator();
    ImGui::Text("Fill with current block:");
    if (ImGui::Button("Fill Selection (Ctrl+F)", ImVec2(-1, 0))) fillSelection(m_selectedBlock);
    if (!hasSelection) ImGui::EndDisabled();

    bool hasClipboard = !m_clipboard.empty();
    ImGui::Separator();
    if (!hasClipboard) ImGui::BeginDisabled();
    ImGui::Text("Clipboard: %d blocks", (int)m_clipboard.size());
    if (ImGui::Button("Paste (Ctrl+V)", ImVec2(-1, 0))) pasteClipboard();
    if (ImGui::Button("Move to Cursor (Ctrl+M)", ImVec2(-1, 0))) moveSelection();

    ImGui::Separator();
    ImGui::Text("Move Axis Constraint:");
    int axisInt = static_cast<int>(m_moveAxis);
    ImGui::RadioButton("Free##axis", &axisInt, 0); ImGui::SameLine();
    ImGui::RadioButton("X##axis", &axisInt, 1); ImGui::SameLine();
    ImGui::RadioButton("Y##axis", &axisInt, 2); ImGui::SameLine();
    ImGui::RadioButton("Z##axis", &axisInt, 3);
    m_moveAxis = static_cast<MoveAxis>(axisInt);

    if (!hasClipboard) ImGui::EndDisabled();

    ImGui::End();
}

// ============================================================================
// Help Window
// ============================================================================

void VxStructEditor::renderHelpWindow() {
    if (!m_showHelpWindow) return;

    ImGui::SetNextWindowSize(ImVec2(520, 620), ImGuiCond_FirstUseEver);
    ImGui::Begin("Keyboard Shortcuts & Help", &m_showHelpWindow);

    ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "VxStruct Editor - Keyboard Reference");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Navigation (Blender-style)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Middle Mouse Drag    - Orbit camera");
        ImGui::BulletText("Shift + Middle Mouse - Pan camera");
        ImGui::BulletText("Ctrl + Middle Mouse  - Zoom camera");
        ImGui::BulletText("Scroll Wheel         - Zoom in/out");
        ImGui::BulletText("Numpad 1             - Front view");
        ImGui::BulletText("Numpad 3             - Right view");
        ImGui::BulletText("Numpad 7             - Top view");
        ImGui::BulletText("Ctrl+Numpad 1        - Back view");
        ImGui::BulletText("Ctrl+Numpad 3        - Left view");
        ImGui::BulletText("Ctrl+Numpad 7        - Bottom view");
        ImGui::BulletText("Numpad .             - Focus on selection");
        ImGui::BulletText("Home                 - Reset camera");
    }

    if (ImGui::CollapsingHeader("Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Q - Select tool");
        ImGui::BulletText("1 - Place block");
        ImGui::BulletText("2 - Erase block");
        ImGui::BulletText("3 - Pick block (eyedropper)");
        ImGui::BulletText("4 - Place marker");
        ImGui::BulletText("G - Toggle grid");
        ImGui::BulletText("W - Toggle wireframe");
        ImGui::BulletText("X - Toggle axes");
    }

    if (ImGui::CollapsingHeader("Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Left Click (Select tool)   - Select block (sets anchor)");
        ImGui::BulletText("Shift + Left Click         - Range select (anchor to cursor)");
        ImGui::BulletText("Ctrl + A                   - Select all");
        ImGui::BulletText("Ctrl + Shift + A           - Deselect all");
        ImGui::BulletText("Ctrl + I                   - Invert selection");
    }

    if (ImGui::CollapsingHeader("Edit Operations", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Ctrl + Z     - Undo");
        ImGui::BulletText("Ctrl + Y     - Redo");
        ImGui::BulletText("Ctrl + C     - Copy selection");
        ImGui::BulletText("Ctrl + X     - Cut selection");
        ImGui::BulletText("Ctrl + V     - Paste clipboard");
        ImGui::BulletText("Ctrl + M     - Move clipboard to cursor");
        ImGui::BulletText("  (Set axis: Free/X/Y/Z in Selection panel)");
        ImGui::BulletText("Ctrl + D     - Duplicate selection");
        ImGui::BulletText("R            - Rotate block faces 90 deg CW");
        ImGui::BulletText("  (In-place texture rotation, hovered or selected)");
        ImGui::BulletText("Ctrl + R     - Rotate structure 90 deg (spatial)");
        ImGui::BulletText("Ctrl+Shift+R - Rotate selection 90 deg (spatial)");
        ImGui::BulletText("  (Set rotation axis: X/Y/Z in Selection panel)");
        ImGui::BulletText("Ctrl + F     - Fill selection with current block");
        ImGui::BulletText("Delete       - Delete selected blocks");
        ImGui::BulletText("Arrow Keys   - Move selection +/- X/Z (1 block)");
        ImGui::BulletText("PgUp/PgDn    - Move selection +/- Y (1 block)");
    }

    if (ImGui::CollapsingHeader("File", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Ctrl + N         - New structure");
        ImGui::BulletText("Ctrl + O         - Open file");
        ImGui::BulletText("Ctrl + S         - Save");
        ImGui::BulletText("Ctrl + Shift + S - Save As");
        ImGui::BulletText("Ctrl + ,         - Settings");
    }

    if (ImGui::CollapsingHeader("Mouse")) {
        ImGui::BulletText("Left Click       - Use current tool");
        ImGui::BulletText("Right Click      - Quick erase block");
        ImGui::BulletText("Middle Click     - Orbit (hold & drag)");
    }

    ImGui::Separator();
    ImGui::TextWrapped(
        "Tip: Click a block with the Select tool to set an anchor, then "
        "Shift+Click another block to select all blocks in the 3D box between them."
    );

    ImGui::End();
}

// ============================================================================
// About Window
// ============================================================================

void VxStructEditor::renderAboutWindow() {
    if (!m_showAboutWindow) return;

    ImGui::SetNextWindowSize(ImVec2(380, 260), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("About VxStruct Editor", &m_showAboutWindow)) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "VxStruct Editor");
        ImGui::Text("Version 1.0.0");
        ImGui::Separator();
        ImGui::TextWrapped(
            "A standalone 3D voxel structure editor for creating and editing "
            ".vxstruct files used by the Vortex Engine Minecraft-like game."
        );
        ImGui::Separator();
        ImGui::Text("Features:");
        ImGui::BulletText("Blender-style 3D navigation");
        ImGui::BulletText("Multi-block selection & editing");
        ImGui::BulletText("Undo/Redo with 200 history steps");
        ImGui::BulletText("Copy/Paste/Duplicate blocks");
        ImGui::BulletText("Structure rotation (90 deg increments)");
        ImGui::BulletText("Block palette with 100+ block types");
        ImGui::BulletText("Marker placement for doors & spawns");
        ImGui::Separator();
        ImGui::Text("Built with: OpenGL 4.5, GLFW, ImGui, GLM");
        ImGui::Text("Format: VxStruct (Vortex Structs) JSON");
    }
    ImGui::End();
}

// ============================================================================
// Settings Window
// ============================================================================

void VxStructEditor::renderSettingsWindow() {
    if (!m_showSettingsWindow) return;

    ImGui::SetNextWindowSize(ImVec2(420, 380), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", &m_showSettingsWindow)) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Editor Settings");
        ImGui::Separator();

        // --- Texture Mode ---
        if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Show Textures in Block Palette", &m_settings.showTexturesInPalette);
            ImGui::Separator();
            ImGui::Text("Texture Source:");
            bool pbr = m_settings.usePBRTextures;
            if (ImGui::RadioButton("Block Atlas (16x16 grid)", !pbr)) {
                m_settings.usePBRTextures = false;
                updateTextureMode();
            }
            if (ImGui::RadioButton("PBR Textures (individual files)", pbr)) {
                m_settings.usePBRTextures = true;
                updateTextureMode();
            }
            if (m_settings.usePBRTextures) {
                if (m_pbrAlbedoArray) {
                    ImGui::TextDisabled("  PBR textures loaded (%d textures, %dx%d)",
                        (int)m_pbrTextureMap.size(), m_pbrTexSize, m_pbrTexSize);
                } else {
                    ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "  PBR textures not found!");
                    ImGui::TextDisabled("  Expected at assets/pbr/textures/block/");
                }
            } else {
                ImGui::TextDisabled("  Atlas loaded from assets/block_atlas.png");
            }
        }

        // --- Auto-save ---
        if (ImGui::CollapsingHeader("Auto-Save", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Enable Auto-Save", &m_settings.autoSaveEnabled);

            if (!m_settings.autoSaveEnabled) ImGui::BeginDisabled();

            ImGui::Text("Auto-save interval:");
            ImGui::SetNextItemWidth(200);
            ImGui::SliderFloat("##autosave_sec", &m_settings.autoSaveIntervalSec, 10.0f, 600.0f, "%.0f seconds");

            // Quick preset buttons
            if (ImGui::Button("30s")) m_settings.autoSaveIntervalSec = 30.0f;
            ImGui::SameLine();
            if (ImGui::Button("1 min")) m_settings.autoSaveIntervalSec = 60.0f;
            ImGui::SameLine();
            if (ImGui::Button("2 min")) m_settings.autoSaveIntervalSec = 120.0f;
            ImGui::SameLine();
            if (ImGui::Button("5 min")) m_settings.autoSaveIntervalSec = 300.0f;

            if (!m_settings.autoSaveEnabled) ImGui::EndDisabled();
        }

        // --- Display ---
        if (ImGui::CollapsingHeader("Display")) {
            ImGui::Checkbox("Show Grid on Start", &m_showGrid);
            ImGui::Checkbox("Show Wireframe on Start", &m_showWireframe);
            ImGui::Checkbox("Show Axes on Start", &m_showAxes);
            ImGui::SliderInt("Grid Size", &m_gridSize, 8, 128);
        }

        ImGui::Separator();
        if (ImGui::Button("Save Settings", ImVec2(-1, 0))) {
            m_settings.save(m_settingsFilePath);
        }
    }
    ImGui::End();
}
