#pragma once

#include "BBE/BrotboxEngine.h"
#include "CherrySim.h"

class RenderPacket
{
public:
    bbe::Vector2 start  { 0, 0 };
    bbe::Vector2 stop   { 0, 0 };
    bbe::Vector2 control{ 0, 0 };
    float t = 0;
};

class BBERenderer : public bbe::Game
{
private:
    CherrySim* sim = nullptr;
    float zoomLevel = 10;
    bbe::Vector2 renderOffset = bbe::Vector2(100, 100);
    bool paused = false;
    bbe::List<RenderPacket> renderPackets;

    void resetCamera();

    bool showConnections = true;
    bool showNodes       = true;
    bool showPackets     = true;

public:
    explicit BBERenderer(CherrySim* sim);
    void setSim(CherrySim* sim);

    virtual void onStart() override;
    virtual void update(float timeSinceLastFrame) override;
    virtual void draw3D(bbe::PrimitiveBrush3D& brush) override;
    virtual void draw2D(bbe::PrimitiveBrush2D& brush) override;
    virtual void onEnd() override;

    bool getPosOfId(NodeId id, bbe::Vector2 *pos) const;
    bbe::Vector2 getPosOfNodeEntry(const NodeEntry& entry) const;
    bbe::Vector2 getPosOfIndex(u32 index) const;
    bbe::Vector2 worldPosToScreenPos(const bbe::Vector2& pos) const;
    bbe::Vector2 screenPosToWorldPos(const bbe::Vector2& pos) const;

    bbe::Vector3 clusterIdToColor(ClusterId id) const;

    u32 getClosestIndexToMouse() const;

    bool isPaused() const;
    void reset();

    void addPacket(const NodeEntry* sender, const NodeEntry* receiver);
};
