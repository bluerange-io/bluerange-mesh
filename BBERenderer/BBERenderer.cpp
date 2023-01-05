#include "BBERenderer.h"
#ifdef CHERRYSIM_TESTER_ENABLED
#include "gtest/gtest.h"
#endif
#include <cmath>

constexpr u32 NODE_DIAMETER = 10;

void BBERenderer::resetCamera()
{
    zoomLevel = 10;
    renderOffset = bbe::Vector2(100, 100);
}

BBERenderer::BBERenderer(CherrySim* sim)
    : sim(sim)
{
}

void BBERenderer::setSim(CherrySim* sim)
{
    this->sim = sim;
}

void BBERenderer::onStart()
{
    resetCamera();

    //Create an image from the given floorplan if available
    if (!sim->simConfig.floorplanImage.empty()) {
        std::ifstream imageFile(sim->simConfig.floorplanImage);
        if (!imageFile) {
            printf("Could not load floorplan image %s" EOL, sim->simConfig.floorplanImage.c_str());
            SIMEXCEPTIONFORCE(IllegalArgumentException);
        }

        backgroundImage = bbe::Image(sim->simConfig.floorplanImage.c_str());
    }

}

void BBERenderer::update(float timeSinceLastFrame)
{
    const bbe::Vector2 mouseToWorld = screenPosToWorldPos(getMouse());
    if (getMouseScrollY() < 0) { zoomLevel *= 0.9f; }
    if (getMouseScrollY() > 0) { zoomLevel /= 0.9f; }
    const bbe::Vector2 mouseToWorldAfterTransform = screenPosToWorldPos(getMouse());
    const bbe::Vector2 error = worldPosToScreenPos(mouseToWorld - mouseToWorldAfterTransform) - renderOffset;
    renderOffset -= error;

    //Stop dragging a node once the mouse button is up
    if (isMouseUp(bbe::MouseButton::LEFT)) {
        draggedNodeIndex = -1;
    }

    if (isKeyDown(bbe::Key::LEFT_CONTROL)) {
        //Move nodes around
        if (isMouseDown(bbe::MouseButton::LEFT))
        {
            //Check if we should start dragging a node
            if (draggedNodeIndex < 0) {
                draggedNodeIndex = getNodeIndexUnderMouse();
            }
            //Drag the node that was selected for dragging
            if (draggedNodeIndex >= 0) {
                const bbe::Vector2 delta = getMouseDelta();
                {
                    //Use Add Position so that the node can realize it was moved (e.g. accelerometer simulation)
                    sim->AddPosition(
                        draggedNodeIndex,
                        delta.x / zoomLevel / sim->simConfig.mapWidthInMeters,
                        delta.y / zoomLevel / sim->simConfig.mapHeightInMeters,
                        0
                    );
                }
            }
        }
    }
    else {
        //Move the camera with the mouse
        if (isMouseDown(bbe::MouseButton::LEFT))
        {
            renderOffset += getMouseDelta();
        }

        //Reset the camera to 0,0
        if (isKeyDown(bbe::Key::SPACE))
        {
            resetCamera();
        }
    }

    if (!paused)
    {
        for (size_t i = 0; i < renderPackets.getLength(); i++)
        {
            renderPackets[i].t += timeSinceLastFrame * 2;
            if (renderPackets[i].t > 1)
            {
                renderPackets.removeIndex(i);
                i--;
            }
        }
    }
}

void BBERenderer::draw3D(bbe::PrimitiveBrush3D& brush)
{
}

bool BBERenderer::getPosOfId(NodeId id, bbe::Vector2* pos) const
{
    for (u32 i = 0; i < sim->GetTotalNodes(); i++)
    {
        if (id == sim->nodes[i].GetNodeId())
        {
            *pos = getPosOfNodeEntry(sim->nodes[i]);
            return true;
        }
    }
    return false;
}

bbe::Vector2 BBERenderer::getPosOfNodeEntry(const NodeEntry& entry) const
{
    return worldPosToScreenPos({ entry.x, entry.y });
}

bbe::Vector2 BBERenderer::getPosOfIndex(u32 index) const
{
    return getPosOfNodeEntry(sim->nodes[index]);
}

