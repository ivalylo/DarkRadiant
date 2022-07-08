#include "Doom3SkinCache.h"

#include "itextstream.h"
#include "ideclmanager.h"
#include "module/StaticModule.h"
#include "SkinCreator.h"

namespace skins
{

namespace
{
    // CONSTANTS
    constexpr const char* const SKINS_FOLDER = "skins/";
    constexpr const char* const SKIN_FILE_EXTENSION = ".skin";
}

decl::ISkin::Ptr Doom3SkinCache::findSkin(const std::string& name)
{
    return std::static_pointer_cast<decl::ISkin>(
        GlobalDeclarationManager().findDeclaration(decl::Type::Skin, name)
    );
}

const StringList& Doom3SkinCache::getSkinsForModel(const std::string& model)
{
    static StringList _emptyList;

    auto existing = _modelSkins.find(model);
    return existing != _modelSkins.end() ? existing->second : _emptyList;
}

const StringList& Doom3SkinCache::getAllSkins()
{
    return _allSkins;
}

sigc::signal<void> Doom3SkinCache::signal_skinsReloaded()
{
	return _sigSkinsReloaded;
}

const std::string& Doom3SkinCache::getName() const
{
	static std::string _name(MODULE_MODELSKINCACHE);
	return _name;
}

const StringSet& Doom3SkinCache::getDependencies() const
{
	static StringSet _dependencies;

	if (_dependencies.empty())
    {
		_dependencies.insert(MODULE_DECLMANAGER);
	}

	return _dependencies;
}

void Doom3SkinCache::refresh()
{
    GlobalDeclarationManager().reloadDeclarations();

	_modelSkins.clear();
	_allSkins.clear();
}

void Doom3SkinCache::initialiseModule(const IApplicationContext& ctx)
{
	rMessage() << getName() << "::initialiseModule called" << std::endl;

    GlobalDeclarationManager().registerDeclType("skin", std::make_shared<SkinCreator>());
    GlobalDeclarationManager().registerDeclFolder(decl::Type::Skin, SKINS_FOLDER, SKIN_FILE_EXTENSION);

    _declsReloadedConnection = GlobalDeclarationManager().signal_DeclsReloaded(decl::Type::Skin).connect(
        sigc::mem_fun(this, &Doom3SkinCache::onSkinDeclsReloaded)
    );
}

void Doom3SkinCache::shutdownModule()
{
    _declsReloadedConnection.disconnect();
}

void Doom3SkinCache::onSkinDeclsReloaded()
{
    // Re-build the lists and mappings
    GlobalDeclarationManager().foreachDeclaration(decl::Type::Skin, [&](const decl::IDeclaration::Ptr& decl)
    {
        auto skin = std::static_pointer_cast<Skin>(decl);

        _allSkins.push_back(skin->getDeclName());

        skin->foreachMatchingModel([&](const std::string& modelName)
        {
            auto& matchingSkins = _modelSkins.try_emplace(modelName).first->second;
            matchingSkins.push_back(skin->getDeclName());
        });
    });

    signal_skinsReloaded().emit();
}

// Module instance
module::StaticModuleRegistration<Doom3SkinCache> skinCacheModule;

} // namespace
