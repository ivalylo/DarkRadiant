#include "ToolMappingDialog.h"

#include <wx/sizer.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/stattext.h>
#include "wxutil/Modifier.h"
#include "wxutil/MouseButton.h"
#include "wxutil/TreeModelFilter.h"
#include <functional>

#include "BindToolDialog.h"

namespace ui
{

namespace
{
    const int TOOLMAPPING_DEFAULT_SIZE_X = 600;
    const int TOOLMAPPING_DEFAULT_SIZE_Y = 550;
    const char* const TOOLMAPPING_WINDOW_TITLE = N_("Edit Mouse Bindings");
}

ToolMappingDialog::ToolMappingDialog() :
    DialogBase(_(TOOLMAPPING_WINDOW_TITLE))
{
    // Create the list store that contains the mouse bindings
    createListStore();

    // Create all the widgets
    populateWindow();

    SetSize(TOOLMAPPING_DEFAULT_SIZE_X, TOOLMAPPING_DEFAULT_SIZE_Y);
    CenterOnParent();
}

void ToolMappingDialog::populateWindow()
{
    SetSizer(new wxBoxSizer(wxVERTICAL));

    wxNotebook* notebook = new wxNotebook(this, wxID_ANY);

    GetSizer()->Add(notebook, 1, wxEXPAND | wxALL, 12);
    GetSizer()->Add(CreateStdDialogButtonSizer(wxOK|wxCANCEL), 0, wxALIGN_RIGHT | wxALL, 12);

    SetAffirmativeId(wxID_OK);

    GlobalMouseToolManager().foreachGroup([&](IMouseToolGroup& group)
    {
        wxPanel* panel = new wxPanel(notebook, wxID_ANY);
        panel->SetSizer(new wxBoxSizer(wxVERTICAL));

        wxutil::TreeView* treeView = createTreeView(group);
        treeView->Reparent(panel);
        
        _treeViews[group.getType()] = treeView;

        // Label
        wxStaticText* label = new wxStaticText(panel, wxID_ANY,
            _("Double click row to edit a binding"));

        panel->GetSizer()->Add(treeView, 1, wxEXPAND | wxBOTTOM, 6);
        panel->GetSizer()->Add(label, 0, wxEXPAND | wxALL, 6);

        notebook->AddPage(panel, group.getDisplayName(), false, -1);
    });
}

void ToolMappingDialog::createListStore()
{
    _listStore.reset(new wxutil::TreeModel(_columns, true));

    // Load all mappings for all tools into the store
    // The various views will display a subset using a TreeModelFilter
    GlobalMouseToolManager().foreachGroup([&](IMouseToolGroup& group)
    {
        group.foreachMouseTool([&](const MouseToolPtr& tool)
        {
            wxutil::TreeModel::Row row = _listStore->AddItem();

            row[_columns.group] = static_cast<int>(group.getType());
            row[_columns.toolDisplayName] = tool->getDisplayName();
            row[_columns.toolName] = tool->getName();

            unsigned int mapping = group.getMappingForTool(tool);

            row[_columns.mouseButton] = wxutil::MouseButton::GetButtonString(mapping);
            row[_columns.modifiers] = wxutil::Modifier::GetModifierString(mapping);

            row.SendItemAdded();
        });
    });
}

wxutil::TreeView* ToolMappingDialog::createTreeView(IMouseToolGroup& group)
{
    wxutil::TreeModelFilter::Ptr filter(new wxutil::TreeModelFilter(_listStore));
    
    IMouseToolGroup::Type type = group.getType();

    filter->SetVisibleFunc([type, this](wxutil::TreeModel::Row& row)->bool
    {
        return static_cast<IMouseToolGroup::Type>(row[_columns.group].getInteger()) == type;
    });

    // Create the treeview and pack two columns into it
    wxutil::TreeView* treeView = wxutil::TreeView::Create(this);
    treeView->AssociateModel(filter.get());

    treeView->AppendTextColumn(_("Tool"), _columns.toolDisplayName.getColumnIndex(),
        wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_NOT, wxDATAVIEW_COL_SORTABLE);

    treeView->AppendTextColumn(_("Modifier"), _columns.modifiers.getColumnIndex(),
        wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_NOT, wxDATAVIEW_COL_SORTABLE);

    treeView->AppendTextColumn(_("Button"), _columns.mouseButton.getColumnIndex(),
        wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_NOT, wxDATAVIEW_COL_SORTABLE);

    // Connect the double click event
    treeView->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, 
        std::bind(&ToolMappingDialog::onItemActivated, this, std::placeholders::_1));

    return treeView;
}

IMouseToolGroup::Type ToolMappingDialog::getGroupType(const wxDataViewItem& item)
{
    wxutil::TreeModel::Row row(item, *_listStore);

    return static_cast<IMouseToolGroup::Type>(row[_columns.group].getInteger());
}

IMouseToolGroup& ToolMappingDialog::getGroup(const wxDataViewItem& item)
{
    return GlobalMouseToolManager().getGroup(getGroupType(item));
}

MouseToolPtr ToolMappingDialog::getTool(const wxDataViewItem& item)
{
    IMouseToolGroup& group = getGroup(item);

    wxutil::TreeModel::Row row(item, *_listStore);
    std::string toolName = row[_columns.toolName];

    return group.getMouseToolByName(toolName);
}

void ToolMappingDialog::onItemActivated(wxDataViewEvent& ev)
{
    if (!ev.GetItem().IsOk()) return;

    // Display the bind tool dialog.
    BindToolDialog* dialog = new BindToolDialog(this, getGroup(ev.GetItem()), getTool(ev.GetItem()));

    if (dialog->ShowModal() == wxID_OK)
    {
        unsigned int binding = dialog->getChosenMouseButtonState();

        // TODO: Save to liststore
    }

    dialog->Destroy();
}

void ToolMappingDialog::saveToolMapping()
{
    // TODO
}

int ToolMappingDialog::ShowModal()
{
    int result = DialogBase::ShowModal();

    if (result == wxID_OK)
    {
        // Save mapping
        saveToolMapping();
    }

    return result;
}

void ToolMappingDialog::ShowDialog(const cmd::ArgumentList& args)
{
    ToolMappingDialog* dialog = new ToolMappingDialog;

    dialog->ShowModal();
    dialog->Destroy();
}

}
