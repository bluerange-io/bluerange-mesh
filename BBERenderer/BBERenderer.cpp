#include "BBERenderer.h"
#ifdef CHERRYSIM_TESTER_ENABLED
#include "gtest/gtest.h"
#endif

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
}

void BBERenderer::update(float timeSinceLastFrame)
{
    const bbe::Vector2 mouseToWorld = screenPosToWorldPos(getMouse());
    if (getMouseScrollY() < 0) { zoomLevel *= 0.9f; }
    if (getMouseScrollY() > 0) { zoomLevel /= 0.9f; }
    const bbe::Vector2 mouseToWorldAfterTransform = screenPosToWorldPos(getMouse());
    const bbe::Vector2 error = worldPosToScreenPos(mouseToWorld - mouseToWorldAfterTransform) - renderOffset;
    renderOffset -= error;

    if (isMouseDown(bbe::MouseButton::LEFT))
    {
        renderOffset += getMouseDelta();
    }

    if (isKeyDown(bbe::Key::SPACE))
    {
        resetCamera();
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
        if (id == sim->nodes[i].id)
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

u32 BBERenderer::getClosestIndexToMouse() const
{
    const bbe::Vector2 mouse = getMouse();

    u32 closestIndex = 0;
    float closestDistance = mouse.getDistanceTo(getPosOfIndex(0));

    for (u32 i = 1; i < sim->GetTotalNodes(); i++)
    {
        const float dist = mouse.getDistanceTo(getPosOfIndex(i));
        if (dist < closestDistance)
        {
            closestDistance = dist;
            closestIndex = i;
        }
    }

    return closestIndex;
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
    if (showConnections)
    {
        //Draw all GAP connections that exist in a red color
        brush.setColorRGB(1, 0, 0);
        for (u32 i = 0; i < sim->GetTotalNodes(); i++)
        {
            for (u32 k = 0; k < SIM_MAX_CONNECTION_NUM; k++)
            {
                if (sim->nodes[i].state.connections[k].connectionActive)
                {
                    brush.fillLine(getPosOfNodeEntry(sim->nodes[i]), getPosOfIndex(sim->nodes[i].state.connections[k].partner->index));
                }
            }
        }

        //Redraw all FruityMesh connections in green
        brush.setColorRGB(0, 1, 0);
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
                        if (conns.handles[k].GetConnectionState() == ConnectionState::HANDSHAKE_DONE) brush.setColorRGB(0, 1, 0);
                        else  brush.setColorRGB(0, 0, 1);

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

    const u32 closestMouseIndex = getClosestIndexToMouse();
    if (showNodes)
    {
        //Highlight the active node
        brush.setColorRGB(1, 1, 1);
        brush.fillCircle(getPosOfIndex(closestMouseIndex) - bbe::Vector2{ 7, 7 }, 14, 14);

        for (u32 i = 0; i < sim->GetTotalNodes(); i++)
        {
            //Draw the LED state
            if (sim->nodes[i].led1On || sim->nodes[i].led2On || sim->nodes[i].led3On) {
                float led1Value = sim->nodes[i].led1On ? 1.0f : 0.0f;
                float led2Value = sim->nodes[i].led2On ? 1.0f : 0.0f;
                float led3Value = sim->nodes[i].led3On ? 1.0f : 0.0f;

                brush.setColorRGB(led1Value, led2Value, led3Value);
                brush.fillCircle(getPosOfIndex(i) - bbe::Vector2{ 8, 8 }, 16, 16);
            }

            //Draw each node with the color of its cluster
            brush.setColorRGB(clusterIdToColor(sim->nodes[i].gs.node.clusterId));
            brush.fillCircle(getPosOfIndex(i) - bbe::Vector2{ 5, 5 }, 10, 10);
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
    ImGui::Text("Cl. Done:    %s", sim->IsClusteringDone() ? "True" : "False");
    ImGui::Text("Map Width:   %d", sim->simConfig.mapWidthInMeters);
    ImGui::Text("Map Height:  %d", sim->simConfig.mapHeightInMeters);
    ImGui::Text("Time:        %.3fs", sim->simState.simTimeMs / 1000.f);
    ImGui::End();

    ImGui::SetNextWindowSize({(float)getScaledWindowWidth() / 5.f, (float)getScaledWindowHeight() / 2});
    ImGui::SetNextWindowPos({ (float)getScaledWindowWidth() * 4.f / 5.f, (float)getScaledWindowHeight() / 2 });
    ImGui::Begin("Mouse Node Stats");
    {
        NodeIndexSetter setter(closestMouseIndex);
        ImGui::Text("Serial:            %s", GS->config.GetSerialNumber());
        ImGui::Text("Featureset:        %s", FEATURESET_NAME);
        ImGui::Text("Node ID:           %d", GS->node.configuration.nodeId);
        ImGui::Text("Network ID:        %d", GS->node.configuration.networkId);
        ImGui::Text("Cluster ID:        %u", GS->node.clusterId);
        ImGui::Text("Cluster size:      %d", GS->node.GetClusterSize());
        ImGui::Text("Free In:           %d", GS->cm.freeMeshInConnections);
        ImGui::Text("Free Out:          %d", GS->cm.freeMeshOutConnections);
    }
    ImGui::End();

    ImGui::SetNextWindowSize({ (float)getScaledWindowWidth() * 4.f / 5.f, (float)getScaledWindowHeight() / 13.f });
    ImGui::SetNextWindowPos({ 0, (float)getScaledWindowHeight() * 12.f / 13.f});
    ImGui::Begin("Control");
    if (ImGui::Button("Reset Camera"))
    {
        resetCamera();
    }
    ImGui::SameLine(0, 100);
    ImGui::Checkbox("Paused", &paused);
    ImGui::SameLine(0, 100);
    ImGui::Checkbox("Show Connections", &showConnections);
    ImGui::SameLine(0, 100);
    ImGui::Checkbox("Show Nodes", &showNodes);
    ImGui::SameLine(0, 100);
    ImGui::Checkbox("Show Packets", &showPackets);
    ImGui::End();
}

void BBERenderer::onEnd()
{
}
