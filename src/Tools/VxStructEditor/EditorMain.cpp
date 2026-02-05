// ============================================================================
// VxStruct Editor - Entry Point & Core Loop
// Part of the Vortex Engine Tooling
//
// A standalone 3D voxel editor for creating and editing .vxstruct files.
// Uses the same block types and structure format as the game engine.
// ============================================================================
#include "EditorApp.h"

// ============================================================================
// Initialization
// ============================================================================

bool VxStructEditor::initialize() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, "VxStruct Editor - Vortex Engine Tools", nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // VSync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    // Set callbacks
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);
    glfwSetScrollCallback(m_window, scrollCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
    glfwSetKeyCallback(m_window, keyCallback);

    // OpenGL settings
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.18f, 0.20f, 0.25f, 1.0f);

    // Load shaders
    m_blockShader = createShaderProgram("shaders/editor_block.vert", "shaders/editor_block.frag");
    m_gridShader = createShaderProgram("shaders/editor_grid.vert", "shaders/editor_grid.frag");
    m_wireShader = createShaderProgram("shaders/editor_wireframe.vert", "shaders/editor_wireframe.frag");

    if (!m_blockShader || !m_gridShader || !m_wireShader) {
        std::cerr << "Failed to compile shaders" << std::endl;
        return false;
    }

    // Create VAOs/VBOs
    glGenVertexArrays(1, &m_blockVAO);
    glGenBuffers(1, &m_blockVBO);

    glGenVertexArrays(1, &m_gridVAO);
    glGenBuffers(1, &m_gridVBO);

    glGenVertexArrays(1, &m_wireVAO);
    glGenBuffers(1, &m_wireVBO);

    // Initialize wireframe cube VBO (unit cube edges)
    {
        float cubeEdges[] = {
            // Bottom face
            0,0,0, 1,0,0,  1,0,0, 1,0,1,  1,0,1, 0,0,1,  0,0,1, 0,0,0,
            // Top face
            0,1,0, 1,1,0,  1,1,0, 1,1,1,  1,1,1, 0,1,1,  0,1,1, 0,1,0,
            // Vertical edges
            0,0,0, 0,1,0,  1,0,0, 1,1,0,  1,0,1, 1,1,1,  0,0,1, 0,1,1,
        };

        glBindVertexArray(m_wireVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_wireVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cubeEdges), cubeEdges, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = "vxstruct_editor_imgui.ini";

    // Dark theme with editor accent colors
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;

    // Custom colors - Vortex theme
    auto& colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.35f, 0.55f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.40f, 0.60f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.50f, 0.70f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.35f, 0.55f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.45f, 0.65f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.30f, 0.50f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.35f, 0.55f, 0.80f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.45f, 0.65f, 0.80f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.17f, 0.21f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.11f, 0.14f, 1.00f);

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 450");

    // Initialize structure
    newStructure();
    rebuildGridMesh();

    std::cout << "VxStruct Editor initialized successfully" << std::endl;
    return true;
}

// ============================================================================
// Main Loop
// ============================================================================

void VxStructEditor::run() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        processInput();
        updateHover();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render
        render();
        renderUI();

        // Finalize ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_window);
    }
}