bbe::Vector2 BBERenderer::worldPosToScreenPos(const bbe::Vector2& pos) const
{
    return bbe::Vector2(pos.x * sim->simConfig.mapWidthInMeters, pos.y * sim->simConfig.mapHeightInMeters) * zoomLevel + renderOffset;
}

bbe::Vector2 BBERenderer::screenPosToWorldPos(const bbe::Vector2& pos) const
{
    const bbe::Vector2 partial = ((pos - renderOffset) / zoomLevel);
    return bbe::Vector2{partial.x / sim->simConfig.mapWidthInMeters, partial.y / sim->simConfig.mapHeightInMeters};
}

bbe::Vector3 BBERenderer::clusterIdToColor(ClusterId id) const
{
    const float hue = (id % 360) * 17.5f;
    const float s = 0.5f + ((id % 100) / 100.f) / 2.0f;
    return bbe::Color::HSVtoRGB(hue, s, 1);
}

i32 BBERenderer::getClosestIndexToMouse() const
{
    const bbe::Vector2 mouse = getMouse();

    i32 closestIndex = -1;
    float closestDistance = FLT_MAX;

    for (u32 i = 0; i < sim->GetTotalNodes(); i++)
    {
        if (!checkNodeVisible(&sim->nodes[i])) continue;

        const float dist = mouse.getDistanceTo(getPosOfIndex(i));
        if (dist < closestDistance)
        {
            closestDistance = dist;
            closestIndex = i;
        }
    }

    return closestIndex;
}

i32 BBERenderer::getNodeIndexUnderMouse() const
{
    const bbe::Vector2 mouse = getMouse();

    for (u32 i = 0; i < sim->GetTotalNodes(); i++)
    {
        if (!checkNodeVisible(&sim->nodes[i])) continue;

        const float dist = mouse.getDistanceTo(getPosOfIndex(i));
        if (dist < NODE_DIAMETER)
        {
            return i;
        }
    }

    return -1;
}

bool BBERenderer::isPaused() const
{
    return paused;
}

void BBERenderer::reset()
{
    renderPackets.clear();
}

void BBERenderer::addPacket(const NodeEntry* sender, const NodeEntry* receiver)
{
    if (!checkNodeVisible(sender)) return;

    const bbe::Vector2 start = bbe::Vector2{ sender  ->x, sender  ->y };
    const bbe::Vector2 stop  = bbe::Vector2{ receiver->x, receiver->y };
    const bbe::Vector2 diff = start - stop;
    const bbe::Vector2 rotatedNorm = diff.rotate90Clockwise().normalize();
    renderPackets.add({
        start,
        stop,
        (start + stop) / 2 + rotatedNorm * 0.01,
        0
    });
}

