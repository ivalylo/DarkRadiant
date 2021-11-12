#include "PatchRenderables.h"

void RenderablePatchWireframe::render(const RenderInfo& info) const
{
    // No colour changing
    glDisableClientState(GL_COLOR_ARRAY);
    if (info.checkFlag(RENDER_VERTEX_COLOUR))
    {
        glColor3f(1, 1, 1);
    }

    if (_tess.vertices.empty()) return;

    if (_needsUpdate)
    {
        _needsUpdate = false;

        // Create a VBO and add the vertex data
        VertexBuffer_T currentVBuf;
        currentVBuf.addVertices(_tess.vertices.begin(), _tess.vertices.end());

        // Submit index batches
        const RenderIndex* strip_indices = &_tess.indices.front();
        for (std::size_t i = 0;
            i < _tess.numStrips;
            i++, strip_indices += _tess.lenStrips)
        {
            currentVBuf.addIndexBatch(strip_indices, _tess.lenStrips);
        }

        // Render all index batches
        _vertexBuf.replaceData(currentVBuf);
    }

    _vertexBuf.renderAllBatches(GL_QUAD_STRIP);
}

void RenderablePatchWireframe::queueUpdate()
{
    _needsUpdate = true;
}

RenderablePatchSolid::RenderablePatchSolid(PatchTesselation& tess) :
    _tess(tess),
    _needsUpdate(true)
{}

void RenderablePatchSolid::render(const RenderInfo& info) const
{
    if (_tess.vertices.empty() || _tess.indices.empty()) return;

    if (!info.checkFlag(RENDER_BUMP))
    {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    // No colour changing
    glDisableClientState(GL_COLOR_ARRAY);

    if (info.checkFlag(RENDER_VERTEX_COLOUR))
    {
        glColor3f(1, 1, 1);
    }

    if (_needsUpdate)
    {
        _needsUpdate = false;

        // Add vertex geometry to vertex buffer
        VertexBuffer_T currentVBuf;
        currentVBuf.addVertices(_tess.vertices.begin(), _tess.vertices.end());

        // Submit indices
        const RenderIndex* strip_indices = &_tess.indices.front();
        for (std::size_t i = 0;
            i < _tess.numStrips;
            i++, strip_indices += _tess.lenStrips)
        {
            currentVBuf.addIndexBatch(strip_indices, _tess.lenStrips);
        }

        // Render all batches
        _vertexBuf.replaceData(currentVBuf);
    }

    _vertexBuf.renderAllBatches(GL_QUAD_STRIP, info.checkFlag(RENDER_BUMP));

	if (!info.checkFlag(RENDER_BUMP))
	{
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
}

void RenderablePatchSolid::queueUpdate()
{
    _needsUpdate = true;
}

const ShaderPtr& RenderablePatchVectorsNTB::getShader() const
{
	return _shader;
}

RenderablePatchVectorsNTB::RenderablePatchVectorsNTB(const PatchTesselation& tess) :
	_tess(tess)
{}

void RenderablePatchVectorsNTB::setRenderSystem(const RenderSystemPtr& renderSystem)
{
	if (renderSystem)
	{
		_shader = renderSystem->capture("$PIVOT");
	}
	else
	{
		_shader.reset();
	}
}

#define	VectorMA( v, s, b, o )		((o)[0]=(v)[0]+(b)[0]*(s),(o)[1]=(v)[1]+(b)[1]*(s),(o)[2]=(v)[2]+(b)[2]*(s))

void RenderablePatchVectorsNTB::render(const RenderInfo& info) const
{
	if (_tess.vertices.empty()) return;

	glBegin(GL_LINES);

	for (const ArbitraryMeshVertex& v : _tess.vertices)
	{
		Vector3 end;

		glColor3f(0, 0, 1);
		glVertex3dv(v.vertex);
		VectorMA(v.vertex, 5, v.normal, end);
		glVertex3dv(end);

		glColor3f(1, 0, 0);
		glVertex3dv(v.vertex);
		VectorMA(v.vertex, 5, v.tangent, end);
		glVertex3dv(end);

		glColor3f(0, 1, 0);
		glVertex3dv(v.vertex);
		VectorMA(v.vertex, 5, v.bitangent, end);
		glVertex3dv(end);

		glColor3f(1, 1, 1);
		glVertex3dv(v.vertex);
		glVertex3dv(v.vertex);
	}

	glEnd();
}

void RenderablePatchVectorsNTB::render(RenderableCollector& collector, const VolumeTest& volume, const Matrix4& localToWorld) const
{
	collector.setHighlightFlag(RenderableCollector::Highlight::Primitives, false);
	collector.addRenderable(*_shader, *this, localToWorld);
}

void RenderablePatchTesselation::queueUpdate()
{
    _needsUpdate = true;
}

void RenderablePatchTesselation::update(const ShaderPtr& shader)
{
    bool shaderChanged = _shader != shader;

    if (!_needsUpdate && !shaderChanged) return;

    _needsUpdate = false;
    auto sizeChanged = _tess.vertices.size() != _size;

    if (_shader && _surfaceSlot != render::ISurfaceRenderer::InvalidSlot && (shaderChanged || sizeChanged))
    {
        _shader->removeSurface(_surfaceSlot);
        _surfaceSlot = render::ISurfaceRenderer::InvalidSlot;
        _size = 0;
    }

    _shader = shader;
    _size = _tess.vertices.size();

    // Generate the index array
    std::vector<unsigned int> indices;
    indices.reserve((_tess.height - 1) * (_tess.width - 1) * 6); // 6 => 2 triangles per quad

    // Generate the indices to define the triangles in clockwise order
    for (std::size_t h = 0; h < _tess.height - 1; ++h)
    {
        auto rowOffset = h * _tess.width;

        for (std::size_t w = 0; w < _tess.width - 1; ++w)
        {
            indices.push_back(static_cast<unsigned int>(rowOffset + w + _tess.width));
            indices.push_back(static_cast<unsigned int>(rowOffset + w + 1));
            indices.push_back(static_cast<unsigned int>(rowOffset + w));

            indices.push_back(static_cast<unsigned int>(rowOffset + w + _tess.width));
            indices.push_back(static_cast<unsigned int>(rowOffset + w + _tess.width + 1));
            indices.push_back(static_cast<unsigned int>(rowOffset + w + 1));
        }
    }

    if (_surfaceSlot == render::ISurfaceRenderer::InvalidSlot)
    {
        _surfaceSlot = shader->addSurface(_tess.vertices, indices);
    }
    else
    {
        shader->updateSurface(_surfaceSlot, _tess.vertices, indices);
    }
}
