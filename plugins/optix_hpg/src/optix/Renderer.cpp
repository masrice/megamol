#include "Renderer.h"

#include "mmcore/param/BoolParam.h"
#include "mmcore/param/FloatParam.h"
#include "mmcore/param/IntParam.h"

#include "raygen.h"

#include "optix/CallGeometry.h"

#include "vislib/graphics/gl/IncludeAllGL.h"

#include "optix/Utils.h"

#include "optix_stubs.h"

namespace megamol::optix_hpg {
extern "C" const char embedded_raygen_programs[];
extern "C" const char embedded_miss_programs[];
extern "C" const char embedded_miss_occlusion_programs[];
} // namespace megamol::optix_hpg


megamol::optix_hpg::Renderer::Renderer()
        : _in_geo_slot("inGeo", "")
        , spp_slot_("spp", "")
        , max_bounces_slot_("max bounces", "")
        , accumulate_slot_("accumulate", "")
        , intensity_slot_("intensity", "") {
    _in_geo_slot.SetCompatibleCall<CallGeometryDescription>();
    MakeSlotAvailable(&_in_geo_slot);

    spp_slot_ << new core::param::IntParam(1, 1);
    MakeSlotAvailable(&spp_slot_);

    max_bounces_slot_ << new core::param::IntParam(0, 0);
    MakeSlotAvailable(&max_bounces_slot_);

    accumulate_slot_ << new core::param::BoolParam(true);
    MakeSlotAvailable(&accumulate_slot_);

    intensity_slot_ << new core::param::FloatParam(1.0f, std::numeric_limits<float>::min());
    MakeSlotAvailable(&intensity_slot_);

    // Callback should already be set by RendererModule
    this->MakeSlotAvailable(&this->chainRenderSlot);

    // Callback should already be set by RendererModule
    this->MakeSlotAvailable(&this->renderSlot);
}


megamol::optix_hpg::Renderer::~Renderer() {
    this->Release();
}


void megamol::optix_hpg::Renderer::setup() {
    raygen_module_ = MMOptixModule(embedded_raygen_programs, optix_ctx_->GetOptiXContext(),
        &optix_ctx_->GetModuleCompileOptions(), &optix_ctx_->GetPipelineCompileOptions(),
        MMOptixModule::MMOptixProgramGroupKind::MMOPTIX_PROGRAM_GROUP_KIND_RAYGEN,
        {{MMOptixModule::MMOptixNameKind::MMOPTIX_NAME_GENERIC, "raygen_program"}});
    miss_module_ = MMOptixModule(embedded_miss_programs, optix_ctx_->GetOptiXContext(),
        &optix_ctx_->GetModuleCompileOptions(), &optix_ctx_->GetPipelineCompileOptions(),
        MMOptixModule::MMOptixProgramGroupKind::MMOPTIX_PROGRAM_GROUP_KIND_MISS,
        {{MMOptixModule::MMOptixNameKind::MMOPTIX_NAME_GENERIC, "miss_program"}});
    miss_occlusion_module_ = MMOptixModule(embedded_miss_occlusion_programs, optix_ctx_->GetOptiXContext(),
        &optix_ctx_->GetModuleCompileOptions(), &optix_ctx_->GetPipelineCompileOptions(),
        MMOptixModule::MMOptixProgramGroupKind::MMOPTIX_PROGRAM_GROUP_KIND_MISS,
        {{MMOptixModule::MMOptixNameKind::MMOPTIX_NAME_GENERIC, "miss_program_occlusion"}});

    OPTIX_CHECK_ERROR(optixSbtRecordPackHeader(raygen_module_, &_sbt_raygen_record));
    OPTIX_CHECK_ERROR(optixSbtRecordPackHeader(miss_module_, &sbt_miss_records_[0]));
    OPTIX_CHECK_ERROR(optixSbtRecordPackHeader(miss_occlusion_module_, &sbt_miss_records_[1]));

    CUDA_CHECK_ERROR(cuMemAlloc(&_frame_state_buffer, sizeof(device::FrameState)));

    _sbt_raygen_record.data.frameStateBuffer = (device::FrameState*) _frame_state_buffer;
}


