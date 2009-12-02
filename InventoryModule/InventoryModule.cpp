// For conditions of distribution and use, see copyright notice in license.txt

/**
 *  @file   InventoryModule.cpp
 *  @brief  Inventory module. Inventory module is the owner of the inventory data model.
 *          Implement data model -spesific event handling etc. here, not in InventoryWindow
 *          or InventoryItemModel classes.
 */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "InventoryModule.h"
#include "RexLogicModule.h"
#include "InventoryWindow.h"
#include "NetworkEvents.h"
#include "Inventory/InventoryEvents.h"
#include "AssetUploader.h"
#include "QtUtils.h"
#include "AssetEvents.h"
#include "OpenSimInventoryDataModel.h"
#include "WebdavInventoryDataModel.h"

#include <QObject>
#include <QStringList>
#include <QVector>

namespace Inventory
{

InventoryModule::InventoryModule() :
    ModuleInterfaceImpl(Foundation::Module::MT_Inventory),
    inventoryEventCategory_(0),
    networkStateEventCategory_(0),
    assetEventCategory_(0),
    resourceEventCategory_(0),
    frameworkEventCategory_(0),
    inventoryWindow_(0),
    inventoryType_(IDMT_Unknown)
{
}

InventoryModule::~InventoryModule()
{
}

void InventoryModule::Load()
{
    LogInfo("System " + Name() + " loaded.");
}

void InventoryModule::Unload()
{
    LogInfo("System " + Name() + " unloaded.");
}

void InventoryModule::Initialize()
{
    // Register event category and events.
    eventManager_ = framework_->GetEventManager();
    inventoryEventCategory_ = eventManager_->RegisterEventCategory("Inventory");
    eventManager_->RegisterEvent(inventoryEventCategory_, Events::EVENT_INVENTORY_DESCENDENT, "InventoryDescendent");
    eventManager_->RegisterEvent(inventoryEventCategory_, Events::EVENT_INVENTORY_UPLOAD_FILE, "InventoryUpload");
    eventManager_->RegisterEvent(inventoryEventCategory_, Events::EVENT_INVENTORY_UPLOAD_BUFFER, "InventoryUploadBuffer");
    eventManager_->RegisterEvent(inventoryEventCategory_, Events::EVENT_INVENTORY_ITEM_OPEN, "InventoryItemOpen");
    eventManager_->RegisterEvent(inventoryEventCategory_, Events::EVENT_INVENTORY_ITEM_DOWNLOADED, "InventoryItemDownloaded");

    // Register console commands.
    boost::shared_ptr<Console::CommandService> console = framework_->GetService<Console::CommandService>
        (Foundation::Service::ST_ConsoleCommand).lock();
    if (console)
    {
        console->RegisterCommand(Console::CreateCommand("Upload",
            "Upload an asset. Usage: Upload(AssetType, Name, Description)",
            Console::Bind(this, &Inventory::InventoryModule::UploadAsset)));

        console->RegisterCommand(Console::CreateCommand("MultiUpload", "Upload multiple assets.",
            Console::Bind(this, &Inventory::InventoryModule::UploadMultipleAssets)));
    }

    // Create inventory_ window.
    inventoryWindow_ = new InventoryWindow(framework_);

    LogInfo("System " + Name() + " initialized.");
}

void InventoryModule::PostInitialize()
{
    frameworkEventCategory_ = eventManager_->QueryEventCategory("Framework");
    if (frameworkEventCategory_ == 0)
        LogError("Failed to query \"Framework\" event category");

    assetEventCategory_ = eventManager_->QueryEventCategory("Asset");
    if (assetEventCategory_ == 0)
        LogError("Failed to query \"Asset\" event category");

    resourceEventCategory_ = eventManager_->QueryEventCategory("Resource");
    if (resourceEventCategory_ == 0)
        LogError("Failed to query \"Resource\" event category");
}

void InventoryModule::Uninitialize()
{
    SAFE_DELETE(inventoryWindow_);
    LogInfo("System " + Name() + " uninitialized.");

    eventManager_.reset();
    currentWorldStream_.reset();
    inventory_.reset();
}

void InventoryModule::SubscribeToNetworkEvents(ProtocolUtilities::ProtocolWeakPtr currentProtocolModule)
{
    networkStateEventCategory_ = eventManager_->QueryEventCategory("NetworkState");
    if (networkStateEventCategory_ == 0)
        LogError("Failed to query \"NetworkState\" event category");
    else
        LogInfo("System " + Name() + " subscribed to [NetworkIn]");
}

void InventoryModule::Update(Core::f64 frametime)
{
}

bool InventoryModule::HandleEvent(
    Core::event_category_id_t category_id,
    Core::event_id_t event_id,
    Foundation::EventDataInterface* data)
{
    if (category_id == networkStateEventCategory_)
    {
        // Connected to server. Initialize inventory_ tree model.
        if (event_id == ProtocolUtilities::Events::EVENT_SERVER_CONNECTED)
        {
            ProtocolUtilities::AuthenticationEventData *auth = dynamic_cast<ProtocolUtilities::AuthenticationEventData *>(data);
            if (!auth)
                return false;

            switch(auth->type)
            {
            case ProtocolUtilities::AT_Taiga:
                // Check if python module is loaded and has taken care of PythonQt::init()
                if (!framework_->GetModuleManager()->HasModule(Foundation::Module::MT_PythonScript))
                {
                    LogError("Python module not in use. WebDAV inventory can't be used!");
                    inventoryType_ = IDMT_Unknown;
                }
                else
                {
                    // Create WebDAV inventory model.
                    inventory_ = InventoryPtr(new WebDavInventoryDataModel(STD_TO_QSTR(auth->identityUrl), STD_TO_QSTR(auth->hostUrl)));
                    inventoryWindow_->InitInventoryTreeModel(inventory_);
                    inventoryType_ = IDMT_WebDav;
                }
                break;
            case ProtocolUtilities::AT_OpenSim:
            case ProtocolUtilities::AT_RealXtend:
            {
                // Create OpenSim inventory model.
                RexLogic::RexLogicModule *rexLogic = dynamic_cast<RexLogic::RexLogicModule *>(framework_->GetModuleManager()->GetModule(
                    Foundation::Module::MT_WorldLogic).lock().get());

                inventory_ = InventoryPtr(new OpenSimInventoryDataModel(framework_, rexLogic->GetInventory().get()));

                // Set world stream used for sending udp packets.
                static_cast<OpenSimInventoryDataModel *>(inventory_.get())->SetWorldStream(currentWorldStream_);

                inventoryWindow_->InitInventoryTreeModel(inventory_);
                inventoryType_ = IDMT_OpenSim;
                break;
            }
            case ProtocolUtilities::AT_Unknown:
            default:
                inventoryType_ = IDMT_Unknown;
                break;
            }

            return false;
        }

        // Disconnected from server. Hide inventory_ and reset tree model.
        if (event_id == ProtocolUtilities::Events::EVENT_SERVER_DISCONNECTED)
        {
            if (inventoryWindow_)
            {
                inventoryWindow_->Hide();
                inventoryWindow_->ResetInventoryTreeModel();
            }
        }

        return false;
    }

    if (category_id == inventoryEventCategory_)
    {
        // Add new items to inventory_.
        if (event_id == Inventory::Events::EVENT_INVENTORY_DESCENDENT)
            if (inventoryType_ == IDMT_OpenSim)
                checked_static_cast<OpenSimInventoryDataModel *>(inventory_.get())->HandleInventoryDescendents(data);

        // Upload request from other modules.
        if (event_id == Inventory::Events::EVENT_INVENTORY_UPLOAD_FILE)
        {
            InventoryUploadEventData *upload_data = dynamic_cast<InventoryUploadEventData *>(data);
            if (!upload_data)
                return false;

            inventory_->UploadFiles(upload_data->filenames, 0);
        }

        // Upload request from other modules, using buffers.
        if (event_id == Inventory::Events::EVENT_INVENTORY_UPLOAD_BUFFER)
        {
            InventoryUploadBufferEventData *upload_data = dynamic_cast<InventoryUploadBufferEventData *>(data);
            if (!upload_data)
                return false;

            inventory_->UploadFilesFromBuffer(upload_data->filenames, upload_data->buffers, 0);
        }

        return false;
    }

    if (category_id == frameworkEventCategory_)
    {
        if (event_id == Foundation::NETWORKING_REGISTERED)
        {
            Foundation::NetworkingRegisteredEvent *event_data = dynamic_cast<Foundation::NetworkingRegisteredEvent *>(data);
            if (event_data)
                SubscribeToNetworkEvents(event_data->currentProtocolModule);
            return false;
        }

        if(event_id == Foundation::WORLD_STREAM_READY)
        {
            Foundation::WorldStreamReadyEvent *event_data = dynamic_cast<Foundation::WorldStreamReadyEvent *>(data);
            if (event_data)
                currentWorldStream_ = event_data->WorldStream;

            return false;
        }
    }

    if (inventoryType_ == IDMT_OpenSim)
    {
        if (!inventory_.get())
            return false;

        OpenSimInventoryDataModel *osmodel = checked_static_cast<OpenSimInventoryDataModel *>(inventory_.get());
        if (osmodel->HasPendingDownloadRequests())
        {
            if (category_id == assetEventCategory_ && event_id == Asset::Events::ASSET_READY)
                osmodel->HandleAssetReady(data);

            if (category_id == resourceEventCategory_ && event_id == Resource::Events::RESOURCE_READY)
                osmodel->HandleResourceReady(data);
        }

        if (osmodel->HasPendingOpenItemRequests())
        {
            if (category_id == assetEventCategory_ && event_id == Asset::Events::ASSET_READY)
                osmodel->HandleAssetReady(data);

            if (category_id == resourceEventCategory_ && event_id == Resource::Events::RESOURCE_READY)
                osmodel->HandleResourceReady(data);
        }
    }

    return false;
}

Console::CommandResult InventoryModule::UploadAsset(const Core::StringVector &params)
{
    using namespace RexTypes;

    if (!currentWorldStream_.get())
        return Console::ResultFailure("Not connected to server.");

    if (!inventory_.get())
        return Console::ResultFailure("Inventory doesn't exist. Can't upload!.");

    if (inventoryType_ != IDMT_OpenSim)
        return Console::ResultFailure("Console upload supported only for classic OpenSim inventory_.");

    AssetUploader *uploader = static_cast<OpenSimInventoryDataModel *>(inventory_.get())->GetAssetUploader();
    if (!uploader)
        return Console::ResultFailure("Asset uploader not initialized. Can't upload.");

    std::string name = "(No Name)";
    std::string description = "(No Description)";

    if (params.size() < 1)
        return Console::ResultFailure("Invalid syntax. Usage: \"upload [asset_type] [name] [description]."
            "Name and description are optional. Supported asset types:\n"
            "Texture\nMesh\nSkeleton\nMaterialScript\nParticleScript\nFlashAnimation");

    asset_type_t asset_type = GetAssetTypeFromTypeName(params[0]);
    if (asset_type == -1)
        return Console::ResultFailure("Invalid asset type. Supported parameters:\n"
            "Texture\nMesh\nSkeleton\nMaterialScript\nParticleScript\nFlashAnimation");

    if (params.size() > 1)
        name = params[1];

    if (params.size() > 2)
        description = params[2];

    std::string filter = GetOpenFileNameFilter(asset_type);
    std::string cat_name = GetCategoryNameForAssetType(asset_type);

    RexUUID folder_id = RexUUID(inventory_->GetFirstChildFolderByName(cat_name.c_str())->GetID().toStdString());
    if (folder_id.IsNull())
         return Console::ResultFailure("Inventory folder for this type of file doesn't exists. File can't be uploaded.");

    currentWorldStream_->SendAgentPausePacket();

    std::string filename = Foundation::QtUtils::GetOpenFileName(filter, "Open", Foundation::QtUtils::GetCurrentPath());
    if (filename == "")
        return Console::ResultFailure("No file chosen.");

    currentWorldStream_->SendAgentResumePacket();

    uploader->UploadFile(asset_type, filename, name, description, folder_id);

    return Console::ResultSuccess();
}

Console::CommandResult InventoryModule::UploadMultipleAssets(const Core::StringVector &params)
{
    if (!currentWorldStream_.get())
        return Console::ResultFailure("Not connected to server.");

    if (!inventory_.get())
        return Console::ResultFailure("Inventory doesn't exist. Can't upload!.");

    if (inventoryType_ != IDMT_OpenSim)
        return Console::ResultFailure("Console upload supported only for classic OpenSim inventory.");

    AssetUploader *uploader = static_cast<OpenSimInventoryDataModel *>(inventory_.get())->GetAssetUploader();
    if (!uploader)
        return Console::ResultFailure("Asset uploader not initialized. Can't upload.");

    currentWorldStream_->SendAgentPausePacket();

    Core::StringList filenames = Foundation::QtUtils::GetOpenRexFileNames(Foundation::QtUtils::GetCurrentPath());
    if (filenames.empty())
        return Console::ResultFailure("No files chosen.");

    currentWorldStream_->SendAgentResumePacket();

    uploader->UploadFiles(filenames);

    return Console::ResultSuccess();
}

} // namespace Inventory

extern "C" void POCO_LIBRARY_API SetProfiler(Foundation::Profiler *profiler);
void SetProfiler(Foundation::Profiler *profiler)
{
    Foundation::ProfilerSection::SetProfiler(profiler);
}

using namespace Inventory;

POCO_BEGIN_MANIFEST(Foundation::ModuleInterface)
    POCO_EXPORT_CLASS(InventoryModule)
POCO_END_MANIFEST
