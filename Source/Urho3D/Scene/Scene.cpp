//
// Copyright (c) 2008-2022 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "Urho3D/Precompiled.h"

#include "Urho3D/Scene/Scene.h"

#include "Urho3D/Core/Context.h"
#include "Urho3D/Core/CoreEvents.h"
#include "Urho3D/Core/Profiler.h"
#include "Urho3D/Core/WorkQueue.h"
#include "Urho3D/Graphics/Texture2D.h"
#include "Urho3D/IO/Archive.h"
#include "Urho3D/IO/Log.h"
#include "Urho3D/IO/PackageFile.h"
#include "Urho3D/Resource/JSONFile.h"
#include "Urho3D/Resource/ResourceCache.h"
#include "Urho3D/Resource/ResourceEvents.h"
#include "Urho3D/Resource/XMLArchive.h"
#include "Urho3D/Resource/XMLFile.h"
#include "Urho3D/Scene/Component.h"
#include "Urho3D/Scene/ObjectAnimation.h"
#include "Urho3D/Scene/PrefabReference.h"
#include "Urho3D/Scene/PrefabResource.h"
#include "Urho3D/Scene/SceneEvents.h"
#include "Urho3D/Scene/SceneResource.h"
#include "Urho3D/Scene/ShakeComponent.h"
#include "Urho3D/Scene/SplinePath.h"
#include "Urho3D/Scene/UnknownComponent.h"
#include "Urho3D/Scene/ValueAnimation.h"

#include "Urho3D/DebugNew.h"

namespace Urho3D
{

const StringVector Scene::DefaultUpdateEvents = {
    "@SceneForcedUpdate", // E_SCENEFORCEDUPDATE
    "SceneUpdate", // E_SCENEUPDATE
    "AttributeAnimationUpdate", // E_ATTRIBUTEANIMATIONUPDATE
    "SceneSubsystemUpdate", // E_SCENESUBSYSTEMUPDATE
    "ScenePostUpdate", // E_SCENEPOSTUPDATE
    "@SceneForcedPostUpdate", // E_SCENEFORCEDPOSTUPDATE
};

Scene::Scene(Context* context) :
    Node(context),
    replicatedNodeID_(FIRST_REPLICATED_ID),
    replicatedComponentID_(FIRST_REPLICATED_ID),
    checksum_(0),
    asyncLoadingMs_(5),
    timeScale_(1.0f),
    elapsedTime_(0),
    updateEnabled_(true),
    asyncLoading_(false),
    threadedUpdate_(false),
    lightmaps_(Texture2D::GetTypeStatic())
{
    SetUpdateEvents(DefaultUpdateEvents);

    // Assign an ID to self so that nodes can refer to this node as a parent
    SetID(GetFreeNodeID());
    NodeAdded(this);

    SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(Scene, HandleUpdate));
    SubscribeToEvent(E_RESOURCEBACKGROUNDLOADED, URHO3D_HANDLER(Scene, HandleResourceBackgroundLoaded));
}

Scene::~Scene()
{
    // Remove root-level components first, so that scene subsystems such as the octree destroy themselves. This will speed up
    // the removal of child nodes' components
    RemoveAllComponents();
    RemoveAllChildren();

    // Remove scene reference and owner from all nodes that still exist
    for (auto i = replicatedNodes_.begin(); i != replicatedNodes_.end(); ++i)
        i->second->ResetScene();
}