bool megamol::optix_hpg::Renderer::Render(CallRender3DCUDA& call) {
    auto const width = call.GetFramebuffer()->width;
    auto const height = call.GetFramebuffer()->height;
    auto viewport = vislib::math::Rectangle<int>(0, 0, width, height);

    static bool not_init = true;

    if (not_init) {
        setup();

        _sbt_raygen_record.data.fbSize = glm::uvec2(viewport.Width(), viewport.Height());
        _sbt_raygen_record.data.col_surf = call.GetFramebuffer()->colorBuffer;
        _sbt_raygen_record.data.depth_surf = call.GetFramebuffer()->depthBuffer;
        call.GetFramebuffer()->data.exec_stream = optix_ctx_->GetExecStream();

        _current_fb_size = viewport;

        not_init = false;
    }

    auto in_geo = _in_geo_slot.CallAs<CallGeometry>();
    if (in_geo == nullptr)
        return false;

    in_geo->set_ctx(optix_ctx_.get());
    if (!(*in_geo)())
        return false;

    bool rebuild_sbt = false;

    ++_frame_state.frameIdx;

    if (viewport != _current_fb_size) {
        _sbt_raygen_record.data.fbSize = glm::uvec2(viewport.Width(), viewport.Height());
        _sbt_raygen_record.data.col_surf = call.GetFramebuffer()->colorBuffer;
        _sbt_raygen_record.data.depth_surf = call.GetFramebuffer()->depthBuffer;
        _frame_state.frameIdx = 0;

        rebuild_sbt = true;

        _current_fb_size = viewport;
    }

    // Camera
    core::view::Camera cam = call.GetCamera();

    auto cam_pose = cam.get<core::view::Camera::Pose>();
    auto cam_intrinsics = cam.get<core::view::Camera::PerspectiveParameters>();

    if (!(cam_intrinsics == old_cam_intrinsics)) {
        auto proj = cam.getProjectionMatrix();
        // Generate complete snapshot and calculate matrices
        // is this a) correct and b) actually needed for the new cam?
        // auto const depth_A = proj[2][2];//projTemp(2, 2);
        // auto const depth_B = proj[3][2]; // projTemp(2, 3);
        // auto const depth_C = proj[2][3]; //projTemp(3, 2);
        auto const depth_A = proj[2][2]; // projTemp(2, 2);
        auto const depth_B = proj[2][3]; // projTemp(2, 3);
        auto const depth_C = proj[3][2]; // projTemp(3, 2);
        auto const depth_params = glm::vec3(depth_A, depth_B, depth_C);
        _frame_state.depth_params = depth_params;

        auto curCamNearClip = 100;
        auto curCamAspect = cam_intrinsics.aspect;
        auto hfov = cam_intrinsics.fovy / 2.0f;

        auto th = std::tan(hfov) * curCamNearClip;
        auto rw = th * curCamAspect;

        _frame_state.rw = rw;
        _frame_state.th = th;
        _frame_state.near = curCamNearClip;

        _frame_state.frameIdx = 0;
        old_cam_intrinsics = cam_intrinsics;
    }

    if (!(cam_pose == old_cam_pose)) {
        auto curCamPos = cam_pose.position;
        auto curCamView = cam_pose.direction;
        auto curCamUp = cam_pose.up;
        auto curCamRight = glm::cross(cam_pose.direction, cam_pose.up);
        // auto curCamNearClip = snapshot.frustum_near;
        _frame_state.camera_center = glm::vec3(curCamPos.x, curCamPos.y, curCamPos.z);
        _frame_state.camera_front = glm::vec3(curCamView.x, curCamView.y, curCamView.z);
        _frame_state.camera_right = glm::vec3(curCamRight.x, curCamRight.y, curCamRight.z);
        _frame_state.camera_up = glm::vec3(curCamUp.x, curCamUp.y, curCamUp.z);

        _frame_state.frameIdx = 0;
        old_cam_pose = cam_pose;
    }

    if (is_dirty()) {
        _frame_state.samplesPerPixel = spp_slot_.Param<core::param::IntParam>()->Value();
        _frame_state.maxBounces = max_bounces_slot_.Param<core::param::IntParam>()->Value();
        _frame_state.accumulate = accumulate_slot_.Param<core::param::BoolParam>()->Value();

        _frame_state.intensity = intensity_slot_.Param<core::param::FloatParam>()->Value();

        _frame_state.frameIdx = 0;
        reset_dirty();
    }

    if (old_bg != call.BackgroundColor()) {
        _frame_state.background = call.BackgroundColor();
        sbt_miss_records_[0].data.bg = _frame_state.background;
        old_bg = call.BackgroundColor();
        _frame_state.frameIdx = 0;
        rebuild_sbt = true;
    }

    if (_frame_id != in_geo->FrameID() || _in_data_hash != in_geo->DataHash()) {
        _sbt_raygen_record.data.world = *in_geo->get_handle();
        _frame_state.frameIdx = 0;

        rebuild_sbt = true;
        _frame_id = in_geo->FrameID();
        _in_data_hash = in_geo->DataHash();
    }

    if (in_geo->has_program_update()) {
        _frame_state.frameIdx = 0;

        auto const& [geo_progs, num_geo_progs] = in_geo->get_program_groups();

        auto num_groups = 3 + num_geo_progs;
        std::vector<OptixProgramGroup> groups;
        groups.reserve(num_groups);
        groups.push_back(raygen_module_);
        groups.push_back(miss_module_);
        groups.push_back(miss_occlusion_module_);
        std::for_each(
            geo_progs, geo_progs + num_geo_progs, [&groups](OptixProgramGroup const el) { groups.push_back(el); });

        std::size_t log_size = 2048;
        std::string log;
        log.resize(log_size);

        OPTIX_CHECK_ERROR(optixPipelineCreate(optix_ctx_->GetOptiXContext(), &optix_ctx_->GetPipelineCompileOptions(),
            &optix_ctx_->GetPipelineLinkOptions(), groups.data(), groups.size(), log.data(), &log_size, &_pipeline));
    }

    CUDA_CHECK_ERROR(
        cuMemcpyHtoDAsync(_frame_state_buffer, &_frame_state, sizeof(_frame_state), optix_ctx_->GetExecStream()));

    if (rebuild_sbt || in_geo->has_sbt_update()) {
        auto const& [geo_records, num_geo_records, geo_records_stride] = in_geo->get_record();
        sbt_.SetSBT(&_sbt_raygen_record, sizeof(_sbt_raygen_record), nullptr, 0, sbt_miss_records_.data(),
            sizeof(SBTRecord<device::MissData>), sbt_miss_records_.size(), geo_records, geo_records_stride,
            num_geo_records, nullptr, 0, 0, optix_ctx_->GetExecStream());
    }

    OPTIX_CHECK_ERROR(
        optixLaunch(_pipeline, optix_ctx_->GetExecStream(), 0, 0, sbt_, viewport.Width(), viewport.Height(), 1));

    return true;
}


