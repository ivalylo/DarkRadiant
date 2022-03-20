#include "ObjectRenderer.h"

#include "igl.h"
#include "math/Matrix4.h"
#include "igeometrystore.h"
#include "render/RenderVertex.h"

namespace render
{

void ObjectRenderer::SubmitObject(IRenderableObject& object, IGeometryStore& store)
{
    // Orient the object
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMultMatrixd(object.getObjectTransform());

    // Submit the geometry of this single slot (objects are using triangle primitives)
    SubmitGeometry(object.getStorageLocation(), GL_TRIANGLES, store);

    glPopMatrix();
}

void ObjectRenderer::InitAttributePointers(RenderVertex* bufferStart)
{
    glVertexPointer(3, GL_FLOAT, sizeof(RenderVertex), &bufferStart->vertex);
    glColorPointer(4, GL_FLOAT, sizeof(RenderVertex), &bufferStart->colour);
    glTexCoordPointer(2, GL_FLOAT, sizeof(RenderVertex), &bufferStart->texcoord);
    glNormalPointer(GL_FLOAT, sizeof(RenderVertex), &bufferStart->normal);

    glVertexAttribPointer(GLProgramAttribute::Position, 3, GL_FLOAT, 0, sizeof(RenderVertex), &bufferStart->vertex);
    glVertexAttribPointer(GLProgramAttribute::Normal, 3, GL_FLOAT, 0, sizeof(RenderVertex), &bufferStart->normal);
    glVertexAttribPointer(GLProgramAttribute::TexCoord, 2, GL_FLOAT, 0, sizeof(RenderVertex), &bufferStart->texcoord);
    glVertexAttribPointer(GLProgramAttribute::Tangent, 3, GL_FLOAT, 0, sizeof(RenderVertex), &bufferStart->tangent);
    glVertexAttribPointer(GLProgramAttribute::Bitangent, 3, GL_FLOAT, 0, sizeof(RenderVertex), &bufferStart->bitangent);
    glVertexAttribPointer(GLProgramAttribute::Colour, 4, GL_FLOAT, 0, sizeof(RenderVertex), &bufferStart->colour);
}

void ObjectRenderer::SubmitGeometry(IGeometryStore::Slot slot, GLenum primitiveMode, IGeometryStore& store)
{
    auto renderParams = store.getRenderParameters(slot);

    glDrawElementsBaseVertex(primitiveMode, static_cast<GLsizei>(renderParams.indexCount),
        GL_UNSIGNED_INT, renderParams.firstIndex, static_cast<GLint>(renderParams.firstVertex));
}

template<typename ContainerT>
void SubmitGeometryInternal(const ContainerT& slots, GLenum primitiveMode, IGeometryStore& store)
{
    auto surfaceCount = slots.size();

    if (surfaceCount == 0) return;

    // Build the indices and offsets used for the glMulti draw call
    std::vector<GLsizei> sizes;
    std::vector<void*> firstIndices;
    std::vector<GLint> firstVertices;

    sizes.reserve(surfaceCount);
    firstIndices.reserve(surfaceCount);
    firstVertices.reserve(surfaceCount);

    for (const auto slot : slots)
    {
        auto renderParams = store.getRenderParameters(slot);

        sizes.push_back(static_cast<GLsizei>(renderParams.indexCount));
        firstVertices.push_back(static_cast<GLint>(renderParams.firstVertex));
        firstIndices.push_back(renderParams.firstIndex);
    }

    glMultiDrawElementsBaseVertex(primitiveMode, sizes.data(), GL_UNSIGNED_INT,
        firstIndices.data(), static_cast<GLsizei>(sizes.size()), firstVertices.data());
}

void ObjectRenderer::SubmitGeometry(const std::set<IGeometryStore::Slot>& slots, GLenum primitiveMode, IGeometryStore& store)
{
    SubmitGeometryInternal(slots, primitiveMode, store);
}

void ObjectRenderer::SubmitGeometry(const std::vector<IGeometryStore::Slot>& slots, GLenum primitiveMode, IGeometryStore& store)
{
    SubmitGeometryInternal(slots, primitiveMode, store);
}

}