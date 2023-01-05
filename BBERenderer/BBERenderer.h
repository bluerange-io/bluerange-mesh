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

    bbe::Image backgroundImage;

    void resetCamera();

    bool showConnections = true;
    bool showNodes       = true;
    bool showPackets = true;
    int zPos             = 0;

    int draggedNodeIndex = -1;

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

    //Returns the node index that has the closest position to the mouse cursor
    //Will return -1 if no node was found
    //This will pay attention to the currently selected z position and will only
    //mark displayed nodes
    i32 getClosestIndexToMouse() const;

    //Similar to the above method but will only return nodes that are directly
    //below the mouse cursor
    i32 getNodeIndexUnderMouse() const;

    bool isPaused() const;
    void reset();

    void addPacket(const NodeEntry* sender, const NodeEntry* receiver);

    bool checkNodeVisible(const NodeEntry* node) const;
    float getAlpha(const NodeEntry* node, bool veryFaint = false) const;
};
