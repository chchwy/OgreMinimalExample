
#define WIN32_LEAN_AND_MEAN    // Exclude rarely-used stuff from Windows headers
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// Ogre
#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <OgreConfigFile.h>
#include <OgreArchiveManager.h>
#include <Compositor/OgreCompositorManager2.h>
#include <OgreRenderWindow.h>
#include <OgreWindowEventUtilities.h>
#include <OgreTimer.h>

#include <OgreHlmsManager.h>
#include <OgreHlmsDatablock.h>
#include <Hlms/Unlit/OgreHlmsUnlit.h>
#include <Hlms/Pbs/OgreHlmsPbs.h>
#include <Hlms/Pbs/OgreHlmsPbsDatablock.h>

#include <OgreCamera.h>
#include <OgreMesh.h>
#include <OgreMesh2.h>
#include <OgreMeshManager.h>
#include <OgreMeshManager2.h>
#include <OgreItem.h>

#include "OgreMinimalExample.h"


void addResourceLocation(const std::string& resourcePath,
						 const std::string& resourceType,
						 const std::string& groupName,
						 const bool recursive = false)
{
	auto& manager = Ogre::ResourceGroupManager::getSingleton();
	manager.addResourceLocation(resourcePath, resourceType, groupName, recursive);
}

void SetupResources()
{
	// Load resource paths from config file
	Ogre::ConfigFile cf;
	cf.load("resources2.cfg");

	// Go through all sections & settings in the file
	Ogre::ConfigFile::SectionIterator seci = cf.getSectionIterator();

	while (seci.hasMoreElements())
	{
		Ogre::String secName = seci.peekNextKey();
		Ogre::ConfigFile::SettingsMultiMap *settings = seci.getNext();
		for (auto i = settings->begin(); i != settings->end(); ++i)
		{
			Ogre::String typeName = i->first;
			Ogre::String archName = i->second;
			addResourceLocation(archName, typeName, secName);
		}
	}
}

void registerHlms(Ogre::Root* root)
{
	Ogre::ConfigFile cf;
	cf.load("resources2.cfg");

	Ogre::String dataFolder = "./";

	Ogre::RenderSystem* renderSystem = root->getRenderSystem();

	Ogre::String shaderSyntax = "GLSL";
	if (renderSystem->getName() == "Direct3D11 Rendering Subsystem")
		shaderSyntax = "HLSL";
	else if (renderSystem->getName() == "Metal Rendering Subsystem")
		shaderSyntax = "Metal";

	Ogre::ArchiveManager* arch = Ogre::ArchiveManager::getSingletonPtr();
	Ogre::Archive*archiveLibrary = arch->load(dataFolder + "Hlms/Common/" + shaderSyntax,
		"FileSystem", true);
	Ogre::Archive*archiveLibraryAny = arch->load(dataFolder + "Hlms/Common/Any",
		"FileSystem", true);

	Ogre::ArchiveVec library;
	library.push_back(archiveLibrary);
	library.push_back(archiveLibraryAny);

	Ogre::Archive *archiveUnlit = arch->load(dataFolder + "Hlms/Unlit/" + shaderSyntax,
		"FileSystem", true);

	Ogre::HlmsUnlit* hlmsUnlit = OGRE_NEW Ogre::HlmsUnlit(archiveUnlit, &library);
	root->getHlmsManager()->registerHlms(hlmsUnlit);

	Ogre::Archive* archivePbs = arch->load(dataFolder + "Hlms/Pbs/" + shaderSyntax,
		"FileSystem", true);
	Ogre::HlmsPbs* hlmsPbs = OGRE_NEW Ogre::HlmsPbs(archivePbs, &library);
	root->getHlmsManager()->registerHlms(hlmsPbs);

	if (renderSystem->getName() == "Direct3D11 Rendering Subsystem")
	{
		//Set lower limits 512kb instead of the default 4MB per Hlms in D3D 11.0
		//and below to avoid saturating AMD's discard limit (8MB) or
		//saturate the PCIE bus in some low end machines.
		bool supportsNoOverwriteOnTextureBuffers;
		renderSystem->getCustomAttribute("MapNoOverwriteOnDynamicBufferSRV",
			&supportsNoOverwriteOnTextureBuffers);

		if (!supportsNoOverwriteOnTextureBuffers)
		{
			hlmsPbs->setTextureBufferDefaultSize(512 * 1024);
			hlmsUnlit->setTextureBufferDefaultSize(512 * 1024);
		}
	}
}

void LoadResources(Ogre::Root* root)
{
	registerHlms(root);

	// Initialise, parse scripts etc
	Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();
}

Ogre::SceneManager* chooseSceneManager(Ogre::Root* root)
{
	std::string strSceneManagerName = "SceneManager";

	int numThreads = 1;
	Ogre::InstancingThreadedCullingMethod threadedCullingMethod = Ogre::INSTANCING_CULLING_SINGLETHREAD;

	// Create the SceneManager, in this case a generic one
	Ogre::SceneManager* sceneManager = root->createSceneManager(Ogre::ST_GENERIC,
																numThreads,
																threadedCullingMethod,
																strSceneManagerName);
	//Set sane defaults for proper shadow mapping
	sceneManager->setShadowDirectionalLightExtrusionDistance(500.0f);
	sceneManager->setShadowFarDistance(500.0f);
	return sceneManager;
}

