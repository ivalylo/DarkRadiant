#include "VcsStatus.h"

#include "i18n.h"
#include "imap.h"
#include "iuserinterface.h"
#include "imainframe.h"

#include <wx/sizer.h>
#include <sigc++/functors/mem_fun.h>

#include "registry/registry.h"
#include "os/file.h"
#include "os/path.h"
#include "../GitModule.h"
#include "../Diff.h"
#include "../GitException.h"

namespace vcs
{

namespace ui
{

VcsStatus::VcsStatus(wxWindow* parent) :
    wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS, "VcsStatusBarPanel"),
    _timer(this),
    _fetchInProgress(false)
{
    SetSizer(new wxBoxSizer(wxVERTICAL));

    auto table = new wxFlexGridSizer(2);
    table->AddGrowableCol(0);
    table->AddGrowableCol(1);
    table->SetHGap(6);
    GetSizer()->Add(table, 0, wxALL | wxEXPAND, 1);

    _mapStatus = new wxStaticText(this, wxID_ANY, "");
    table->Add(_mapStatus, 0, wxLEFT, 6);

    _remoteStatus = new wxStaticText(this, wxID_ANY, _("Not under Version Control"));
    table->Add(_remoteStatus, 0, wxALIGN_RIGHT | wxRIGHT, 6);

    Bind(wxEVT_TIMER, &VcsStatus::onIntervalReached, this);
    Bind(wxEVT_IDLE, &VcsStatus::onIdle, this);

    GlobalRegistry().signalForKey(RKEY_AUTO_FETCH_ENABLED).connect(
        sigc::mem_fun(this, &VcsStatus::restartTimer)
    );
    GlobalRegistry().signalForKey(RKEY_AUTO_FETCH_INTERVAL).connect(
        sigc::mem_fun(this, &VcsStatus::restartTimer)
    );

    GlobalMapModule().signal_modifiedChanged().connect(
        sigc::mem_fun(this, &VcsStatus::updateMapFileStatus)
    );

    GlobalMapModule().signal_mapEvent().connect(
        sigc::mem_fun(this, &VcsStatus::onMapEvent)
    );
}

VcsStatus::~VcsStatus()
{
    if (_fetchTask.valid())
    {
        _fetchTask.get(); // Wait for the thread to complete
    }

    if (_mapFileTask.valid())
    {
        _mapFileTask.get(); // Wait for the thread to complete
    }
}

void VcsStatus::setRepository(const std::shared_ptr<git::Repository>& repository)
{
    _repository = repository;

    if (!_repository)
    {
        _remoteStatus->SetLabel(_("Not under version control"));
        _timer.Stop();
        return;
    }

    _remoteStatus->SetLabel(_repository->getCurrentBranchName());
    restartTimer();

    // Run a fetch update right after connecting to the repo, if auto-fetch is enabled
    if (registry::getValue<bool>(RKEY_AUTO_FETCH_ENABLED))
    {
        startFetchTask();
    }
}

void VcsStatus::restartTimer()
{
    _timer.Stop();

    if (registry::getValue<bool>(RKEY_AUTO_FETCH_ENABLED))
    {
        int interval = static_cast<int>(registry::getValue<float>(RKEY_AUTO_FETCH_INTERVAL) * 60 * 1000);

        if (interval > 0)
        {
            _timer.Start(interval);
        }
    }
}

void VcsStatus::onMapEvent(IMap::MapEvent ev)
{
    if (ev == IMap::MapSaved)
    {
        updateMapFileStatus();
    }
}

void VcsStatus::startFetchTask()
{
    {
        std::lock_guard<std::mutex> guard(_taskLock);

        if (_fetchInProgress || !_repository) return;

        if (!GlobalMainFrame().isActiveApp())
        {
            rMessage() << "Skipping fetch this round, since the app is not active." << std::endl;
            return;
        }

        _fetchInProgress = true;
    }

    auto repository = _repository->clone();
    _fetchTask = std::async(std::launch::async, std::bind(&VcsStatus::performFetch, this, repository));
}

void VcsStatus::onIntervalReached(wxTimerEvent& ev)
{
    startFetchTask();
}

void VcsStatus::updateMapFileStatus()
{
    if (GlobalMapModule().isModified())
    {
        _mapStatus->SetLabel(_("Map is modified"));
    }
    else
    {
        _mapStatus->SetLabel(_("Map is saved"));

        if (_repository)
        {
            auto repository = _repository->clone();
            _mapFileTask = std::async(std::launch::async, std::bind(&VcsStatus::performMapFileStatusCheck, this, repository));
        }
    }
}

void VcsStatus::onIdle(wxIdleEvent& ev)
{
    ev.Skip();
}

void VcsStatus::performFetch(std::shared_ptr<git::Repository> repository)
{
    std::string statusText;

    auto head = repository->getHead();

    if (!head)
    {
        _fetchInProgress = false;
        return;
    }

    try
    {
        repository->getUpstreamRemoteName(*head);
    }
    catch (const git::GitException&)
    {
        setRemoteStatus(_("Not connected"));
        _fetchInProgress = false;
        return;
    }

    try
    {
        setRemoteStatus(_("Fetching..."));

        repository->fetchFromTrackedRemote();

        auto status = repository->getSyncStatusOfBranch(*repository->getHead());

        if (status.localIsUpToDate)
        {
            statusText = _("Up to date");
        }
        else if (status.localCanBePushed)
        {
            statusText = fmt::format(_("{0} to push"), status.localCommitsAhead);
        }
        else if (status.localCommitsAhead == 0)
        {
            statusText = fmt::format(_("{0} to integrate"), status.remoteCommitsAhead);
        }
        else
        {
            statusText = fmt::format(_("{0} to push, {1} to integrate"), status.localCommitsAhead, status.remoteCommitsAhead);
        }

        if (status.remoteCommitsAhead > 0)
        {
            auto mapPath = getRepositoryRelativePath(GlobalMapModule().getMapName(), repository);

            if (!mapPath.empty())
            {
                // Check the incoming commits for modifications of the loaded map
                auto head = repository->getHead();
                auto upstream = head->getUpstream();

                // Find the merge base for this ref and its upstream
                auto mergeBase = repository->findMergeBase(*head, *upstream);

                auto diffAgainstBase = repository->getDiff(*upstream, *mergeBase);

                if (diffAgainstBase->containsFile(mapPath))
                {
                    statusText = fmt::format(_("{0} possible conflict"), status.remoteCommitsAhead);
                }
                else
                {
                    statusText = fmt::format(_("{0} no conflicts"), status.remoteCommitsAhead);
                }
            }
        }
    }
    catch (const git::GitException& ex)
    {
        statusText = ex.what();
    }

    setRemoteStatus(statusText);

    _fetchInProgress = false;
}

void VcsStatus::setMapFileStatus(const std::string& status)
{
    GlobalUserInterface().dispatch([this, status]() { _mapStatus->SetLabel(status); });
}

void VcsStatus::setRemoteStatus(const std::string& status)
{
    GlobalUserInterface().dispatch([this, status]() { _remoteStatus->SetLabel(status); });
}

std::string VcsStatus::getRepositoryRelativePath(const std::string& path, const std::shared_ptr<git::Repository>& repository)
{
    if (!os::fileOrDirExists(path))
    {
        return ""; // doesn't exist
    }

    auto relativePath = os::getRelativePath(path, repository->getPath());

    if (relativePath == path)
    {
        return ""; // outside VCS
    }

    return relativePath;
}

void VcsStatus::performMapFileStatusCheck(std::shared_ptr<git::Repository> repository)
{
    setMapFileStatus(_("Checking map status..."));

    try
    {
        if (GlobalMapModule().isUnnamed())
        {
            setMapFileStatus(_("Map not saved yet"));
            return;
        }

        auto relativePath = getRepositoryRelativePath(GlobalMapModule().getMapName(), repository);

        if (relativePath.empty())
        {
            setMapFileStatus(_("Map not in VCS"));
            return;
        }

        if (repository->fileHasUncommittedChanges(relativePath))
        {
            setMapFileStatus(_("Map saved, pending commit"));
        }
        else if (repository->fileIsIndexed(relativePath))
        {
            setMapFileStatus(_("Map committed"));
        }
        else
        {
            setMapFileStatus(_("Map saved"));
        }
    }
    catch (const git::GitException& ex)
    {
        setMapFileStatus(std::string("ERROR: ") + ex.what());
    }
}

}

}
