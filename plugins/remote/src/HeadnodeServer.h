#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "mmcore/CallerSlot.h"
#include "mmcore/Module.h"
#include "mmcore/param/ParamSlot.h"
#include "mmcore/param/ParamUpdateListener.h"

#include "DistributedProto.h"
#include "FBOCommFabric.h"

namespace megamol {
namespace remote {

class HeadnodeServer : public core::Module, public core::param::ParamUpdateListener {
public:
    HeadnodeServer();
    ~HeadnodeServer() override;

    /**
     * Answer the name of this module.
     *
     * @return The name of this module.
     */
    static const char* ClassName(void) {
        return "HeadnodeServer";
    }

    /**
     * Answer a human readable description of this module.
     *
     * @return A human readable description of this module.
     */
    static const char* Description(void) {
        return "HeadnodeServer";
    }

    /**
     * Answers whether this module is available on the current system.
     *
     * @return 'true' if the module is available, 'false' otherwise.
     */
    static bool IsAvailable(void) {
        return true;
    }

    /**
     * Disallow usage in quickstarts
     *
     * @return false
     */
    static bool SupportQuickstart(void) {
        return false;
    }

protected:
    bool create() override;

    void release() override;

    void ParamUpdated(core::param::ParamSlot& slot) override;

    void BatchParamUpdated(param_updates_vec_t const& updates) override;

private:
    bool onStartServer(core::param::ParamSlot& param);

    bool onLuaCommand(core::param::ParamSlot& param);

    bool get_cam_upd(std::vector<char>& msg);

    bool init_threads();

    bool shutdown_threads();

    void do_communication();

    core::CallerSlot view_slot_;

    core::param::ParamSlot address_slot_;

    core::param::ParamSlot start_server_slot_;

    core::param::ParamSlot lua_command_slot_;

    core::param::ParamSlot deploy_project_slot_;

    FBOCommFabric comm_fabric_;

    mutable std::mutex send_buffer_guard_;

    std::vector<char> send_buffer_;

    unsigned int syncnumber = -1;

    std::thread comm_thread_;

    bool run_threads_;

    bool is_job_running_;

    std::atomic<bool> buffer_has_changed_;

    uint64_t msg_id_;

}; // end class HeadnodeServer

} // end namespace remote
} // end namespace megamol
