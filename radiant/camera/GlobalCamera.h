#pragma once

#include <map>
#include "icamera.h"
#include "icommandsystem.h"
#include "ieventmanager.h"
#include "imousetoolmanager.h"

#include "CamWnd.h"
#include "FloatingCamWnd.h"
#include "CameraObserver.h"

class wxWindow;

namespace ui
{

/**
 * greebo: This is the gateway class to access the currently active CamWindow
 *
 * This class provides an interface for creating and deleting CamWnd instances
 * as well as some methods that are passed to the currently active CamWnd, like
 * resetCameraAngles() or lookThroughSelected().
 **/
class GlobalCameraManager :
	public ICamera
{
private:
	typedef std::map<int, CamWndWeakPtr> CamWndMap;
	CamWndMap _cameras;

	// The currently active camera window (-1 if no cam active)
	int _activeCam;

	// The connected callbacks (get invoked when movedNotify() is called)
	CameraObserverList _cameraObservers;

    unsigned int _toggleStrafeModifierFlags;
    unsigned int _toggleStrafeForwardModifierFlags;

    float _strafeSpeed;
    float _forwardStrafeFactor;

public:
	// Constructor
	GlobalCameraManager();

	/**
	 * Returns the currently active CamWnd or NULL if none is active.
	 */
	CamWndPtr getActiveCamWnd();

	/**
	 * Create a new camera window, ready for packing into a parent widget.
	 */
	CamWndPtr createCamWnd(wxWindow* parent);

	// Remove the camwnd with the given ID
	void removeCamWnd(int id);

	/**
	 * Get a PersistentFloatingWindow containing the CamWnd widget, creating
	 * it if necessary.
	 */
	FloatingCamWndPtr createFloatingWindow();

	// Resets the camera angles of the currently active Camera
	void resetCameraAngles(const cmd::ArgumentList& args);

	/** greebo: Sets the camera to the given point/angle.
	 */
	void focusCamera(const Vector3& point, const Vector3& angles) override;
	ICameraView& getActiveView() override;

	// Toggles between lighting and solid rendering mode (passes the call to the CameraSettings class)
	void toggleLightingMode(const cmd::ArgumentList& args);

    // Increases/decreases the far clip plane distance (passes the call to
    // CamWnd)
	void farClipPlaneIn(const cmd::ArgumentList& args);
	void farClipPlaneOut(const cmd::ArgumentList& args);

	// Change the floor up/down, passes the call on to the CamWnd class
	void changeFloorUp(const cmd::ArgumentList& args);
	void changeFloorDown(const cmd::ArgumentList& args);

	// angua: increases and decreases the movement speed of the camera
	void increaseCameraSpeed(const cmd::ArgumentList& args);
	void decreaseCameraSpeed(const cmd::ArgumentList& args);

	// greebo: This measures the rendering time for a full 360 degrees turn of the camera
	// Note: unused at the moment
	void benchmark();

	void update();
    void forceDraw();

	// Add a "CameraMoved" callback to the signal member
	void addCameraObserver(CameraObserver* observer);
	void removeCameraObserver(CameraObserver* observer);

	// Notify the attached "CameraMoved" callbacks
	void movedNotify();

	// Movement commands (the calls are passed on to the Camera class)
	void moveCameraCmd(const cmd::ArgumentList& args);
	void moveLeftDiscrete(const cmd::ArgumentList& args);
	void moveRightDiscrete(const cmd::ArgumentList& args);
	void pitchUpDiscrete(const cmd::ArgumentList& args);
	void pitchDownDiscrete(const cmd::ArgumentList& args);

    // Camera strafe behaviour
    float getCameraStrafeSpeed();
    float getCameraForwardStrafeFactor();
    unsigned int getStrafeModifierFlags();
    unsigned int getStrafeForwardModifierFlags();

    MouseToolStack getMouseToolsForEvent(wxMouseEvent& ev);
    void foreachMouseTool(const std::function<void(const ui::MouseToolPtr&)>& func);

public:
	// Callbacks for the named camera KeyEvents
	void onFreelookMoveForwardKey(ui::KeyEventType eventType);
	void onFreelookMoveBackKey(ui::KeyEventType eventType);
	void onFreelookMoveLeftKey(ui::KeyEventType eventType);
	void onFreelookMoveRightKey(ui::KeyEventType eventType);
	void onFreelookMoveUpKey(ui::KeyEventType eventType);
	void onFreelookMoveDownKey(ui::KeyEventType eventType);

	// RegisterableModule implementation
	const std::string& getName() const;
	const StringSet& getDependencies() const;
	void initialiseModule(const IApplicationContext& ctx);
	void shutdownModule();

private:
	// greebo: The construct method registers all the commands
	void registerCommands();
    void loadCameraStrafeDefinitions();

	void doWithActiveCamWnd(const std::function<void(CamWnd&)>& action);
};

} // namespace

// The accessor function that contains the static instance of the GlobalCameraManager class
ui::GlobalCameraManager& GlobalCamera();