void Scene::RegisterObject(Context* context)
{
    context->AddFactoryReflection<Scene>();

    URHO3D_ACCESSOR_ATTRIBUTE("Name", GetName, SetName, ea::string, EMPTY_STRING, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Time Scale", GetTimeScale, SetTimeScale, float, 1.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Elapsed Time", GetElapsedTime, SetElapsedTime, float, 0.0f, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Next Node ID", unsigned, replicatedNodeID_, FIRST_REPLICATED_ID, AM_DEFAULT | AM_NOEDIT);
    URHO3D_ATTRIBUTE("Next Component ID", unsigned, replicatedComponentID_, FIRST_REPLICATED_ID, AM_DEFAULT | AM_NOEDIT);
    URHO3D_ATTRIBUTE("Variables", StringVariantMap, vars_, Variant::emptyStringVariantMap, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Update Events", GetUpdateEvents, SetUpdateEvents, StringVector, DefaultUpdateEvents, AM_DEFAULT);
    URHO3D_ATTRIBUTE_EX("Lightmaps", ResourceRefList, lightmaps_, ReloadLightmaps, ResourceRefList(Texture2D::GetTypeStatic()), AM_DEFAULT);
}

bool Scene::CreateComponentIndex(StringHash componentType)
{
    if (!IsEmpty())
    {
        URHO3D_LOGERROR("Component Index may be created only for empty Scene");
        return false;
    }

    indexedComponentTypes_.push_back(componentType);
    componentIndexes_.emplace_back();
    return true;
}

const SceneComponentIndex& Scene::GetComponentIndex(StringHash componentType)
{
    static const SceneComponentIndex emptyIndex;
    if (auto index = GetMutableComponentIndex(componentType))
        return *index;
    return emptyIndex;
}

void Scene::SerializeInBlock(
    Archive& archive, bool serializeTemporary, PrefabSaveFlags saveFlags, PrefabLoadFlags loadFlags)
{
    Node::SerializeInBlock(archive, serializeTemporary, saveFlags, loadFlags | PrefabLoadFlag::SkipApplyAttributes);

    int placeholder{};
    SerializeOptionalValue(archive, "auxiliary", placeholder, AlwaysSerialize{},
        [&](Archive& archive, const char* name, int&)
    {
        ea::unordered_map<ea::string, Component*> components;
        for (Component* component : GetComponents())
        {
            if (component->HasAuxiliaryData())
                components[component->GetTypeName()] = component;
        }

        SerializeMap(archive, name, components, "component",
            [](Archive& archive, const char* name, auto& value)
        {
            if constexpr (ea::is_same_v<ea::remove_reference_t<decltype(value)>, ea::string>)
                SerializeValue(archive, name, value);
            else if (value)
            {
                auto block = archive.OpenSafeUnorderedBlock(name);
                ConsumeArchiveException([&] { value->SerializeAuxiliaryData(archive); });
            }
        }, false);
    });

    if (archive.IsInput())
        ApplyAttributes();

    if (!archive.GetName().empty())
        fileName_ = archive.GetName();
    if (archive.GetChecksum() != 0)
        checksum_ = archive.GetChecksum();
}

void Scene::SerializeInBlock(Archive& archive)
{
    const bool compactSave = !archive.IsHumanReadable();
    const PrefabSaveFlags saveFlags =
        compactSave ? PrefabSaveFlag::CompactAttributeNames : PrefabSaveFlag::EnumsAsStrings;

    SerializeInBlock(archive, false, saveFlags, PrefabLoadFlag::None);
}

bool Scene::Load(Deserializer& source)
{
    URHO3D_PROFILE("LoadScene");

    StopAsyncLoading();

    constexpr BinaryMagic sceneBinaryMagic{{'U', 'S', 'C', 'N'}};

    const InternalResourceFormat format = PeekResourceFormat(source, sceneBinaryMagic);

    switch (format)
    {
    case InternalResourceFormat::Json: return LoadJSON(source);
    case InternalResourceFormat::Xml: return LoadXML(source);
    case InternalResourceFormat::Binary: break; // Fall through to binary load.
    default: URHO3D_LOGERROR(source.GetName() + " is not a valid scene file"); return false;
    }

    URHO3D_LOGINFO("Loading scene from " + source.GetName());

    Clear();

    // Load the whole scene, then perform post-load if successfully loaded
    if (Node::Load(source))
    {
        FinishLoading(&source);
        return true;
    }
    else
        return false;
}

bool Scene::Save(Serializer& dest) const
{
    URHO3D_PROFILE("SaveScene");

    // Write ID first
    if (!dest.WriteFileID("USCN"))
    {
        URHO3D_LOGERROR("Could not save scene, writing to stream failed");
        return false;
    }

    auto* ptr = dynamic_cast<Deserializer*>(&dest);
    if (ptr)
        URHO3D_LOGINFO("Saving scene to " + ptr->GetName());

    if (Node::Save(dest))
    {
        FinishSaving(&dest);
        return true;
    }
    else
        return false;
}

bool Scene::LoadXML(const XMLElement& source)
{
    URHO3D_PROFILE("LoadSceneXML");

    StopAsyncLoading();

    if (source.GetName() == SceneResource::GetXmlRootName())
    {
        XMLInputArchive archive{context_, source, source.GetFile()};
        ArchiveBlock block = archive.OpenUnorderedBlock(SceneResource::GetXmlRootName());
        SerializeInBlock(archive, false, PrefabSaveFlag::None, PrefabLoadFlag::None);
        return true;
    }
    else
    {
        // Load the whole scene, then perform post-load if successfully loaded
        // Note: the scene filename and checksum can not be set, as we only used an XML element
        if (Node::LoadXML(source))
        {
            FinishLoading(nullptr);
            return true;
        }
        throw ArchiveException("Cannot load Scene from legacy XML format");
    }
    return false;
}

bool Scene::LoadJSON(const JSONValue& source)
{
    URHO3D_PROFILE("LoadSceneJSON");

    StopAsyncLoading();

    // Load the whole scene, then perform post-load if successfully loaded
    // Note: the scene filename and checksum can not be set, as we only used an XML element
    if (Node::LoadJSON(source))
    {
        FinishLoading(nullptr);
        return true;
    }
    else
        return false;
}

void Scene::ResetLightmaps()
{
    lightmaps_.names_.clear();
    lightmapTextures_.clear();
}

void Scene::AddLightmap(const ea::string& lightmapTextureName)
{
    lightmaps_.names_.push_back(lightmapTextureName);
    auto cache = GetSubsystem<ResourceCache>();
    lightmapTextures_.emplace_back(cache->GetResource<Texture2D>(lightmapTextureName));
}

void Scene::ReloadLightmaps()
{
    auto cache = GetSubsystem<ResourceCache>();
    lightmapTextures_.clear();
    for (const ea::string& lightmapTextureName : lightmaps_.names_)
        lightmapTextures_.emplace_back(cache->GetResource<Texture2D>(lightmapTextureName));
}

Texture2D* Scene::GetLightmapTexture(unsigned index) const
{
    return index < lightmapTextures_.size() ? lightmapTextures_[index] : nullptr;
}

void Scene::SetUpdateEvents(const StringVector& events)
{
    updateEvents_ = events;

    cookedUpdateEvents_.clear();
    for (const ea::string& event : events)
    {
        if (event.empty())
            continue;

        const bool isForced = event[0] == '@';
        const ea::string_view eventName = isForced ? ea::string_view{event}.substr(1) : event;
        cookedUpdateEvents_.emplace_back(StringHash{eventName}, isForced);
    }
}

bool Scene::LoadXML(Deserializer& source)
{
    URHO3D_PROFILE("LoadSceneXML");

    StopAsyncLoading();

    SharedPtr<XMLFile> xml(MakeShared<XMLFile>(context_));
    if (!xml->Load(source))
        return false;

    URHO3D_LOGINFO("Loading scene from " + source.GetName());

    Clear();

    return LoadXML(xml->GetRoot());
}

bool Scene::LoadJSON(Deserializer& source)
{
    URHO3D_PROFILE("LoadSceneJSON");

    StopAsyncLoading();

    SharedPtr<JSONFile> json(MakeShared<JSONFile>(context_));
    if (!json->Load(source))
        return false;

    URHO3D_LOGINFO("Loading scene from " + source.GetName());

    Clear();

    if (Node::LoadJSON(json->GetRoot()))
    {
        FinishLoading(&source);
        return true;
    }
    else
        return false;
}

bool Scene::SaveXML(Serializer& dest, const ea::string& indentation) const
{
    URHO3D_PROFILE("SaveSceneXML");

    SharedPtr<XMLFile> xml(MakeShared<XMLFile>(context_));
    XMLElement rootElem = xml->CreateRoot("scene");
    if (!SaveXML(rootElem))
        return false;

    auto* ptr = dynamic_cast<Deserializer*>(&dest);
    if (ptr)
        URHO3D_LOGINFO("Saving scene to " + ptr->GetName());

    if (xml->Save(dest, indentation))
    {
        FinishSaving(&dest);
        return true;
    }
    else
        return false;
}

bool Scene::SaveJSON(Serializer& dest, const ea::string& indentation) const
{
    URHO3D_PROFILE("SaveSceneJSON");

    SharedPtr<JSONFile> json(MakeShared<JSONFile>(context_));
    JSONValue rootVal;
    if (!SaveJSON(rootVal))
        return false;

    auto* ptr = dynamic_cast<Deserializer*>(&dest);
    if (ptr)
        URHO3D_LOGINFO("Saving scene to " + ptr->GetName());

    json->GetRoot() = rootVal;

    if (json->Save(dest, indentation))
    {
        FinishSaving(&dest);
        return true;
    }
    else
        return false;
}

bool Scene::LoadAsync(AbstractFilePtr file, LoadMode mode)
{
    if (!file)
    {
        URHO3D_LOGERROR("Null file for async loading");
        return false;
    }

    StopAsyncLoading();

    // Check ID
    bool isSceneFile = file->ReadFileID() == "USCN";
    if (!isSceneFile)
    {
        // In resource load mode can load also object prefabs, which have no identifier
        if (mode > LOAD_RESOURCES_ONLY)
        {
            URHO3D_LOGERROR(file->GetName() + " is not a valid scene file");
            return false;
        }
        else
            file->Seek(0);
    }

    if (mode > LOAD_RESOURCES_ONLY)
    {
        URHO3D_LOGINFO("Loading scene from " + file->GetName());
        Clear();
    }

    asyncLoading_ = true;
    asyncProgress_.file_ = file;
    asyncProgress_.mode_ = mode;
    asyncProgress_.loadedNodes_ = asyncProgress_.totalNodes_ = asyncProgress_.loadedResources_ = asyncProgress_.totalResources_ = 0;
    asyncProgress_.resources_.clear();

    if (mode > LOAD_RESOURCES_ONLY)
    {
        // Preload resources if appropriate, then return to the original position for loading the scene content
        if (mode != LOAD_SCENE)
        {
            URHO3D_PROFILE("FindResourcesToPreload");

            unsigned currentPos = file->GetPosition();
            PreloadResources(file, isSceneFile);
            file->Seek(currentPos);
        }

        // Store own old ID for resolving possible root node references
        unsigned nodeID = file->ReadUInt();
        resolver_.AddNode(nodeID, this);

        // Load root level components first
        if (!Node::Load(*file, resolver_, false))
        {
            StopAsyncLoading();
            return false;
        }

        // Then prepare to load child nodes in the async updates
        asyncProgress_.totalNodes_ = file->ReadVLE();
    }
    else
    {
        URHO3D_PROFILE("FindResourcesToPreload");

        URHO3D_LOGINFO("Preloading resources from " + file->GetName());
        PreloadResources(file, isSceneFile);
    }

    return true;
}

bool Scene::LoadAsyncXML(AbstractFilePtr file, LoadMode mode)
{
    if (!file)
    {
        URHO3D_LOGERROR("Null file for async loading");
        return false;
    }

    StopAsyncLoading();

    SharedPtr<XMLFile> xml(MakeShared<XMLFile>(context_));
    if (!xml->Load(*file))
        return false;

    if (mode > LOAD_RESOURCES_ONLY)
    {
        URHO3D_LOGINFO("Loading scene from " + file->GetName());
        Clear();
    }

    asyncLoading_ = true;
    asyncProgress_.xmlFile_ = xml;
    asyncProgress_.file_ = file;
    asyncProgress_.mode_ = mode;
    asyncProgress_.loadedNodes_ = asyncProgress_.totalNodes_ = asyncProgress_.loadedResources_ = asyncProgress_.totalResources_ = 0;
    asyncProgress_.resources_.clear();

    if (mode > LOAD_RESOURCES_ONLY)
    {
        XMLElement rootElement = xml->GetRoot();

        // Preload resources if appropriate
        if (mode != LOAD_SCENE)
        {
            URHO3D_PROFILE("FindResourcesToPreload");

            PreloadResourcesXML(rootElement);
        }

        // Store own old ID for resolving possible root node references
        unsigned nodeID = rootElement.GetUInt("id");
        resolver_.AddNode(nodeID, this);

        // Load the root level components first
        if (!Node::LoadXML(rootElement, resolver_, false))
            return false;

        // Then prepare for loading all root level child nodes in the async update
        XMLElement childNodeElement = rootElement.GetChild("node");
        asyncProgress_.xmlElement_ = childNodeElement;

        // Count the amount of child nodes
        while (childNodeElement)
        {
            ++asyncProgress_.totalNodes_;
            childNodeElement = childNodeElement.GetNext("node");
        }
    }
    else
    {
        URHO3D_PROFILE("FindResourcesToPreload");

        URHO3D_LOGINFO("Preloading resources from " + file->GetName());
        PreloadResourcesXML(xml->GetRoot());
    }

    return true;
}

bool Scene::LoadAsyncJSON(AbstractFilePtr file, LoadMode mode)
{
    if (!file)
    {
        URHO3D_LOGERROR("Null file for async loading");
        return false;
    }

    StopAsyncLoading();

    SharedPtr<JSONFile> json(MakeShared<JSONFile>(context_));
    if (!json->Load(*file))
        return false;

    if (mode > LOAD_RESOURCES_ONLY)
    {
        URHO3D_LOGINFO("Loading scene from " + file->GetName());
        Clear();
    }

    asyncLoading_ = true;
    asyncProgress_.jsonFile_ = json;
    asyncProgress_.file_ = file;
    asyncProgress_.mode_ = mode;
    asyncProgress_.loadedNodes_ = asyncProgress_.totalNodes_ = asyncProgress_.loadedResources_ = asyncProgress_.totalResources_ = 0;
    asyncProgress_.resources_.clear();

    if (mode > LOAD_RESOURCES_ONLY)
    {
        JSONValue rootVal = json->GetRoot();

        // Preload resources if appropriate
        if (mode != LOAD_SCENE)
        {
            URHO3D_PROFILE("FindResourcesToPreload");

            PreloadResourcesJSON(rootVal);
        }

        // Store own old ID for resolving possible root node references
        unsigned nodeID = rootVal.Get("id").GetUInt();
        resolver_.AddNode(nodeID, this);

        // Load the root level components first
        if (!Node::LoadJSON(rootVal, resolver_, false))
            return false;

        // Then prepare for loading all root level child nodes in the async update
        JSONArray childrenArray = rootVal.Get("children").GetArray();
        asyncProgress_.jsonIndex_ = 0;

        // Count the amount of child nodes
        asyncProgress_.totalNodes_ = childrenArray.size();
    }
    else
    {
        URHO3D_PROFILE("FindResourcesToPreload");

        URHO3D_LOGINFO("Preloading resources from " + file->GetName());
        PreloadResourcesJSON(json->GetRoot());
    }

    return true;
}

void Scene::StopAsyncLoading()
{
    asyncLoading_ = false;
    asyncProgress_.file_.Reset();
    asyncProgress_.xmlFile_.Reset();
    asyncProgress_.jsonFile_.Reset();
    asyncProgress_.xmlElement_ = XMLElement::EMPTY;
    asyncProgress_.jsonIndex_ = 0;
    asyncProgress_.resources_.clear();
    resolver_.Reset();
}

Node* Scene::Instantiate(Deserializer& source, const Vector3& position, const Quaternion& rotation)
{
    URHO3D_PROFILE("Instantiate");

    SceneResolver resolver;
    unsigned nodeID = source.ReadUInt();
    // Rewrite IDs when instantiating
    Node* node = CreateChild(0);
    resolver.AddNode(nodeID, node);
    if (node->Load(source, resolver, true, true))
    {
        resolver.Resolve();
        node->SetTransform(position, rotation);
        node->ApplyAttributes();
        return node;
    }
    else
    {
        node->Remove();
        return nullptr;
    }
}

Node* Scene::InstantiateXML(const XMLElement& source, const Vector3& position, const Quaternion& rotation)
{
    URHO3D_PROFILE("InstantiateXML");

    SceneResolver resolver;
    unsigned nodeID = source.GetUInt("id");
    // Rewrite IDs when instantiating
    Node* node = CreateChild(0);
    resolver.AddNode(nodeID, node);
    if (node->LoadXML(source, resolver, true, true))
    {
        resolver.Resolve();
        node->SetTransform(position, rotation);
        node->ApplyAttributes();
        return node;
    }
    else
    {
        node->Remove();
        return nullptr;
    }
}

Node* Scene::InstantiateJSON(const JSONValue& source, const Vector3& position, const Quaternion& rotation)
{
    URHO3D_PROFILE("InstantiateJSON");

    SceneResolver resolver;
    unsigned nodeID = source.Get("id").GetUInt();
    // Rewrite IDs when instantiating
    Node* node = CreateChild(0);
    resolver.AddNode(nodeID, node);
    if (node->LoadJSON(source, resolver, true, true))
    {
        resolver.Resolve();
        node->SetTransform(position, rotation);
        node->ApplyAttributes();
        return node;
    }
    else
    {
        node->Remove();
        return nullptr;
    }
}

Node* Scene::InstantiateXML(Deserializer& source, const Vector3& position, const Quaternion& rotation)
{
    SharedPtr<XMLFile> xml(MakeShared<XMLFile>(context_));
    if (!xml->Load(source))
        return nullptr;

    return InstantiateXML(xml->GetRoot(), position, rotation);
}

Node* Scene::InstantiateJSON(Deserializer& source, const Vector3& position, const Quaternion& rotation)
{
    SharedPtr<JSONFile> json(MakeShared<JSONFile>(context_));
    if (!json->Load(source))
        return nullptr;

    return InstantiateJSON(json->GetRoot(), position, rotation);
}

void Scene::Clear()
{
    StopAsyncLoading();

    RemoveChildren(true);
    RemoveComponents();

    SetName(EMPTY_STRING);
    SetUpdateEvents(DefaultUpdateEvents);
    fileName_.clear();
    checksum_ = 0;

    // Reset ID generators
    replicatedNodeID_ = FIRST_REPLICATED_ID;
    replicatedComponentID_ = FIRST_REPLICATED_ID;
}

void Scene::SetUpdateEnabled(bool enable)
{
    if (updateEnabled_ != enable)
    {
        updateEnabled_ = enable;

        using namespace SceneUpdateChanged;

        VariantMap& eventData = GetEventDataMap();
        eventData[P_SCENE] = this;
        eventData[P_UPDATEENABLED] = updateEnabled_;

        // Notify subscribers about scene updates been enabled.
        SendEvent(E_SCENEUPDATESCHANGED, eventData);
    }
}

void Scene::SetTimeScale(float scale)
{
    timeScale_ = Max(scale, M_EPSILON);
}

void Scene::SetAsyncLoadingMs(int ms)
{
    asyncLoadingMs_ = Max(ms, 1);
}

void Scene::SetElapsedTime(float time)
{
    elapsedTime_ = time;
}

void Scene::AddRequiredPackageFile(PackageFile* package)
{
    // Do not add packages that failed to load
    if (!package || !package->GetNumFiles())
        return;

    requiredPackageFiles_.push_back(SharedPtr<PackageFile>(package));
}

void Scene::ClearRequiredPackageFiles()
{
    requiredPackageFiles_.clear();
}

bool Scene::IsEmpty(bool ignoreComponents) const
{
    const bool noNodesExceptSelf = replicatedNodes_.size() == 1;
    const bool noComponents = replicatedComponents_.size() == 0;
    return noNodesExceptSelf && (noComponents || ignoreComponents);
}

Node* Scene::GetNode(unsigned id) const
{
    auto i = replicatedNodes_.find(id);
    return i != replicatedNodes_.end() ? i->second : nullptr;
}

bool Scene::GetNodesWithTag(ea::vector<Node*>& dest, const ea::string& tag) const
{
    dest.clear();
    auto it = taggedNodes_.find(tag);
    if (it != taggedNodes_.end())
    {
        dest = it->second;
        return true;
    }
    else
        return false;
}

Component* Scene::GetComponent(unsigned id) const
{
    auto i = replicatedComponents_.find(id);
    return i != replicatedComponents_.end() ? i->second : nullptr;
}

float Scene::GetAsyncProgress() const
{
    return !asyncLoading_ || asyncProgress_.totalNodes_ + asyncProgress_.totalResources_ == 0 ? 1.0f :
        (float)(asyncProgress_.loadedNodes_ + asyncProgress_.loadedResources_) /
        (float)(asyncProgress_.totalNodes_ + asyncProgress_.totalResources_);
}

void Scene::Update(float timeStep)
{
    // TODO: Revisit async loading. Do we want to load paused Scene?
    if (updateEnabled_ && asyncLoading_)
    {
        UpdateAsyncLoading();
        // If only preloading resources, scene update can continue
        if (asyncProgress_.mode_ > LOAD_RESOURCES_ONLY)
            return;
    }

    URHO3D_PROFILE("UpdateScene");

    timeStep *= timeScale_;

    VariantMap& eventData = GetEventDataMap();
    eventData[SceneUpdate::P_SCENE] = this;
    eventData[SceneUpdate::P_TIMESTEP] = timeStep;

    for (const auto& [eventId, isForced] : cookedUpdateEvents_)
    {
        if (updateEnabled_ || isForced)
            SendEvent(eventId, eventData);
    }

    if (updateEnabled_)
    {
        // Note: using a float for elapsed time accumulation is inherently inaccurate. The purpose of this value is
        // primarily to update material animation effects, as it is available to shaders. It can be reset by calling
        // SetElapsedTime()
        elapsedTime_ += timeStep;
    }
}

void Scene::BeginThreadedUpdate()
{
    // Check the work queue subsystem whether it actually has created worker threads. If not, do not enter threaded mode.
    if (GetSubsystem<WorkQueue>()->IsMultithreaded())
        threadedUpdate_ = true;
}

void Scene::EndThreadedUpdate()
{
    if (!threadedUpdate_)
        return;

    threadedUpdate_ = false;

    if (!delayedDirtyComponents_.empty())
    {
        URHO3D_PROFILE("EndThreadedUpdate");

        for (auto i = delayedDirtyComponents_.begin(); i !=
            delayedDirtyComponents_.end(); ++i)
            (*i)->OnMarkedDirty((*i)->GetNode());
        delayedDirtyComponents_.clear();
    }
}

void Scene::DelayedMarkedDirty(Component* component)
{
    MutexLock lock(sceneMutex_);
    delayedDirtyComponents_.push_back(component);
}

unsigned Scene::GetFreeNodeID()
{
    for (;;)
    {
        unsigned ret = replicatedNodeID_;
        if (replicatedNodeID_ < LAST_REPLICATED_ID)
            ++replicatedNodeID_;
        else
            replicatedNodeID_ = FIRST_REPLICATED_ID;

        if (!replicatedNodes_.contains(ret))
            return ret;
    }
}

unsigned Scene::GetFreeComponentID()
{
    for (;;)
    {
        unsigned ret = replicatedComponentID_;
        if (replicatedComponentID_ < LAST_REPLICATED_ID)
            ++replicatedComponentID_;
        else
            replicatedComponentID_ = FIRST_REPLICATED_ID;

        if (!replicatedComponents_.contains(ret))
            return ret;
    }
}

void Scene::NodeAdded(Node* node)
{
    if (!node || node->GetScene() == this)
        return;

    // Remove from old scene first
    Scene* oldScene = node->GetScene();
    if (oldScene)
        oldScene->NodeRemoved(node);

    node->SetScene(this);

    // If the new node has an ID of zero (default), assign a replicated ID now
    unsigned id = node->GetID();
    if (!id)
    {
        id = GetFreeNodeID();
        node->SetID(id);
    }

    // If node with same ID exists, remove the scene reference from it and overwrite with the new node
    auto i = replicatedNodes_.find(id);
    if (i != replicatedNodes_.end() && i->second != node)
    {
        URHO3D_LOGWARNING("Overwriting node with ID " + ea::to_string(id));
        NodeRemoved(i->second);
    }

    replicatedNodes_[id] = node;

    // Cache tag if already tagged.
    if (!node->GetTags().empty())
    {
        const StringVector& tags = node->GetTags();
        for (unsigned i = 0; i < tags.size(); ++i)
            taggedNodes_[tags[i]].push_back(node);
    }

    // Add already created components and child nodes now
    const ea::vector<SharedPtr<Component> >& components = node->GetComponents();
    for (auto i = components.begin(); i != components.end(); ++i)
        ComponentAdded(*i);
    const ea::vector<SharedPtr<Node> >& children = node->GetChildren();
    for (auto i = children.begin(); i != children.end(); ++i)
        NodeAdded(*i);
}

void Scene::NodeTagAdded(Node* node, const ea::string& tag)
{
    taggedNodes_[tag].push_back(node);
}

void Scene::NodeTagRemoved(Node* node, const ea::string& tag)
{
    taggedNodes_[tag].erase_first(node);
}

void Scene::NodeRemoved(Node* node)
{
    if (!node || node->GetScene() != this)
        return;

    unsigned id = node->GetID();
    replicatedNodes_.erase(id);

    node->ResetScene();

    // Remove node from tag cache
    if (!node->GetTags().empty())
    {
        const StringVector& tags = node->GetTags();
        for (unsigned i = 0; i < tags.size(); ++i)
            taggedNodes_[tags[i]].erase_first(node);
    }

    // Remove components and child nodes as well
    const ea::vector<SharedPtr<Component> >& components = node->GetComponents();
    for (auto i = components.begin(); i != components.end(); ++i)
        ComponentRemoved(*i);
    const ea::vector<SharedPtr<Node> >& children = node->GetChildren();
    for (auto i = children.begin(); i != children.end(); ++i)
        NodeRemoved(*i);
}

void Scene::ComponentAdded(Component* component)
{
    if (!component)
        return;

    unsigned id = component->GetID();

    // If the new component has an ID of zero (default), assign a replicated ID now
    if (!id)
    {
        id = GetFreeComponentID();
        component->SetID(id);
    }

    auto i = replicatedComponents_.find(id);
    if (i != replicatedComponents_.end() && i->second != component)
    {
        URHO3D_LOGWARNING("Overwriting component with ID " + ea::to_string(id));
        ComponentRemoved(i->second);
    }

    replicatedComponents_[id] = component;

    component->OnSceneSet(nullptr, this);

    if (auto index = GetMutableComponentIndex(component->GetType()))
        index->insert(component);
}

void Scene::ComponentRemoved(Component* component)
{
    if (!component)
        return;

    if (auto index = GetMutableComponentIndex(component->GetType()))
        index->erase(component);

    unsigned id = component->GetID();
    replicatedComponents_.erase(id);

    component->SetID(0);
    component->OnSceneSet(this, nullptr);
}

void Scene::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
    if (manualUpdate_)
        return;

    using namespace Update;
    Update(eventData[P_TIMESTEP].GetFloat());
}

void Scene::HandleResourceBackgroundLoaded(StringHash eventType, VariantMap& eventData)
{
    using namespace ResourceBackgroundLoaded;

    if (asyncLoading_)
    {
        auto* resource = static_cast<Resource*>(eventData[P_RESOURCE].GetPtr());
        if (asyncProgress_.resources_.contains(resource->GetNameHash()))
        {
            asyncProgress_.resources_.erase(resource->GetNameHash());
            ++asyncProgress_.loadedResources_;
        }
    }
}

void Scene::UpdateAsyncLoading()
{
    URHO3D_PROFILE("UpdateAsyncLoading");

    // If resources left to load, do not load nodes yet
    if (asyncProgress_.loadedResources_ < asyncProgress_.totalResources_)
        return;

    HiresTimer asyncLoadTimer;

    for (;;)
    {
        if (asyncProgress_.loadedNodes_ >= asyncProgress_.totalNodes_)
        {
            FinishAsyncLoading();
            return;
        }


        // Read one child node with its full sub-hierarchy either from binary, JSON, or XML
        /// \todo Works poorly in scenes where one root-level child node contains all content
        if (asyncProgress_.xmlFile_)
        {
            unsigned nodeID = asyncProgress_.xmlElement_.GetUInt("id");
            Node* newNode = CreateChild(nodeID);
            resolver_.AddNode(nodeID, newNode);
            newNode->LoadXML(asyncProgress_.xmlElement_, resolver_);
            asyncProgress_.xmlElement_ = asyncProgress_.xmlElement_.GetNext("node");
        }
        else if (asyncProgress_.jsonFile_) // Load from JSON
        {
            const JSONValue& childValue = asyncProgress_.jsonFile_->GetRoot().Get("children").GetArray().at(
                asyncProgress_.jsonIndex_);

            unsigned nodeID =childValue.Get("id").GetUInt();
            Node* newNode = CreateChild(nodeID);
            resolver_.AddNode(nodeID, newNode);
            newNode->LoadJSON(childValue, resolver_);
            ++asyncProgress_.jsonIndex_;
        }
        else // Load from binary
        {
            unsigned nodeID = asyncProgress_.file_->ReadUInt();
            Node* newNode = CreateChild(nodeID);
            resolver_.AddNode(nodeID, newNode);
            newNode->Load(*asyncProgress_.file_, resolver_);
        }

        ++asyncProgress_.loadedNodes_;

        // Break if time limit exceeded, so that we keep sufficient FPS
        if (asyncLoadTimer.GetUSec(false) >= asyncLoadingMs_ * 1000LL)
            break;
    }

    using namespace AsyncLoadProgress;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_SCENE] = this;
    eventData[P_PROGRESS] = GetAsyncProgress();
    eventData[P_LOADEDNODES] = asyncProgress_.loadedNodes_;
    eventData[P_TOTALNODES] = asyncProgress_.totalNodes_;
    eventData[P_LOADEDRESOURCES] = asyncProgress_.loadedResources_;
    eventData[P_TOTALRESOURCES] = asyncProgress_.totalResources_;
    SendEvent(E_ASYNCLOADPROGRESS, eventData);
}

