#include "EntityClassChooser.h"
#include "EntityClassTreePopulator.h"

#include "i18n.h"
#include "imainframe.h"
#include "iuimanager.h"

#include <glibmm.h>
#include <gtkmm/box.h>
#include <gtkmm/textview.h>
#include <gtkmm/button.h>
#include <gtkmm/stock.h>
#include <gtkmm/paned.h>

#include "gtkutil/TreeModel.h"
#include "gtkutil/ScrolledFrame.h"
#include "gtkutil/RightAlignment.h"
#include "gtkutil/IconTextColumn.h"
#include "gtkutil/MultiMonitor.h"
#include "string/string.h"
#include "registry/bind.h"

#include "debugging/ScopedDebugTimer.h"

namespace ui
{

namespace
{
    const char* const ECLASS_CHOOSER_TITLE = N_("Create entity");
    const std::string RKEY_SPLIT_POS = "user/ui/entityClassChooser/splitPos";
}

// Local class for loading entity class definitions in a separate thread. A
// delegated class is required because the slot executed in a thread must not
// belong to a class which derives from sigc::trackable (which all Gtk::Widget
// subclasses do) [1].
//
// The class owns a Glib::Dispatcher which is like a sigc::signal which works
// across threads [2]. The EntityClassChooser must connect to this dispatcher
// BEFORE the thread is started, then the thread may invoke the dispatcher to
// emit a signal but must NOT write to any member variables directly. When the
// dispatcher is invoked, the connected slot in EntityClassChooser will run in
// the main GUI thread, and will retrieve the data from
// ThreadedEntityClassLoader and display it in the GUI.
//
// [1] http://developer.gnome.org/glibmm/2.28/classGlib_1_1Thread.html#ab165854ff2fc9b454ee4d97050485782
// [2] http://developer.gnome.org/glibmm/2.28/classGlib_1_1Dispatcher.html

class EntityClassChooser::ThreadedEntityClassLoader
{
    // The dispatcher object
    Glib::Dispatcher _dispatcher;

    // Column specification struct
    const EntityClassChooser::TreeColumns& _columns;

    // The tree store to populate. We must operate on our own tree store, since
    // updating the EntityClassChooser's tree store from a different thread
    // wouldn't be safe
    Glib::RefPtr<Gtk::TreeStore> _treeStore;

    // The thread object
    Glib::Thread* _thread;

private:

    // The worker function that will execute in the thread
    void run()
    {
        ScopedDebugTimer timer("ThreadedEntityClassLoader::run()");

        // Create new treestoree
        _treeStore = Gtk::TreeStore::create(_columns);

        // Populate it with the list of entity classes by using a visitor class.
        EntityClassTreePopulator visitor(_treeStore, _columns);
        GlobalEntityClassManager().forEachEntityClass(visitor);

        // Insert the data, using the same walker class as Visitor
        visitor.forEachNode(visitor);

        // Invoke dispatcher to notify the EntityClassChooser
        _dispatcher();
    }

public:

    // Construct and initialise variables
    ThreadedEntityClassLoader(const EntityClassChooser::TreeColumns& cols)
    : _columns(cols), _thread(NULL)
    { }

    // Connect the given slot to be invoked when entity population has finished.
    // The slot will be invoked in the main thread (to be precise, the thread
    // that called connectFinishedSlot()).
    void connectFinishedSlot(const sigc::slot<void>& slot)
    {
        _dispatcher.connect(slot);
    }

    // Return the populated treestore, and join the thread (wait for it to
    // finish and release its resources).
    Glib::RefPtr<Gtk::TreeStore> getTreeStoreAndQuit()
    {
        g_assert(_thread);
        _thread->join();

        return _treeStore;
    }

