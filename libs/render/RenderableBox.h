#pragma once

#include "isurfacerenderer.h"
#include "render/RenderableGeometry.h"
#include "render/RenderableSurface.h"

namespace render
{

namespace detail
{

inline std::vector<RenderVertex> getFillBoxVertices(const Vector3& min, const Vector3& max, const Vector4& colour)
{
    // Load the 6 times 4 = 24 corner points, each with the correct face normal
    return
    {
        // Bottom quad
        RenderVertex(Vector3{ min[0], max[1], min[2] }, {0,0,-1}, {0,1}, colour),
        RenderVertex(Vector3{ max[0], max[1], min[2] }, {0,0,-1}, {1,1}, colour),
        RenderVertex(Vector3{ max[0], min[1], min[2] }, {0,0,-1}, {1,0}, colour),
        RenderVertex(Vector3{ min[0], min[1], min[2] }, {0,0,-1}, {0,0}, colour),

        // Top quad
        RenderVertex(Vector3{ min[0], min[1], max[2] }, {0,0,+1}, {0,1}, colour),
        RenderVertex(Vector3{ max[0], min[1], max[2] }, {0,0,+1}, {1,1}, colour),
        RenderVertex(Vector3{ max[0], max[1], max[2] }, {0,0,+1}, {1,0}, colour),
        RenderVertex(Vector3{ min[0], max[1], max[2] }, {0,0,+1}, {0,0}, colour),

        // Front quad
        RenderVertex(Vector3{ min[0], min[1], min[2] }, {0,-1,0}, {0,1}, colour),
        RenderVertex(Vector3{ max[0], min[1], min[2] }, {0,-1,0}, {1,1}, colour),
        RenderVertex(Vector3{ max[0], min[1], max[2] }, {0,-1,0}, {1,0}, colour),
        RenderVertex(Vector3{ min[0], min[1], max[2] }, {0,-1,0}, {0,0}, colour),

        // Back quad
        RenderVertex(Vector3{ min[0], max[1], min[2] }, {0,+1,0}, {1,1}, colour),
        RenderVertex(Vector3{ min[0], max[1], max[2] }, {0,+1,0}, {1,0}, colour),
        RenderVertex(Vector3{ max[0], max[1], max[2] }, {0,+1,0}, {0,0}, colour),
        RenderVertex(Vector3{ max[0], max[1], min[2] }, {0,+1,0}, {0,1}, colour),

        // Right quad
        RenderVertex(Vector3{ max[0], max[1], min[2] }, {+1,0,0}, {1,1}, colour),
        RenderVertex(Vector3{ max[0], max[1], max[2] }, {+1,0,0}, {1,0}, colour),
        RenderVertex(Vector3{ max[0], min[1], max[2] }, {+1,0,0}, {0,0}, colour),
        RenderVertex(Vector3{ max[0], min[1], min[2] }, {+1,0,0}, {0,1}, colour),

        // Left quad
        RenderVertex(Vector3{ min[0], max[1], min[2] }, {-1,0,0}, {0,1}, colour),
        RenderVertex(Vector3{ min[0], min[1], min[2] }, {-1,0,0}, {1,1}, colour),
        RenderVertex(Vector3{ min[0], min[1], max[2] }, {-1,0,0}, {1,0}, colour),
        RenderVertex(Vector3{ min[0], max[1], max[2] }, {-1,0,0}, {0,0}, colour),
    };
}

inline std::vector<RenderVertex> getWireframeBoxVertices(const Vector3& min, const Vector3& max, const Vector4& colour)
{
    // Load the 8 corner points
    return
    {
        // Bottom quad
        RenderVertex(Vector3{ min[0], min[1], min[2] }, {0,0,1}, {0,0}, colour),
        RenderVertex(Vector3{ max[0], min[1], min[2] }, {0,0,1}, {0,0}, colour),
        RenderVertex(Vector3{ max[0], max[1], min[2] }, {0,0,1}, {0,0}, colour),
        RenderVertex(Vector3{ min[0], max[1], min[2] }, {0,0,1}, {0,0}, colour),

        // Top quad
        RenderVertex(Vector3{ min[0], min[1], max[2] }, {0,0,1}, {0,0}, colour),
        RenderVertex(Vector3{ max[0], min[1], max[2] }, {0,0,1}, {0,0}, colour),
        RenderVertex(Vector3{ max[0], max[1], max[2] }, {0,0,1}, {0,0}, colour),
        RenderVertex(Vector3{ min[0], max[1], max[2] }, {0,0,1}, {0,0}, colour),
    };
}

// Indices drawing a hollow box outline, corresponding to the order in getWireframeBoxVertices()
inline std::vector<unsigned int> generateWireframeBoxIndices()
{
    return
    {
        0, 1, // bottom rectangle
        1, 2, //
        2, 3, //
        3, 0, //

        4, 5, // top rectangle
        5, 6, //
        6, 7, //
        7, 4, //

        0, 4, // vertical edges
        1, 5, //
        2, 6, //
        3, 7, //
    };
};

// Indices drawing a hollow box outline, corresponding to the order in getFillBoxVertices()
inline std::vector<unsigned int> generateFillBoxIndices()
{
    return
    {
        3, 2, 1, 0, // bottom rectangle
        7, 6, 5, 4, // top rectangle

        11, 10, 9, 8, // sides
        15, 14, 13, 12,
        19, 18, 17, 16,
        23, 22, 21, 20,
    };
};

inline std::vector<unsigned int> generateTriangleBoxIndices()
{
    return
    {
        3, 2, 1, 3, 1, 0, // bottom rectangle
        7, 6, 5, 7, 5, 4, // top rectangle

        11, 10, 9, 11, 9, 8, // sides
        15, 14, 13, 15, 13, 12,
        19, 18, 17, 19, 17, 16,
        23, 22, 21, 23, 21, 20,
    };
};

}

class RenderableBox :
    public RenderableGeometry
{
private:
    const AABB& _bounds;
    const Vector3& _worldPos;
    bool _needsUpdate;
    bool _filledBox;

public:
    RenderableBox(const AABB& bounds, const Vector3& worldPos) :
        _bounds(bounds),
        _worldPos(worldPos),
        _needsUpdate(true),
        _filledBox(true)
    {}

    void queueUpdate()
    {
        _needsUpdate = true;
    }

    void setFillMode(bool fill)
    {
        if (_filledBox != fill)
        {
            _filledBox = fill;
            clear();
            queueUpdate();
        }
    }

    virtual Vector4 getVertexColour()
    {
        return Vector4(1, 1, 1, 1);
    }

    virtual void updateGeometry() override
    {
        if (!_needsUpdate) return;

        _needsUpdate = false;

        // Calculate the corner vertices of this bounding box
        Vector3 max(_bounds.origin + _bounds.extents);
        Vector3 min(_bounds.origin - _bounds.extents);

        auto colour = getVertexColour();

        auto vertices = _filledBox ?
            detail::getFillBoxVertices(min, max, colour) :
            detail::getWireframeBoxVertices(min, max, colour);

        auto worldPos = Vector3f(static_cast<float>(_worldPos.x()), static_cast<float>(_worldPos.y()), static_cast<float>(_worldPos.z()));

        // Move the points to their world position
        for (auto& vertex : vertices)
        {
            vertex.vertex += worldPos;
        }

        static auto FillBoxIndices = detail::generateFillBoxIndices();
        static auto WireframeBoxIndices = detail::generateWireframeBoxIndices();

        if (_filledBox)
        {
            updateGeometryWithData(GeometryType::Quads, vertices, FillBoxIndices);
        }
        else
        {
            updateGeometryWithData(GeometryType::Lines, vertices, WireframeBoxIndices);
        }
    }
};

}
