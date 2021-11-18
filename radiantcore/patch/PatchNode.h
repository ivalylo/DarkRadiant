#pragma once

#include "irenderable.h"
#include "icomparablenode.h"
#include "iscenegraph.h"
#include "itraceable.h"
#include "imap.h"
#include "Patch.h"
#include "scene/SelectableNode.h"
#include "PatchControlInstance.h"
#include "dragplanes.h"
#include "PatchRenderables.h"

class PatchNode final :
	public scene::SelectableNode,
	public scene::Cloneable,
	public Snappable,
	public IdentityTransform,
	public IPatchNode,
	public SelectionTestable,
	public ComponentSelectionTestable,
	public ComponentEditable,
	public ComponentSnappable,
	public PlaneSelectable,
	public LitObject,
	public Transformable,
	public ITraceable,
    public scene::IComparableNode
{
	selection::DragPlanes m_dragPlanes;

	// The patch control instances
	typedef std::vector<PatchControlInstance> PatchControlInstances;
	PatchControlInstances m_ctrl_instances;

	// An array of renderable points
	mutable RenderablePointVector m_render_selected;

	Patch m_patch;

	// An internal AABB variable to calculate the bounding box of the selected components (has to be mutable)
	mutable AABB m_aabb_component;

	ShaderPtr m_state_selpoint;

    // For pivoted rotations, we need a copy of this lying around
    Vector3 _untransformedOrigin;
    // If true, the _untransformedOrigin member needs an update
    bool _untransformedOriginChanged;

    mutable bool _selectedControlVerticesNeedUpdate;

    RenderablePatchTesselation<TesselationIndexer_Triangles> _renderableSurfaceSolid;
    RenderablePatchTesselation<TesselationIndexer_Quads> _renderableSurfaceWireframe;

public:
	// Construct a PatchNode with no arguments
	PatchNode(patch::PatchDefType type);

	// Copy Constructor
	PatchNode(const PatchNode& other);

    ~PatchNode();

	// Patch::Observer implementation
	void allocate(std::size_t size);

	std::string name() const override;
	Type getNodeType() const override;

    // IComparableNode implementation
    std::string getFingerprint() override;

	// Bounded implementation
	const AABB& localAABB() const override;

	// IPatchNode implementation
	Patch& getPatchInternal() override;
	IPatch& getPatch() override;

	// Snappable implementation
	virtual void snapto(float snap) override;

	// Test the Patch instance for selection (SelectionTestable)
	void testSelect(Selector& selector, SelectionTest& test) override;

	// Check if the drag planes pass the given selection test (and select them of course and call the callback)
	void selectPlanes(Selector& selector, SelectionTest& test, const PlaneCallback& selectedPlaneCallback) override;
  	void selectReversedPlanes(Selector& selector, const SelectedPlanes& selectedPlanes) override;

	// Returns true if any of the patch components is selected
	bool isSelectedComponents() const override;
	// Set the components (control points or dragplanes) selection to <select>
	void setSelectedComponents(bool select, selection::ComponentSelectionMode mode) override;
	// Invert the component selection
	void invertSelectedComponents(selection::ComponentSelectionMode mode) override;
	// Tests the patch components on selection using the passed SelectionTest
	void testSelectComponents(Selector& selector, SelectionTest& test, selection::ComponentSelectionMode mode) override;

	// override scene::Inode::onRemoveFromScene to deselect the child components
    void onInsertIntoScene(scene::IMapRootNode& root) override;
    void onRemoveFromScene(scene::IMapRootNode& root) override;

	// Traceable implementation
	bool getIntersection(const Ray& ray, Vector3& intersection) override;

	// Create the axis aligned bounding box of the selected components
	const AABB& getSelectedComponentsBounds() const override;

	// Sets all Control Instances to selected = <selected>
  	void selectCtrl(bool selected);

	// Returns true if this patch can be rendered
	bool isVisible() const;

	// Returns true if the material itself is visible
	bool hasVisibleMaterial() const;

	// greebo: snaps all the _selected_ components to the grid (should be called "snapSelectedComponents")
	void snapComponents(float snap) override;

	// Returns true if any of the Control Vertices is selected
	bool selectedVertices();

	// Clones this node, allocates a new Node on the heap and passes itself to the constructor of the new node
	scene::INodePtr clone() const override;

	// greebo: This gets called by the ObservedSelectable as soon as its selection state changes
	// (see ObservedSelectable and PatchControlInstance)
	void selectedChangedComponent(const ISelectable& selectable);

	// LitObject implementation
	bool intersectsLight(const RendererLight& light) const override;

	// Renderable implementation

	// Render functions, these make sure that all things get rendered properly. The calls are also passed on
	// to the contained patch <m_patch>
    void onPreRender(const VolumeTest& volume) override;
	void renderSolid(IRenderableCollector& collector, const VolumeTest& volume) const override;
	void renderWireframe(IRenderableCollector& collector, const VolumeTest& volume) const override;
	void renderHighlights(IRenderableCollector& collector, const VolumeTest& volume) override;
	void setRenderSystem(const RenderSystemPtr& renderSystem) override;

	// Renders the components of this patch instance, makes use of the Patch::render_component() method
	void renderComponents(IRenderableCollector& collector, const VolumeTest& volume) const override;

	void evaluateTransform();
	std::size_t getHighlightFlags() override;

    // Returns the center of the untransformed world AABB
    const Vector3& getUntransformedOrigin() override;

    void onControlPointsChanged();
    void onMaterialChanged();
    void onTesselationChanged();

protected:
	// Gets called by the Transformable implementation whenever
	// scale, rotation or translation is changed.
    void _onTransformationChanged() override;

	// Called by the Transformable implementation before freezing
	// or when reverting transformations.
    void _applyTransformation() override;

    void onVisibilityChanged(bool isVisibleNow) override;

private:
	// Transforms the patch components with the given transformation matrix
	void transformComponents(const Matrix4& matrix);

	// greebo: Updates the internal render array m_render_selected, that contains all control vertices that should be
	// rendered as highlighted.
	void updateSelectedControlVertices() const;

	// greebo: Renders the selected components. This is called by the above two render functions
	void renderComponentsSelected(IRenderableCollector& collector, const VolumeTest& volume) const;
};
typedef std::shared_ptr<PatchNode> PatchNodePtr;
typedef std::weak_ptr<PatchNode> PatchNodeWeakPtr;