Ogre::CompositorWorkspace* setupCompositor(Ogre::Root* root,
	Ogre::SceneManager* sceneManager,
	Ogre::RenderWindow* renderWindow,
	Ogre::Camera* camera)
{
	Ogre::CompositorManager2 *compositorManager = root->getCompositorManager2();

	return compositorManager->addWorkspace(sceneManager, renderWindow, camera,
		"PbsMaterialsWorkspace", true);
}

void createScene(Ogre::SceneManager* sceneManager)
{
	auto& v1MeshManager = Ogre::v1::MeshManager::getSingleton();
	Ogre::v1::MeshPtr planeMeshV1 = v1MeshManager.createPlane("Plane v1",
		Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
		Ogre::Plane(Ogre::Vector3::UNIT_Y, 1.0f), 50.0f, 50.0f,
		1, 1, true, 1, 4.0f, 4.0f, Ogre::Vector3::UNIT_Z,
		Ogre::v1::HardwareBuffer::HBU_STATIC,
		Ogre::v1::HardwareBuffer::HBU_STATIC);

	Ogre::MeshPtr planeMesh = Ogre::MeshManager::getSingleton().createManual(
		"Plane", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
	planeMesh->importV1(planeMeshV1.get(), true, true, true);
	Ogre::Item* item = sceneManager->createItem(planeMesh, Ogre::SCENE_DYNAMIC);

	Ogre::SceneNode* sceneNode = sceneManager->getRootSceneNode(Ogre::SCENE_DYNAMIC)->
		createChildSceneNode(Ogre::SCENE_DYNAMIC);
	sceneNode->setPosition(0, -1, 0);
	sceneNode->attachObject(item);

	Ogre::SceneNode* rootNode = sceneManager->getRootSceneNode();

	Ogre::Light* light = sceneManager->createLight();
	Ogre::SceneNode *lightNode = rootNode->createChildSceneNode();
	lightNode->attachObject(light);
	light->setPowerScale(1.0f);
	light->setType(Ogre::Light::LT_DIRECTIONAL);
	light->setDirection(Ogre::Vector3(-1, -1, -1).normalisedCopy());

	sceneManager->setAmbientLight(Ogre::ColourValue(0.3f, 0.5f, 0.7f) * 0.1f * 0.75f,
		Ogre::ColourValue(0.6f, 0.45f, 0.3f) * 0.065f * 0.75f,
		-light->getDirection() + Ogre::Vector3::UNIT_Y * 0.2f);

	light = sceneManager->createLight();
	lightNode = rootNode->createChildSceneNode();
	lightNode->attachObject(light);
	light->setDiffuseColour(0.8f, 0.4f, 0.2f); //Warm
	light->setSpecularColour(0.8f, 0.4f, 0.2f);
	light->setPowerScale(Ogre::Math::PI);
	light->setType(Ogre::Light::LT_SPOTLIGHT);
	lightNode->setPosition(-30.0f, 30.0f, 30.0f);
	light->setDirection(Ogre::Vector3(1, -1, -1).normalisedCopy());
	light->setAttenuationBasedOnRadius(50.0f, 0.01f);

	light = sceneManager->createLight();
	lightNode = rootNode->createChildSceneNode();
	lightNode->attachObject(light);
	light->setDiffuseColour(0.2f, 0.4f, 0.8f); //Cold
	light->setSpecularColour(0.2f, 0.4f, 0.8f);
	light->setPowerScale(Ogre::Math::PI);
	light->setType(Ogre::Light::LT_SPOTLIGHT);
	lightNode->setPosition(30.0f, 30.0f, -30.0f);
	light->setDirection(Ogre::Vector3(-1, -1, 1).normalisedCopy());
	light->setAttenuationBasedOnRadius(50.0f, 0.01f);
}

int APIENTRY wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	std::string strPlugins    = "plugins.cfg";
	std::string strConfigFile = "ogre.cfg";
	std::string strLogFile    = "Ogre.log";

	Ogre::Root* root = new Ogre::Root(strPlugins, strConfigFile, strLogFile);

	if (!root->restoreConfig())
	{
		if (!root->showConfigDialog())
		{
			return false;
		}
	}

	Ogre::RenderWindow* renderWindow = root->initialise(true);

	SetupResources();
	LoadResources(root);
	Ogre::SceneManager* sceneManager = chooseSceneManager(root);


	Ogre::Camera* mCamera = sceneManager->createCamera("Main Camera");
	mCamera->setPosition(Ogre::Vector3(0, 5, 30));
	// Look back along -Z
	mCamera->lookAt(Ogre::Vector3(0, 0, 0));
	mCamera->setNearClipDistance(1.f);
	mCamera->setFarClipDistance(1000.0f);
	mCamera->setAutoAspectRatio(true);

	Ogre::CompositorWorkspace* compositor = setupCompositor(root, sceneManager, renderWindow, mCamera);

	createScene(sceneManager);

	Ogre::Timer timer;
	unsigned long startTime = timer.getMicroseconds();

	double timeSinceLast = 1.0 / 60.0;

	while (true)
	{
		Ogre::WindowEventUtilities::messagePump();

		if (!renderWindow->isVisible())
		{
			//Don't burn CPU cycles unnecessary when we're minimized.
			Ogre::Threads::Sleep(500);
		}

		if (renderWindow->isVisible())
			root->renderOneFrame();

		unsigned long endTime = timer.getMicroseconds();
		timeSinceLast = (endTime - startTime) / 1000000.0;
		timeSinceLast = std::min(1.0, timeSinceLast); //Prevent from going haywire.
		startTime = endTime;
	}

	delete root;

	return 0;
}