void Scene::FinishAsyncLoading()
{
    if (asyncProgress_.mode_ > LOAD_RESOURCES_ONLY)
    {
        resolver_.Resolve();
        ApplyAttributes();
        FinishLoading(asyncProgress_.file_);
    }

    StopAsyncLoading();

    using namespace AsyncLoadFinished;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_SCENE] = this;
    SendEvent(E_ASYNCLOADFINISHED, eventData);
}

void Scene::FinishLoading(Deserializer* source)
{
    if (source)
    {
        // TODO: This name is not full file name, it's resource name. Consider changing it.
        fileName_ = source->GetName();
        checksum_ = source->GetChecksum();
    }
}

void Scene::FinishSaving(Serializer* dest) const
{
    auto* ptr = dynamic_cast<Deserializer*>(dest);
    if (ptr)
    {
        // TODO: This name is not full file name, it's resource name. Consider changing it.
        fileName_ = ptr->GetName();
        checksum_ = ptr->GetChecksum();
    }
}

void Scene::PreloadResources(AbstractFilePtr file, bool isSceneFile)
{
    // If not threaded, can not background load resources, so rather load synchronously later when needed
#ifdef URHO3D_THREADING
    auto* cache = GetSubsystem<ResourceCache>();

    // Read node ID (not needed)
    /*unsigned nodeID = */file->ReadUInt();

    // Read Node or Scene attributes; these do not include any resources
    const ea::vector<AttributeInfo>* attributes = context_->GetAttributes(isSceneFile ? Scene::GetTypeStatic() : Node::GetTypeStatic());
    assert(attributes);

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (!(attr.mode_ & AM_FILE))
            continue;
        /*Variant varValue = */file->ReadVariant(attr.type_);
    }

    // Read component attributes
    unsigned numComponents = file->ReadVLE();
    for (unsigned i = 0; i < numComponents; ++i)
    {
        VectorBuffer compBuffer(*file, file->ReadVLE());
        StringHash compType = compBuffer.ReadStringHash();
        // Read component ID (not needed)
        /*unsigned compID = */compBuffer.ReadUInt();

        attributes = context_->GetAttributes(compType);
        if (attributes)
        {
            for (unsigned j = 0; j < attributes->size(); ++j)
            {
                const AttributeInfo& attr = attributes->at(j);
                if (!(attr.mode_ & AM_FILE))
                    continue;
                Variant varValue = compBuffer.ReadVariant(attr.type_);
                if (attr.type_ == VAR_RESOURCEREF)
                {
                    const ResourceRef& ref = varValue.GetResourceRef();
                    // Sanitate resource name beforehand so that when we get the background load event, the name matches exactly
                    ea::string name = cache->SanitateResourceName(ref.name_);
                    bool success = cache->BackgroundLoadResource(ref.type_, name);
                    if (success)
                    {
                        ++asyncProgress_.totalResources_;
                        asyncProgress_.resources_.insert(StringHash(name));
                    }
                }
                else if (attr.type_ == VAR_RESOURCEREFLIST)
                {
                    const ResourceRefList& refList = varValue.GetResourceRefList();
                    for (unsigned k = 0; k < refList.names_.size(); ++k)
                    {
                        ea::string name = cache->SanitateResourceName(refList.names_[k]);
                        bool success = cache->BackgroundLoadResource(refList.type_, name);
                        if (success)
                        {
                            ++asyncProgress_.totalResources_;
                            asyncProgress_.resources_.insert(StringHash(name));
                        }
                    }
                }
            }
        }
    }

    // Read child nodes
    unsigned numChildren = file->ReadVLE();
    for (unsigned i = 0; i < numChildren; ++i)
        PreloadResources(file, false);