    // Start loading entity classes in a new thread
    void loadEntityClasses()
    {
        _thread = Glib::Thread::create(
            sigc::mem_fun(*this, &ThreadedEntityClassLoader::run), true
        );
    }
};

// Main constructor
EntityClassChooser::EntityClassChooser()
: gtkutil::BlockingTransientWindow(_(ECLASS_CHOOSER_TITLE),
                                   GlobalMainFrame().getTopLevelWindow()),
  gtkutil::GladeWidgetHolder(
        GlobalUIManager().getGtkBuilderFromFile("EntityClassChooser.glade")
  ),
  _eclassLoader(new ThreadedEntityClassLoader(_columns)),
  _selection(NULL),
  _selectedName(""),
  _modelPreview(new gtkutil::ModelPreview()),
  _result(RESULT_CANCELLED)
{
    // Set the default size of the window
    const Glib::RefPtr<Gtk::Window>& mainWindow = GlobalMainFrame().getTopLevelWindow();
    Gdk::Rectangle rect = gtkutil::MultiMonitor::getMonitorForWindow(mainWindow);
    set_default_size(
        static_cast<int>(rect.get_width() * 0.7f),
        static_cast<int>(rect.get_height() * 0.6f)
    );

    // Set the model preview height to something significantly smaller than the
    // window's height to allow shrinking
    _modelPreview->setSize(static_cast<int>(rect.get_width() * 0.4f),
                           static_cast<int>(rect.get_height() * 0.2f));

    // Create GUI elements and pack into self
    Gtk::Paned* mainPaned = getGladeWidget<Gtk::Paned>("mainPaned");
    add(*mainPaned);
    g_assert(get_child() != NULL);

    // Connect button signals
    getGladeWidget<Gtk::Button>("okButton")->signal_clicked().connect(
        sigc::mem_fun(*this, &EntityClassChooser::callbackOK)
    );
    getGladeWidget<Gtk::Button>("cancelButton")->signal_clicked().connect(
        sigc::mem_fun(*this, &EntityClassChooser::callbackCancel)
    );

    // Add model preview to right-hand-side of main container
    mainPaned->pack2(*_modelPreview, true, true);

    // Listen for defs-reloaded signal (cannot bind directly to
    // ThreadedEntityClassLoader method because it is not sigc::trackable)
    GlobalEntityClassManager().defsReloadedSignal().connect(
        sigc::mem_fun(this, &EntityClassChooser::reloadEntityClasses)
    );

    // Setup the tree view and invoke threaded loader to get the entity classes
    setupTreeView();
    _eclassLoader->connectFinishedSlot(
        sigc::mem_fun(*this, &EntityClassChooser::getEntityClassesFromLoader)
    );
    _eclassLoader->loadEntityClasses();

    // Persist layout to registry
    registry::bindPropertyToKey(mainPaned->property_position(), RKEY_SPLIT_POS);
}

void EntityClassChooser::getEntityClassesFromLoader()
{
    _treeStore = _eclassLoader->getTreeStoreAndQuit();
    setTreeViewModel();
}

// Display the singleton instance
std::string EntityClassChooser::chooseEntityClass()
{
    Instance().show();

    if (Instance().getResult() == RESULT_OK)
    {
        return Instance().getSelectedEntityClass();
    }
    else
    {
        return ""; // Empty selection on cancel
    }
}

EntityClassChooser& EntityClassChooser::Instance()
{
    EntityClassChooserPtr& instancePtr = InstancePtr();

    if (instancePtr == NULL)
    {
        // Not yet instantiated, do it now
        instancePtr.reset(new EntityClassChooser);

        // Register this instance with GlobalRadiant() at once
        GlobalRadiant().addEventListener(instancePtr);
    }

    return *instancePtr;
}

EntityClassChooserPtr& EntityClassChooser::InstancePtr()
{
    static EntityClassChooserPtr _instancePtr;
    return _instancePtr;
}

void EntityClassChooser::onRadiantShutdown()
{
    globalOutputStream() << "EntityClassChooser shutting down." << std::endl;

    _modelPreview.reset();

    // Final step at shutdown, release the shared ptr
    InstancePtr().reset();
}

void EntityClassChooser::reloadEntityClasses()
{
    // Reload the class tree
    g_assert(_eclassLoader);
    _eclassLoader->loadEntityClasses();
}


EntityClassChooser::Result EntityClassChooser::getResult()
{
    return _result;
}

void EntityClassChooser::setSelectedEntityClass(const std::string& eclass)
{
    // Select immediately if possible, otherwise remember class name for later
    // selection
    if (_treeStore)
    {
        gtkutil::TreeModel::findAndSelectString(
            getGladeWidget<Gtk::TreeView>("entityTreeView"),
            eclass,
            _columns.name
        );
        _classToHighlight.clear();
    }
    else
    {
        _classToHighlight = eclass;
    }
}

const std::string& EntityClassChooser::getSelectedEntityClass() const
{
    return _selectedName;
}

void EntityClassChooser::_onDeleteEvent()
{
    _result = RESULT_CANCELLED;

    // greebo: Clear the selected name on hide, we don't want to create another entity when
    // the user clicks on the X in the upper right corner.
    _selectedName.clear();

    hide(); // just hide, don't call base class which might delete this dialog
}

void EntityClassChooser::_postShow()
{
    // Initialise the GL widget after the widgets have been shown
    _modelPreview->initialisePreview();

    // Update the member variables
    updateSelection();

    // Focus on the treeview
    getGladeWidget<Gtk::TreeView>("entityTreeView")->grab_focus();

    // Call the base class, will enter main loop
    BlockingTransientWindow::_postShow();
}

// Create the tree view

Gtk::TreeView* EntityClassChooser::treeView()
{
    return getGladeWidget<Gtk::TreeView>("entityTreeView");
}

void EntityClassChooser::setTreeViewModel()
{
    // Ensure model is sorted before giving it to the tree view
    gtkutil::TreeModel::applyFoldersFirstSortFunc(
        _treeStore, _columns.name, _columns.isFolder
    );
    treeView()->set_model(_treeStore);

    // Pre-select the given class if requested by setSelectedEntityClass()
    if (!_classToHighlight.empty())
    {
        g_assert(_treeStore);
        setSelectedEntityClass(_classToHighlight);
    }
}

void EntityClassChooser::setupTreeView()
{
    // Use the TreeModel's full string search function
    Gtk::TreeView* view = treeView();
    view->set_search_equal_func(sigc::ptr_fun(gtkutil::TreeModel::equalFuncStringContains));

    _selection = view->get_selection();
    _selection->set_mode(Gtk::SELECTION_BROWSE);
    _selection->signal_changed().connect(
        sigc::mem_fun(*this, &EntityClassChooser::updateSelection)
    );

    // Single column with icon and name
    Gtk::TreeViewColumn* col = Gtk::manage(
        new gtkutil::IconTextColumn(_("Classname"), _columns.name, _columns.icon)
    );
    col->set_sort_column(_columns.name);

    view->append_column(*col);
}

// Update the usage information
void EntityClassChooser::updateUsageInfo(const std::string& eclass)
{
    // Lookup the IEntityClass instance
    IEntityClassPtr e = GlobalEntityClassManager().findOrInsert(eclass, true);

    // Set the usage panel to the IEntityClass' usage information string
    Gtk::TextView* usageText = getGladeWidget<Gtk::TextView>("usageTextView");
    Glib::RefPtr<Gtk::TextBuffer> buf = usageText->get_buffer();

    // Create the concatenated usage string
    std::string usage = "";
    EntityClassAttributeList usageAttrs = e->getAttributeList("editor_usage");

    for (EntityClassAttributeList::const_iterator i = usageAttrs.begin();
         i != usageAttrs.end();
         ++i)
    {
        // Add only explicit (non-inherited) usage strings
        if (!i->inherited)
        {
            if (!usage.empty())
                usage += std::string("\n") + i->getValue();
            else
                usage += i->getValue();
        }
    }

    buf->set_text(usage);
}

void EntityClassChooser::updateSelection()
{
    Gtk::TreeModel::iterator iter = _selection->get_selected();

    if (iter)
    {
        Gtk::TreeModel::Row row = *iter;

        if (!row[_columns.isFolder])
        {
            // Make the OK button active
            getGladeWidget<Gtk::Widget>("okButton")->set_sensitive(true);

            // Set the panel text with the usage information
            std::string selName = row[_columns.name];
            updateUsageInfo(selName);

            // Lookup the IEntityClass instance
            IEntityClassPtr eclass = GlobalEntityClassManager().findClass(selName);

            if (eclass != NULL)
            {
                _modelPreview->setModel(eclass->getAttribute("model").getValue());
                _modelPreview->setSkin(eclass->getAttribute("skin").getValue());
            }

            // Update the _selectionName field
            _selectedName = selName;

            return; // success
        }
    }

    // Nothing selected
    _modelPreview->setModel("");
    _modelPreview->setSkin("");

    getGladeWidget<Gtk::Widget>("okButton")->set_sensitive(false);
}

void EntityClassChooser::callbackCancel()
{
    _result = RESULT_CANCELLED;
    _selectedName.clear();

    hide(); // breaks main loop
}

void EntityClassChooser::callbackOK()
{
    _result = RESULT_OK;

    hide(); // breaks main loop
}

} // namespace ui
