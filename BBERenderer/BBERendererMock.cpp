#include "BBERenderer.h"
#ifdef CHERRYSIM_TESTER_ENABLED
#include "gtest/gtest.h"
#endif

void BBERenderer::resetCamera()
{
}

BBERenderer::BBERenderer(CherrySim* sim)
    : sim(sim)
{
}

void BBERenderer::setSim(CherrySim* sim)
{
    resetCamera();
}

void BBERenderer::onStart()
{
}

void BBERenderer::update(float timeSinceLastFrame)
{
}

void BBERenderer::draw3D(bbe::PrimitiveBrush3D& brush)
{
}

bool BBERenderer::getPosOfId(NodeId id, bbe::Vector2* pos) const
{
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
    return bbe::Vector2(0, 0);
}

bbe::Vector2 BBERenderer::screenPosToWorldPos(const bbe::Vector2& pos) const
{
    return bbe::Vector2{0, 0};
}

bbe::Vector3 BBERenderer::clusterIdToColor(ClusterId id) const
{
    return bbe::Vector3{0, 0, 0};
}

u32 BBERenderer::getClosestIndexToMouse() const
{
    u32 closestIndex = 0;
    return closestIndex;
}

bool BBERenderer::isPaused() const
{
    return paused;
}

void BBERenderer::reset()
{
}

void BBERenderer::addPacket(const NodeEntry* sender, const NodeEntry* receiver)
{
}

void BBERenderer::draw2D(bbe::PrimitiveBrush2D& brush)
{
}

void BBERenderer::onEnd()
{
}