bool megamol::optix_hpg::Renderer::GetExtents(CallRender3DCUDA& call) {
    auto in_geo = _in_geo_slot.CallAs<CallGeometry>();
    if (in_geo != nullptr) {
        in_geo->SetFrameID(static_cast<unsigned int>(call.Time()));
        if (!(*in_geo)(1))
            return false;
        call.SetTimeFramesCount(in_geo->FrameCount());

        call.AccessBoundingBoxes() = in_geo->AccessBoundingBoxes();
    } else {
        call.SetTimeFramesCount(1);
        call.AccessBoundingBoxes().Clear();
    }

    return true;
}


bool megamol::optix_hpg::Renderer::create() {
    auto& cuda_res = frontend_resources.get<frontend_resources::CUDA_Context>();
    if (cuda_res.ctx_ != nullptr) {
        optix_ctx_ = std::make_unique<Context>(cuda_res);
    } else {
        return false;
    }

    _frame_state.samplesPerPixel = spp_slot_.Param<core::param::IntParam>()->Value();
    _frame_state.maxBounces = max_bounces_slot_.Param<core::param::IntParam>()->Value();
    _frame_state.accumulate = accumulate_slot_.Param<core::param::BoolParam>()->Value();

    _frame_state.intensity = intensity_slot_.Param<core::param::FloatParam>()->Value();

    return true;
}


void megamol::optix_hpg::Renderer::release() {
    CUDA_CHECK_ERROR(cuMemFree(_frame_state_buffer));
    OPTIX_CHECK_ERROR(optixPipelineDestroy(_pipeline));
}