#endif
}

void Scene::PreloadResourcesXML(const XMLElement& element)
{
    // If not threaded, can not background load resources, so rather load synchronously later when needed
#ifdef URHO3D_THREADING
    auto* cache = GetSubsystem<ResourceCache>();

    // Node or Scene attributes do not include any resources; therefore skip to the components
    XMLElement compElem = element.GetChild("component");
    while (compElem)
    {
        ea::string typeName = compElem.GetAttribute("type");
        const ea::vector<AttributeInfo>* attributes = context_->GetAttributes(StringHash(typeName));
        if (attributes)
        {
            XMLElement attrElem = compElem.GetChild("attribute");
            unsigned startIndex = 0;

            while (attrElem)
            {
                ea::string name = attrElem.GetAttribute("name");
                unsigned i = startIndex;
                unsigned attempts = attributes->size();

                while (attempts)
                {
                    const AttributeInfo& attr = attributes->at(i);
                    if ((attr.mode_ & AM_FILE) && !attr.name_.compare(name))
                    {
                        if (attr.type_ == VAR_RESOURCEREF)
                        {
                            ResourceRef ref = attrElem.GetVariantValue(attr.type_).GetResourceRef();
                            ea::string name = cache->SanitateResourceName(ref.name_);
                            bool success = cache->BackgroundLoadResource(ref.type_, name);
                            if (success)
                            {
                                ++asyncProgress_.totalResources_;
                                asyncProgress_.resources_.insert(StringHash(name));
                            }
                        }
                        else if (attr.type_ == VAR_RESOURCEREFLIST)
                        {
                            ResourceRefList refList = attrElem.GetVariantValue(attr.type_).GetResourceRefList();
                            for (unsigned k = 0; k < refList.names_.size(); ++k)
                            {
                                ea::string name = cache->SanitateResourceName(refList.names_[k]);
                                bool success = cache->BackgroundLoadResource(refList.type_, name);
                                if (success)
                                {
                                    ++asyncProgress_.totalResources_;
                                    asyncProgress_.resources_.insert(StringHash(name));
                                }
                            }
                        }

                        startIndex = (i + 1) % attributes->size();
                        break;
                    }
                    else
                    {
                        i = (i + 1) % attributes->size();
                        --attempts;
                    }
                }

                attrElem = attrElem.GetNext("attribute");
            }
        }

        compElem = compElem.GetNext("component");
    }

    XMLElement childElem = element.GetChild("node");
    while (childElem)
    {
        PreloadResourcesXML(childElem);
        childElem = childElem.GetNext("node");
    }
#endif
}

