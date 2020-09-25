#pragma once

#include "imodule.h"
#include "icameraview.h"
#include "math/Vector3.h"

namespace ui
{

enum 
{
    CAMERA_PITCH = 0, // up / down
    CAMERA_YAW = 1, // left / right
    CAMERA_ROLL = 2, // fall over
};

/**
 * The "global" interface of DarkRadiant's camera module.
 */
class ICamera :
	public RegisterableModule
{
public:
	/**
	 * greebo: Sets the camera origin to the given <point> using the given <angles>.
	 */
	virtual void focusCamera(const Vector3& point, const Vector3& angles) = 0;

	// A reference to the currently active view. Will throw a std::runtime_error if no camera is present
	virtual ICameraView& getActiveView() = 0;
};
typedef std::shared_ptr<ICamera> ICameraPtr;

} // namespace

const char* const MODULE_CAMERA("Camera");

// Accessor
// (this is named CameraView to avoid name conflicts with the existing GlobalCamera() accessor)
inline ui::ICamera& GlobalCameraView()
{
	// Cache the reference locally
    static ui::ICamera& _camera(
        *std::static_pointer_cast<ui::ICamera>(
			module::GlobalModuleRegistry().getModule(MODULE_CAMERA)
		)
	);
	return _camera;
}
