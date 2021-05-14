#pragma once

namespace bbe
{
    class Vector2
    {
    public:
        Vector2(float x, float y)
        {
            this->x = x;
            this->y = y;
        }
        float x, y;
    };

    class Vector3
    {
    public:
        Vector3(float x, float y, float z)
        {
            this->x = x;
            this->y = y;
            this->z = z;
        }
        float x, y, z;
    };

    class PrimitiveBrush3D
    {
    public:
        PrimitiveBrush3D();
        virtual ~PrimitiveBrush3D();
        int brush3d;
    };

    class PrimitiveBrush2D
    {
    public:
        PrimitiveBrush2D();
        virtual ~PrimitiveBrush2D();
        int brush2d;
    };
    
    template <typename T>
    class List
    {
    public:
        List(){}
    };

    class Game
    {
    public:
        Game(){}
        virtual ~Game(){}

        void setExternallyManaged(bool set){}
        void start(int x, int y, const char* title){}
        bool keepAlive(){ return true; }
        void frame(){}
        void shutdown(){}
        virtual void onStart()                            = 0;
        virtual void update(float timeSinceLastFrame)     = 0;
        virtual void draw3D(bbe::PrimitiveBrush3D &brush) = 0;
        virtual void draw2D(bbe::PrimitiveBrush2D &brush) = 0;
        virtual void onEnd()                              = 0;
    };
}