void Scene::PreloadResourcesJSON(const JSONValue& value)
{
    // If not threaded, can not background load resources, so rather load synchronously later when needed
#ifdef URHO3D_THREADING
    auto* cache = GetSubsystem<ResourceCache>();

    // Node or Scene attributes do not include any resources; therefore skip to the components
    JSONArray componentArray = value.Get("components").GetArray();

    for (unsigned i = 0; i < componentArray.size(); i++)
    {
        const JSONValue& compValue = componentArray.at(i);
        ea::string typeName = compValue.Get("type").GetString();

        const ea::vector<AttributeInfo>* attributes = context_->GetAttributes(StringHash(typeName));
        if (attributes)
        {
            JSONArray attributesArray = compValue.Get("attributes").GetArray();

            unsigned startIndex = 0;

            for (unsigned j = 0; j < attributesArray.size(); j++)
            {
                const JSONValue& attrVal = attributesArray.at(j);
                ea::string name = attrVal.Get("name").GetString();
                unsigned i = startIndex;
                unsigned attempts = attributes->size();

                while (attempts)
                {
                    const AttributeInfo& attr = attributes->at(i);
                    if ((attr.mode_ & AM_FILE) && !attr.name_.compare(name))
                    {
                        if (attr.type_ == VAR_RESOURCEREF)
                        {
                            ResourceRef ref = attrVal.Get("value").GetVariantValue(attr.type_).GetResourceRef();
                            ea::string name = cache->SanitateResourceName(ref.name_);
                            bool success = cache->BackgroundLoadResource(ref.type_, name);
                            if (success)
                            {
                                ++asyncProgress_.totalResources_;
                                asyncProgress_.resources_.insert(StringHash(name));
                            }
                        }
                        else if (attr.type_ == VAR_RESOURCEREFLIST)
                        {
                            ResourceRefList refList = attrVal.Get("value").GetVariantValue(attr.type_).GetResourceRefList();
                            for (unsigned k = 0; k < refList.names_.size(); ++k)
                            {
                                ea::string name = cache->SanitateResourceName(refList.names_[k]);
                                bool success = cache->BackgroundLoadResource(refList.type_, name);
                                if (success)
                                {
                                    ++asyncProgress_.totalResources_;
                                    asyncProgress_.resources_.insert(StringHash(name));
                                }
                            }
                        }

                        startIndex = (i + 1) % attributes->size();
                        break;
                    }
                    else
                    {
                        i = (i + 1) % attributes->size();
                        --attempts;
                    }
                }

            }
        }

    }

    JSONArray childrenArray = value.Get("children").GetArray();
    for (unsigned i = 0; i < childrenArray.size(); i++)
    {
        const JSONValue& childVal = childrenArray.at(i);
        PreloadResourcesJSON(childVal);
    }
#endif
}

SceneComponentIndex* Scene::GetMutableComponentIndex(StringHash componentType)
{
    if (indexedComponentTypes_.empty())
        return nullptr;

    const unsigned idx = indexedComponentTypes_.index_of(componentType);
    if (idx < componentIndexes_.size())
        return &componentIndexes_[idx];
    return nullptr;
}

void RegisterSceneLibrary(Context* context)
{
    ValueAnimation::RegisterObject(context);
    ObjectAnimation::RegisterObject(context);
    Node::RegisterObject(context);
    Scene::RegisterObject(context);
    SceneResource::RegisterObject(context);
    UnknownComponent::RegisterObject(context);
    SplinePath::RegisterObject(context);
    PrefabReference::RegisterObject(context);
    PrefabResource::RegisterObject(context);
    ShakeComponent::RegisterObject(context);
}

}
