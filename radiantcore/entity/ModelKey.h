#pragma once

#include <string>
#include "inode.h"
#include "ieclass.h"
#include "ObservedUndoable.h"
#include <sigc++/connection.h>

/**
 * @brief A ModelKey watches the "model" spawnarg of an entity.
 *
 * As soon as the keyvalue changes, the according modelnode is loaded and
 * inserted into the entity's Traversable.
 */
class ModelKey :
    public sigc::trackable
{
private:
	// The parent node, where the model node can be added to (as child)
	scene::INode& _parentNode;

	struct ModelNodeAndPath
	{
		scene::INodePtr node;
		std::string path;
        bool modelDefMonitored;
	};

	ModelNodeAndPath _model;

	// To deactivate model handling during node destruction
	bool _active;

	// Saves modelnode and modelpath to undo stack
	undo::ObservedUndoable<ModelNodeAndPath> _undo;

    sigc::connection _modelDefChanged;

public:
	ModelKey(scene::INode& parentNode);

    // Remove any model node from the parent entity
    // used during Entity destruction to remove any child nodes
    // before the parent entity node is going out of business.
    // Disables any further behaviour of this instance, it will no longer be functional
    void destroy();

	// Refreshes the attached model
	void refreshModel();

	// Update the model to the provided keyvalue, this removes the old scene::Node
	// and inserts the new one after acquiring the model from the cache.
	void modelChanged(const std::string& value);

	// Gets called by the attached Entity when the "skin" spawnarg changes
	void skinChanged(const std::string& value);

	// Returns the reference to the "singleton" model node
	const scene::INodePtr& getNode() const;

	void connectUndoSystem(IUndoSystem& undoSystem);
	void disconnectUndoSystem(IUndoSystem& undoSystem);

private:
    void onModelDefChanged();

	// Loads the model node and attaches it to the parent node
    void attachModelNode();
    void detachModelNode();

    // Attaches a model node, making sure that the skin setting is kept
    void attachModelNodeKeepinSkin();

	void importState(const ModelNodeAndPath& data);

    void subscribeToModelDef(const IModelDef::Ptr& modelDef);
    void unsubscribeFromModelDef();
};
