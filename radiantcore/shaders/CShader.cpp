#include "CShader.h"
#include "Doom3ShaderSystem.h"
#include "MapExpression.h"

#include "iregistry.h"
#include "ishaders.h"
#include "texturelib.h"
#include "gamelib.h"
#include "parser/DefTokeniser.h"

/* CONSTANTS */
namespace {

	// Registry path for default light shader
	const std::string DEFAULT_LIGHT_PATH = "/defaults/lightShader";

}

namespace shaders
{

/* Constructor. Sets the name and the ShaderDefinition to use.
 */
CShader::CShader(const std::string& name, const ShaderDefinition& definition) :
    CShader(name, definition, false)
{}

CShader::CShader(const std::string& name, const ShaderDefinition& definition, bool isInternal) :
    _isInternal(isInternal),
    _originalTemplate(definition.shaderTemplate),
    _template(definition.shaderTemplate),
    _fileInfo(definition.file),
    _name(name),
    m_bInUse(false),
    _visible(true)
{
    subscribeToTemplateChanges();

    // Realise the shader
    realise();
}

CShader::~CShader()
{
    _templateChanged.disconnect();
	unrealise();
	GetTextureManager().checkBindings();
}

float CShader::getSortRequest() const
{
    return _template->getSortRequest();
}

void CShader::setSortRequest(float sortRequest)
{
    ensureTemplateCopy();
    _template->setSortRequest(sortRequest);
}

void CShader::resetSortReqest()
{
    ensureTemplateCopy();
    _template->resetSortReqest();
}

float CShader::getPolygonOffset() const
{
    return _template->getPolygonOffset();
}

void CShader::setPolygonOffset(float offset)
{
    ensureTemplateCopy();
    _template->setPolygonOffset(offset);
}

TexturePtr CShader::getEditorImage()
{
    if (!_editorTexture)
    {
        // Pass the call to the GLTextureManager to realise this image
        _editorTexture = GetTextureManager().getBinding(
            _template->getEditorTexture()
        );
    }

    return _editorTexture;
}

IMapExpression::Ptr CShader::getEditorImageExpression()
{
    return _template->getEditorTexture();
}

void CShader::setEditorImageExpressionFromString(const std::string& editorImagePath)
{
    ensureTemplateCopy();

    _editorTexture.reset();
    _template->setEditorImageExpressionFromString(editorImagePath);
}

bool CShader::isEditorImageNoTex()
{
	return (getEditorImage() == GetTextureManager().getShaderNotFound());
}

IMapExpression::Ptr CShader::getLightFalloffExpression()
{
	return _template->getLightFalloff();
}

void CShader::setLightFalloffExpressionFromString(const std::string& expressionString)
{
    ensureTemplateCopy();
    _template->setLightFalloffExpressionFromString(expressionString);
}

IShaderLayer::MapType CShader::getLightFalloffCubeMapType()
{
    return _template->getLightFalloffCubeMapType();
}

void CShader::setLightFalloffCubeMapType(IShaderLayer::MapType type)
{
    ensureTemplateCopy();
    _template->setLightFalloffCubeMapType(type);
}

/*
 * Return the light falloff texture (Z dimension).
 */
TexturePtr CShader::lightFalloffImage() {

	// Construct the texture if necessary
	if (!_texLightFalloff) {

		// Create image. If there is no falloff image defined, use the
		// default.
		if (_template->getLightFalloff()) {
			// create the image
			_texLightFalloff = GetTextureManager().getBinding(_template->getLightFalloff());
		}
		else {
			// Find the default light shader in the ShaderSystem and query its
			// falloff texture name.
			std::string defLight = game::current::getValue<std::string>(DEFAULT_LIGHT_PATH);
			MaterialPtr defLightShader = GetShaderSystem()->getMaterial(defLight);

			// Cast to a CShader so we can call getFalloffName().
			CShaderPtr cshaderPtr = std::static_pointer_cast<CShader>(defLightShader);

			// create the image
			_texLightFalloff = GetTextureManager().getBinding(cshaderPtr->_template->getLightFalloff());
		}

	}
	// Return the texture
	return _texLightFalloff;
}

std::string CShader::getName() const
{
	return _name;
}

std::string CShader::getDescription() const
{
	return _template->getDescription();
}

void CShader::setDescription(const std::string& description)
{
    ensureTemplateCopy();
    _template->setDescription(description);
}

bool CShader::IsInUse() const {
	return m_bInUse;
}

void CShader::SetInUse(bool bInUse) {
	m_bInUse = bInUse;
	GetShaderSystem()->activeShadersChangedNotify();
}

int CShader::getMaterialFlags() const
{
	return _template->getMaterialFlags();
}

void CShader::setMaterialFlag(Flags flag)
{
    ensureTemplateCopy();
    _template->setMaterialFlag(flag);
}

void CShader::clearMaterialFlag(Flags flag)
{
    ensureTemplateCopy();
    _template->clearMaterialFlag(flag);
}

bool CShader::IsDefault() const
{
	return _isInternal || _fileInfo.name.empty();
}

// get the cull type
Material::CullType CShader::getCullType() const
{
	return _template->getCullType();
}

void CShader::setCullType(CullType type)
{
    ensureTemplateCopy();
    _template->setCullType(type);
}

ClampType CShader::getClampType() const
{
	return _template->getClampType();
}

void CShader::setClampType(ClampType type)
{
    ensureTemplateCopy();
    _template->setClampType(type);
}

int CShader::getSurfaceFlags() const
{
	return _template->getSurfaceFlags();
}

void CShader::setSurfaceFlag(Material::SurfaceFlags flag)
{
    ensureTemplateCopy();
    _template->setSurfaceFlag(flag);
}

void CShader::clearSurfaceFlag(Material::SurfaceFlags flag)
{
    ensureTemplateCopy();
    _template->clearSurfaceFlag(flag);
}

Material::SurfaceType CShader::getSurfaceType() const
{
	return _template->getSurfaceType();
}

void CShader::setSurfaceType(SurfaceType type)
{
    ensureTemplateCopy();
    _template->setSurfaceType(type);
}

Material::DeformType CShader::getDeformType() const
{
	return _template->getDeformType();
}

IShaderExpression::Ptr CShader::getDeformExpression(std::size_t index)
{
    return _template->getDeformExpression(index);
}

std::string CShader::getDeformDeclName()
{
    return _template->getDeformDeclName();
}

int CShader::getSpectrum() const
{
	return _template->getSpectrum();
}

void CShader::setSpectrum(int spectrum)
{
    ensureTemplateCopy();
    _template->setSpectrum(spectrum);
}

Material::DecalInfo CShader::getDecalInfo() const
{
	return _template->getDecalInfo();
}

Material::Coverage CShader::getCoverage() const
{
	return _template->getCoverage();
}

// get shader file name (ie the file where this one is defined)
const char* CShader::getShaderFileName() const
{
	return _fileInfo.name.c_str();
}

void CShader::setShaderFileName(const std::string& fullPath)
{
    _fileInfo.topDir = os::getDirectory(GlobalFileSystem().findRoot(fullPath));
    _fileInfo.name = os::getRelativePath(fullPath, _fileInfo.topDir);
}

const vfs::FileInfo& CShader::getShaderFileInfo() const
{
    return _fileInfo;
}

std::string CShader::getDefinition()
{
	return _template->getBlockContents();
}

int CShader::getParseFlags() const
{
    return _template->getParseFlags();
}

bool CShader::isModified()
{
    return _template != _originalTemplate;
}

void CShader::setIsModified()
{
    ensureTemplateCopy();
}

void CShader::revertModifications()
{
    _template = _originalTemplate;

    subscribeToTemplateChanges();

    // We need to update that layer reference vector on change
    unrealise();
    realise();

    _sigMaterialModified.emit();
}

sigc::signal<void>& CShader::sig_materialChanged()
{
    return _sigMaterialModified;
}

std::string CShader::getRenderBumpArguments()
{
    return _template->getRenderBumpArguments();
}

std::string CShader::getRenderBumpFlatArguments()
{
    return _template->getRenderBumpFlatArguments();
}

const std::string& CShader::getGuiSurfArgument()
{
    return _template->getGuiSurfArgument();
}

// -----------------------------------------

void CShader::realise() {
	realiseLighting();
}

void CShader::unrealise() {
	unrealiseLighting();
}

// Parse and load image maps for this shader
void CShader::realiseLighting()
{
	for (const auto& layer : _template->getLayers())
	{
		_layers.push_back(layer);
	}
}

void CShader::unrealiseLighting()
{
	_layers.clear();
}

void CShader::setName(const std::string& name)
{
	_name = name;
    _sigMaterialModified.emit();
}

IShaderLayer* CShader::firstLayer() const
{
	if (_layers.empty())
	{
		return nullptr;
	}

	return _layers.front().get();
}

const IShaderLayerVector& CShader::getAllLayers() const
{
    return _layers;
}

std::size_t CShader::addLayer(IShaderLayer::Type type)
{
    ensureTemplateCopy();

    auto newIndex = _template->addLayer(type);

    unrealiseLighting();
    realiseLighting();

    // We need another signal after the realiseLighting call
    _sigMaterialModified.emit();

    return newIndex;
}

void CShader::removeLayer(std::size_t index)
{
    ensureTemplateCopy();

    _template->removeLayer(index);

    unrealiseLighting();
    realiseLighting();

    // We need another signal after the realiseLighting call
    _sigMaterialModified.emit();
}

void CShader::swapLayerPosition(std::size_t first, std::size_t second)
{
    ensureTemplateCopy();

    _template->swapLayerPosition(first, second);

    unrealiseLighting();
    realiseLighting();

    // We need another signal after the realiseLighting call
    _sigMaterialModified.emit();
}

std::size_t CShader::duplicateLayer(std::size_t index)
{
    ensureTemplateCopy();

    auto newIndex = _template->duplicateLayer(index);

    unrealiseLighting();
    realiseLighting();

    // We need another signal after the realiseLighting call
    _sigMaterialModified.emit();

    return newIndex;
}

IEditableShaderLayer::Ptr CShader::getEditableLayer(std::size_t index)
{
    ensureTemplateCopy();

    const auto& layers = _template->getLayers();
    assert(index >= 0 && index < layers.size());

    return layers[index];
}

/* Required Material light type predicates */

bool CShader::isAmbientLight() const {
	return _template->isAmbientLight();
}

bool CShader::isBlendLight() const {
	return _template->isBlendLight();
}

bool CShader::isFogLight() const {
	return _template->isFogLight();
}

bool CShader::isCubicLight() const
{
    return _template->isCubicLight();
}

void CShader::setIsAmbientLight(bool newValue)
{
    ensureTemplateCopy();
    _template->setIsAmbientLight(newValue);
}

void CShader::setIsBlendLight(bool newValue)
{
    ensureTemplateCopy();
    _template->setIsBlendLight(newValue);
}

void CShader::setIsFogLight(bool newValue)
{
    ensureTemplateCopy();
    _template->setIsFogLight(newValue);
}

void CShader::setIsCubicLight(bool newValue)
{
    ensureTemplateCopy();
    _template->setIsCubicLight(newValue);
}


bool CShader::lightCastsShadows() const
{
	int flags = getMaterialFlags();
	return (flags & FLAG_FORCESHADOWS) ||
		   (!isFogLight() && !isAmbientLight() && !isBlendLight() && !(flags & FLAG_NOSHADOWS));
}

bool CShader::surfaceCastsShadow() const
{
	int flags = getMaterialFlags();
	return (flags & FLAG_FORCESHADOWS) || !(flags & FLAG_NOSHADOWS);
}

bool CShader::isDrawn() const
{
	return _template->getLayers().size() > 0 || (getSurfaceFlags() & SURF_ENTITYGUI) /*|| gui != NULL*/;
}

bool CShader::isDiscrete() const
{
	int flags = getSurfaceFlags();
	return (flags & SURF_ENTITYGUI) /*|| gui*/ || getDeformType() != DEFORM_NONE ||
			getSortRequest() == SORT_SUBVIEW || (flags & SURF_DISCRETE);
}

bool CShader::isVisible() const {
	return _visible;
}

void CShader::setVisible(bool visible)
{
	_visible = visible;
}

void CShader::ensureTemplateCopy()
{
    if (_template != _originalTemplate)
    {
        return; // copy is already in place
    }

    // Create a clone of the original template
    _template = _originalTemplate->clone();

    subscribeToTemplateChanges();

    // We need to update that layer reference vector
    // as long as it's there
    unrealise();
    realise();
}

void CShader::commitModifications()
{
    // Overwrite the original template reference, the material is now unmodified again
    _originalTemplate = _template;
}

const ShaderTemplatePtr& CShader::getTemplate()
{
    return _template;
}

void CShader::subscribeToTemplateChanges()
{
    // Disconnect from any signal first
    _templateChanged.disconnect();

    _templateChanged = _template->sig_TemplateChanged().connect([this]()
    {
        _sigMaterialModified.emit();
    });
}

bool CShader::m_lightingEnabled = false;

} // namespace shaders