void BBERenderer::draw2D(bbe::PrimitiveBrush2D& brush)
{
    bbe::Vector2 rectStart = worldPosToScreenPos({ 0, 0 });
    bbe::Vector2 rectDimensions = bbe::Vector2({ sim->simConfig.mapWidthInMeters * zoomLevel, sim->simConfig.mapHeightInMeters * zoomLevel });

    if (backgroundImage.isLoaded()) {
        brush.drawImage(
            rectStart,
            rectDimensions,
            backgroundImage
        );
    }

    //Draw the map dimensions
    brush.setColorRGB(0.2f, 0.2f, 0.2f, 0.4f);
    brush.fillRect(rectStart, rectDimensions);

    if (showConnections)
    {
        //Draw all GAP connections that exist in a red color
        for (u32 i = 0; i < sim->GetTotalNodes(); i++)
        {
            brush.setColorRGB(1, 0, 0, getAlpha(&sim->nodes[i], true));

            for (u32 k = 0; k < SIM_MAX_CONNECTION_NUM; k++)
            {
                if (sim->nodes[i].state.connections[k].connectionActive)
                {
                    brush.fillLine(getPosOfNodeEntry(sim->nodes[i]), getPosOfIndex(sim->nodes[i].state.connections[k].partner->index));
                }
            }
        }

        //Draw all FruityMesh connections
        for (u32 i = 0; i < sim->GetTotalNodes(); i++)
        {
            NodeIndexSetter setter(i);
            BaseConnections conns = GS->cm.GetBaseConnections(ConnectionDirection::INVALID);
            for (u32 k = 0; k < conns.count; k++)
            {
                if (conns.handles[k].GetPartnerId())
                {
                    bbe::Vector2 partnerPos;
                    if (getPosOfId(conns.handles[k].GetPartnerId(), &partnerPos))
                    {
                        //Draw Handshaked connections in green and all other connection states in blue
                        if (conns.handles[k].GetConnectionState() == ConnectionState::HANDSHAKE_DONE) brush.setColorRGB(0, 1, 0, getAlpha(&sim->nodes[i], true));
                        else  brush.setColorRGB(0, 0, 1, getAlpha(&sim->nodes[i], true));

                        brush.fillLine(getPosOfIndex(i), partnerPos);
                    }
                }
            }
        }
    }

    if (showPackets)
    {
        brush.setColorRGB(1, 1, 1);
        for (size_t i = 0; i < renderPackets.getLength(); i++)
        {
            const bbe::Vector2 interp = bbe::Math::interpolateBezier(renderPackets[i].start, renderPackets[i].stop, renderPackets[i].t, renderPackets[i].control);
            brush.fillRect(worldPosToScreenPos(interp) - bbe::Vector2(1, 1), 3, 3);
        }
    }

    const i32 closestMouseIndex = getClosestIndexToMouse();
    if (showNodes)
    {
        //Highlight the active node
        if (closestMouseIndex >= 0) {
            brush.setColorRGB(1, 1, 1);
            brush.fillCircle(getPosOfIndex(closestMouseIndex) - bbe::Vector2{ 7, 7 }, 14, 14);
        }

        for (u32 i = 0; i < sim->GetTotalNodes(); i++)
        {
            //Draw the LED state
            if (sim->nodes[i].led1On || sim->nodes[i].led2On || sim->nodes[i].led3On) {
                float led1Value = sim->nodes[i].led1On ? 1.0f : 0.0f;
                float led2Value = sim->nodes[i].led2On ? 1.0f : 0.0f;
                float led3Value = sim->nodes[i].led3On ? 1.0f : 0.0f;

                brush.setColorRGB(led1Value, led2Value, led3Value, getAlpha(&sim->nodes[i]));
                brush.fillCircle(getPosOfIndex(i) - bbe::Vector2{ 8, 8 }, 16, 16);
            }

            //Hardcoded value to set the drawing mode for the nodes
            u8 drawMode = 0;

            //Draw each node with a color of its cluster
            if (drawMode == 0) {
                brush.setColorRGB(clusterIdToColor(sim->nodes[i].gs.node.clusterId), getAlpha(&sim->nodes[i]));
            }
            //Draw each node with the color of its cluster
            else if(drawMode == 1) 
            {
                NodeIndexSetter setter(i);

                if (sim->nodes[i].gs.timeManager.IsTimeCorrected())
                {
                    brush.setColorRGB(0, 255, 0);
                }
                else if (sim->nodes[i].gs.timeManager.IsTimeSynced())
                {
                    brush.setColorRGB(0, 255, 255);
                }
                else {
                    brush.setColorRGB(255, 0, 0);
                }
                char timestr[20];
                sprintf(timestr, "%s%u,%d", GS->timeManager.IsTimeMaster() ? "M " : "", GS->timeManager.GetUtcTime(), GS->timeManager.GetOffset());

                ImGui::GetForegroundDrawList()->AddText(ImVec2(getPosOfIndex(i).x, getPosOfIndex(i).y), 0xFFFFFFFF, timestr);
            }
            else
            {
                SIMEXCEPTION(IllegalArgumentException);
            }

            brush.fillCircle(getPosOfIndex(i) - bbe::Vector2{ 5, 5 }, NODE_DIAMETER, NODE_DIAMETER);
        }
    }


    ImGui::SetNextWindowSize({ (float)getScaledWindowWidth() / 5.f, (float)getScaledWindowHeight() / 2 });
    ImGui::SetNextWindowPos({ (float)getScaledWindowWidth() * 4.f / 5.f, 0 });
    ImGui::Begin("Sim Stats");
    ImGui::Text("Num Nodes:   %d", sim->GetTotalNodes());
    ImGui::Text("Asset Nodes: %d", sim->GetAssetNodes());
#ifdef CHERRYSIM_TESTER_ENABLED
    const char* testName = "NULL";
    const ::testing::TestInfo* testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
    if (testInfo) testName = testInfo->name();
    ImGui::Text("Test Name:   %s", testName);
#endif
    ImGui::Text("Cl. Done:      %s", sim->IsClusteringDone() ? "True" : "False");
    ImGui::Text("Map Width:     %d m", sim->simConfig.mapWidthInMeters);
    ImGui::Text("Map Height:    %d m", sim->simConfig.mapHeightInMeters);
    ImGui::Text("Map Elevation: %d m", sim->simConfig.mapElevationInMeters);
    ImGui::Text("Time:          %.3f s", sim->simState.simTimeMs / 1000.f);
    ImGui::End();

    ImGui::SetNextWindowSize({(float)getScaledWindowWidth() / 5.f, (float)getScaledWindowHeight() / 2});
    ImGui::SetNextWindowPos({ (float)getScaledWindowWidth() * 4.f / 5.f, (float)getScaledWindowHeight() / 2 });
    ImGui::Begin("Mouse Node Stats");
    {
        if (closestMouseIndex >= 0) {
            NodeIndexSetter setter(closestMouseIndex);
            ImGui::Text("Serial:            %s", GS->config.GetSerialNumber());
            ImGui::Text("Featureset:        %s", FEATURESET_NAME);
            ImGui::Text("Node ID:           %d", GS->node.configuration.nodeId);
            ImGui::Text("Network ID:        %d", GS->node.configuration.networkId);
            ImGui::Text("Cluster ID:        %u", GS->node.clusterId);
            ImGui::Text("Cluster size:      %d", GS->node.GetClusterSize());
            ImGui::Text("Free In:           %d", GS->cm.freeMeshInConnections);
            ImGui::Text("Free Out:          %d", GS->cm.freeMeshOutConnections);
            ImGui::Text("PosX/Y:            %.01f m, %.01f m", cherrySimInstance->currentNode->GetXinMeters(), cherrySimInstance->currentNode->GetYinMeters());
            ImGui::Text("PosZ:              %.01f m", cherrySimInstance->currentNode->GetZinMeters());
        }
        else {
            ImGui::Text("No node selected");
        }
    }
    ImGui::End();

    ImGui::SetNextWindowSize({ (float)getScaledWindowWidth() * 4.f / 5.f, (float)getScaledWindowHeight() / 13.f });
    ImGui::SetNextWindowPos({ 0, (float)getScaledWindowHeight() * 12.f / 13.f});
    ImGui::Begin("Control");
    if (ImGui::Button("Reset View"))
    {
        resetCamera();
    }
    ImGui::SameLine(0, 20);
    ImGui::Checkbox("Paused", &paused);
    ImGui::SameLine(0, 20);
    ImGui::Checkbox("Show Connections", &showConnections);
    ImGui::SameLine(0, 20);
    ImGui::Checkbox("Show Nodes", &showNodes);
    ImGui::SameLine(0, 20);
    ImGui::Checkbox("Show Packets", &showPackets);
    ImGui::SameLine(0, 20);
    ImGui::Checkbox("FastForward", &sim->renderLess);
    ImGui::SameLine(0, 20);
    ImGui::SetNextItemWidth(100);
    ImGui::SliderInt("Z-pos", &zPos, 0, sim->simConfig.mapElevationInMeters, "%d m");
    ImGui::End();

}

void BBERenderer::onEnd()
{
}

bool BBERenderer::checkNodeVisible(const NodeEntry* node) const
{
    //A node is visible if it is within +/-5 m of the currently selected Zpos
    return std::abs(zPos - node->GetZinMeters()) < 5.0f;
}

float BBERenderer::getAlpha(const NodeEntry* node, bool veryFaint) const
{
    //Visible nodes have 100% alpha
    if (checkNodeVisible(node)) return 1.0f;
    //Nodes with more than 50 meter distance are shown faded
    else if (std::abs(zPos - node->GetZinMeters()) < 50.0f) {
        if (veryFaint) return 0.01f;
        else return 0.1;
    }
    //Everything that is more far is not displayed at all
    else return 0.0;
}