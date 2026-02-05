// ============================================================================
// VxStruct Editor - Rendering
// 3D viewport rendering: blocks, grid, wireframes, hover, selection highlights.
// ============================================================================
#include "EditorApp.h"

void VxStructEditor::render() {
    int fbW, fbH;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (fbW == 0 || fbH == 0) return;

    float aspect = (float)fbW / (float)fbH;
    glm::mat4 proj = m_camera.getProjectionMatrix(aspect);
    glm::mat4 view = m_camera.getViewMatrix();
    glm::mat4 model = glm::mat4(1.0f);

    // Draw grid
    if (m_showGrid) {
        glUseProgram(m_gridShader);
        glUniformMatrix4fv(glGetUniformLocation(m_gridShader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(m_gridShader, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glBindVertexArray(m_gridVAO);
        glDrawArrays(GL_LINES, 0, m_gridVertexCount);
        glBindVertexArray(0);
    }

    // Draw blocks
    if (m_blockVertexCount > 0) {
        glUseProgram(m_blockShader);
        glUniformMatrix4fv(glGetUniformLocation(m_blockShader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(m_blockShader, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(m_blockShader, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3f(glGetUniformLocation(m_blockShader, "uLightDir"), 0.5f, 0.8f, 0.3f);
        glm::vec3 camPos = m_camera.getPosition();
        glUniform3f(glGetUniformLocation(m_blockShader, "uViewPos"), camPos.x, camPos.y, camPos.z);
        glUniform1f(glGetUniformLocation(m_blockShader, "uHighlight"), 0.0f);
        glBindVertexArray(m_blockVAO);
        glDrawArrays(GL_TRIANGLES, 0, m_blockVertexCount);
        glBindVertexArray(0);
    }

    // Draw wireframe overlay on blocks
    if (m_showWireframe && m_blockVertexCount > 0) {
        glUseProgram(m_wireShader);
        glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 0.0f, 0.0f, 0.0f, 0.3f);

        glBindVertexArray(m_wireVAO);
        glLineWidth(1.0f);

        const auto& blocks = m_structure.getBlocks();
        for (const auto& block : blocks) {
            if (m_currentLayer >= 0 && block.position.y != m_currentLayer) continue;
            glm::mat4 blockModel = glm::translate(glm::mat4(1.0f), glm::vec3(block.position));
            glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uModel"), 1, GL_FALSE, glm::value_ptr(blockModel));
            glDrawArrays(GL_LINES, 0, 24);
        }
        glBindVertexArray(0);
    }

    // Draw hover indicator
    if (m_hasHover) {
        glUseProgram(m_wireShader);
        glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uView"), 1, GL_FALSE, glm::value_ptr(view));

        if (m_currentTool == EditorTool::PLACE) {
            glm::mat4 hoverModel = glm::translate(glm::mat4(1.0f), glm::vec3(m_hoverPlacePos));
            glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uModel"), 1, GL_FALSE, glm::value_ptr(hoverModel));
            glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 0.2f, 1.0f, 0.3f, 0.9f);
            glLineWidth(2.5f);
            glBindVertexArray(m_wireVAO);
            glDrawArrays(GL_LINES, 0, 24);
            glBindVertexArray(0);
        } else if (m_currentTool == EditorTool::ERASE && m_hoverHit.hit) {
            glm::mat4 eraseModel = glm::translate(glm::mat4(1.0f), glm::vec3(m_hoverHit.blockPos));
            glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uModel"), 1, GL_FALSE, glm::value_ptr(eraseModel));
            glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 1.0f, 0.2f, 0.2f, 0.9f);
            glLineWidth(2.5f);
            glBindVertexArray(m_wireVAO);
            glDrawArrays(GL_LINES, 0, 24);
            glBindVertexArray(0);
        } else if (m_currentTool == EditorTool::PICK && m_hoverHit.hit) {
            glm::mat4 pickModel = glm::translate(glm::mat4(1.0f), glm::vec3(m_hoverHit.blockPos));
            glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uModel"), 1, GL_FALSE, glm::value_ptr(pickModel));
            glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 1.0f, 0.9f, 0.2f, 0.9f);
            glLineWidth(2.5f);
            glBindVertexArray(m_wireVAO);
            glDrawArrays(GL_LINES, 0, 24);
            glBindVertexArray(0);
        } else if (m_currentTool == EditorTool::SELECT && m_hoverHit.hit) {
            glm::mat4 selModel = glm::translate(glm::mat4(1.0f), glm::vec3(m_hoverHit.blockPos));
            glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uModel"), 1, GL_FALSE, glm::value_ptr(selModel));
            glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 0.0f, 0.8f, 1.0f, 0.9f);
            glLineWidth(2.5f);
            glBindVertexArray(m_wireVAO);
            glDrawArrays(GL_LINES, 0, 24);
            glBindVertexArray(0);
        }
        glLineWidth(1.0f);
    }

    // Draw selection highlights
    renderSelectionHighlights();

    // Draw marker positions
    const auto& markers = m_structure.getMarkers();
    if (!markers.empty()) {
        glUseProgram(m_wireShader);
        glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glLineWidth(3.0f);

        for (const auto& marker : markers) {
            glm::mat4 mModel = glm::translate(glm::mat4(1.0f), glm::vec3(marker.position));
            mModel = glm::scale(mModel, glm::vec3(1.05f));
            mModel = glm::translate(mModel, glm::vec3(-0.025f));
            glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uModel"), 1, GL_FALSE, glm::value_ptr(mModel));

            if (marker.type == "door")
                glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 0.0f, 1.0f, 1.0f, 0.9f);
            else if (marker.type == "spawn")
                glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 1.0f, 0.5f, 0.0f, 0.9f);
            else
                glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 1.0f, 1.0f, 0.0f, 0.9f);

            glBindVertexArray(m_wireVAO);
            glDrawArrays(GL_LINES, 0, 24);
            glBindVertexArray(0);
        }
        glLineWidth(1.0f);
    }
}

void VxStructEditor::renderSelectionHighlights() {
    if (m_selectedBlocks.empty()) return;

    int fbW, fbH;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);
    if (fbW == 0 || fbH == 0) return;

    float aspect = (float)fbW / (float)fbH;
    glm::mat4 proj = m_camera.getProjectionMatrix(aspect);
    glm::mat4 view = m_camera.getViewMatrix();

    glUseProgram(m_wireShader);
    glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniform4f(glGetUniformLocation(m_wireShader, "uColor"), 0.0f, 0.9f, 1.0f, 0.95f);
    glLineWidth(2.5f);

    glBindVertexArray(m_wireVAO);
    for (int64_t enc : m_selectedBlocks) {
        glm::ivec3 pos = decodePos(enc);
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(pos));
        m = glm::scale(m, glm::vec3(1.02f));
        m = glm::translate(m, glm::vec3(-0.01f));
        glUniformMatrix4fv(glGetUniformLocation(m_wireShader, "uModel"), 1, GL_FALSE, glm::value_ptr(m));
        glDrawArrays(GL_LINES, 0, 24);
    }
    glBindVertexArray(0);
    glLineWidth(1.0f);
}