void VxStructEditor::cleanup() {
    if (m_blockVAO) glDeleteVertexArrays(1, &m_blockVAO);
    if (m_blockVBO) glDeleteBuffers(1, &m_blockVBO);
    if (m_gridVAO) glDeleteVertexArrays(1, &m_gridVAO);
    if (m_gridVBO) glDeleteBuffers(1, &m_gridVBO);
    if (m_wireVAO) glDeleteVertexArrays(1, &m_wireVAO);
    if (m_wireVBO) glDeleteBuffers(1, &m_wireVBO);
    if (m_blockShader) glDeleteProgram(m_blockShader);
    if (m_gridShader) glDeleteProgram(m_gridShader);
    if (m_wireShader) glDeleteProgram(m_wireShader);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

// ============================================================================
// Input Processing
// ============================================================================

void VxStructEditor::processInput() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse || io.WantCaptureKeyboard) return;

    double mx, my;
    glfwGetCursorPos(m_window, &mx, &my);

    double dx = mx - m_lastMouseX;
    double dy = my - m_lastMouseY;

    // Middle mouse: orbit camera (Blender-style)
    if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
        bool shiftHeld = glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                         glfwGetKey(m_window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        bool ctrlHeld = glfwGetKey(m_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                        glfwGetKey(m_window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

        if (!m_isDragging && !m_isPanning) {
            if (shiftHeld) {
                m_isPanning = true;
            } else if (ctrlHeld) {
                // Ctrl+MMB = zoom (Blender-style)
                m_isDragging = false;
                m_isPanning = false;
            } else {
                m_isDragging = true;
            }
        }

        if (ctrlHeld && !m_isDragging && !m_isPanning) {
            // Ctrl+MMB drag: zoom
            m_camera.distance -= (float)dy * m_camera.distance * 0.005f;
            m_camera.distance = glm::clamp(m_camera.distance, 2.0f, 200.0f);
        } else if (m_isDragging) {
            // Orbit
            m_camera.yaw -= (float)dx * 0.3f;
            m_camera.pitch += (float)dy * 0.3f;
            m_camera.pitch = glm::clamp(m_camera.pitch, -89.0f, 89.0f);
        } else if (m_isPanning) {
            // Pan (Shift+MMB)
            glm::mat4 view = m_camera.getViewMatrix();
            glm::vec3 right = glm::vec3(view[0][0], view[1][0], view[2][0]);
            glm::vec3 up = glm::vec3(view[0][1], view[1][1], view[2][1]);
            float panSpeed = m_camera.distance * 0.003f;
            m_camera.target -= right * (float)dx * panSpeed;
            m_camera.target += up * (float)dy * panSpeed;
        }
    } else {
        m_isDragging = false;
        m_isPanning = false;
    }

    m_lastMouseX = mx;
    m_lastMouseY = my;
}

// ============================================================================
// Hover / Raycasting
// ============================================================================

void VxStructEditor::updateHover() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        m_hasHover = false;
        return;
    }

    double mx, my;
    glfwGetCursorPos(m_window, &mx, &my);

    int fbW, fbH;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);
    if (fbW == 0 || fbH == 0) return;

    float aspect = (float)fbW / (float)fbH;
    glm::mat4 proj = m_camera.getProjectionMatrix(aspect);
    glm::mat4 view = m_camera.getViewMatrix();

    Ray ray = screenToRay(mx, my, fbW, fbH, proj, view);

    // Test against all blocks
    m_hasHover = false;
    m_hoverHit.hit = false;
    m_hoverHit.distance = 1e30f;

    const auto& blocks = m_structure.getBlocks();
    for (const auto& block : blocks) {
        // Layer filter
        if (m_currentLayer >= 0 && block.position.y != m_currentLayer) continue;

        float t;
        glm::ivec3 n;
        if (rayAABB(ray, block.position, t, n) && t < m_hoverHit.distance) {
            m_hoverHit.hit = true;
            m_hoverHit.blockPos = block.position;
            m_hoverHit.normal = n;
            m_hoverHit.distance = t;
        }
    }

    if (m_hoverHit.hit) {
        m_hasHover = true;
        m_hoverPlacePos = m_hoverHit.blockPos + m_hoverHit.normal;
    } else {
        // Intersect with ground plane (y=0)
        if (std::abs(ray.direction.y) > 1e-6f) {
            float t = -ray.origin.y / ray.direction.y;
            if (t > 0) {
                glm::vec3 hitPoint = ray.origin + ray.direction * t;
                m_hoverPlacePos = glm::ivec3(
                    (int)std::floor(hitPoint.x),
                    0,
                    (int)std::floor(hitPoint.z)
                );
                // Only hover within grid bounds
                if (m_hoverPlacePos.x >= -m_gridSize/2 && m_hoverPlacePos.x < m_gridSize/2 &&
                    m_hoverPlacePos.z >= -m_gridSize/2 && m_hoverPlacePos.z < m_gridSize/2) {
                    m_hasHover = true;
                    m_hoverHit.hit = false; // No block hit, ground plane hit
                }
            }
        }
    }
}

// ============================================================================
// Mesh Building
// ============================================================================

void VxStructEditor::rebuildBlockMesh() {
    std::vector<Vertex> vertices;
    const auto& blocks = m_structure.getBlocks();

    for (const auto& block : blocks) {
        // Layer filter
        if (m_currentLayer >= 0 && block.position.y != m_currentLayer) continue;

        glm::vec3 color = getBlockColor(block.type);
        generateCubeVertices(vertices, glm::vec3(block.position), color);
    }

    m_blockVertexCount = (int)vertices.size();

    glBindVertexArray(m_blockVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_blockVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(glm::vec3)));
    glEnableVertexAttribArray(1);
    // Color
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(glm::vec3)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void VxStructEditor::rebuildGridMesh() {
    struct GridVertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    std::vector<GridVertex> vertices;
    float halfGrid = m_gridSize / 2.0f;

    glm::vec3 gridColor(0.30f, 0.32f, 0.35f);
    glm::vec3 axisX(0.70f, 0.20f, 0.20f);
    glm::vec3 axisZ(0.20f, 0.20f, 0.70f);

    // Grid lines at y=0
    for (int i = -(int)halfGrid; i <= (int)halfGrid; i++) {
        glm::vec3 color = (i == 0) ? axisZ : gridColor;
        vertices.push_back({{(float)i, 0, -halfGrid}, color});
        vertices.push_back({{(float)i, 0,  halfGrid}, color});

        color = (i == 0) ? axisX : gridColor;
        vertices.push_back({{-halfGrid, 0, (float)i}, color});
        vertices.push_back({{ halfGrid, 0, (float)i}, color});
    }

    // Y axis indicator
    if (m_showAxes) {
        glm::vec3 axisY(0.20f, 0.70f, 0.20f);
        vertices.push_back({{0, 0, 0}, axisY});
        vertices.push_back({{0, 20, 0}, axisY});
    }

    m_gridVertexCount = (int)vertices.size();

    glBindVertexArray(m_gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_gridVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GridVertex), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GridVertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GridVertex), (void*)(sizeof(glm::vec3)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

// ============================================================================
// Entry Point
// ============================================================================

int main(int /*argc*/, char** /*argv*/) {
    VxStructEditor editor;

    if (!editor.initialize()) {
        std::cerr << "Failed to initialize VxStruct Editor" << std::endl;
        return -1;
    }

    editor.run();
    return 0;
}
