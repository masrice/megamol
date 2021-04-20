/*
 * View2DGL.cpp
 *
 * Copyright (C) 2009 - 2010 by VISUS (Universitaet Stuttgart).
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "mmcore/view/View2DGL.h"
#include "json.hpp"
#include "mmcore/CoreInstance.h"
#include "mmcore/param/BoolParam.h"
#include "mmcore/param/ButtonParam.h"
#include "mmcore/param/ColorParam.h"
#include "mmcore/param/StringParam.h"
#include "mmcore/utility/ColourParser.h"
#include "mmcore/utility/log/Log.h"
#include "mmcore/view/CallRender2DGL.h"
#include "mmcore/view/CallRenderViewGL.h"
#include "vislib/Trace.h"
#include "vislib/graphics/gl/IncludeAllGL.h"
#include "vislib/math/Matrix4.h"
#include "vislib/math/Rectangle.h"


using namespace megamol::core;


/*
 * view::View2DGL::View2DGL
 */
view::View2DGL::View2DGL(void)
        : view::BaseView<glowl::FramebufferObject, gl2D_fbo_create_or_resize, Camera2DController, Camera2DParameters>()
        , _viewUpdateCnt(0) {

    // Override renderSlot behavior
    this->_lhsRenderSlot.SetCallback(
        view::CallRenderViewGL::ClassName(), InputCall::FunctionName(InputCall::FnOnKey), &AbstractView::OnKeyCallback);
    this->_lhsRenderSlot.SetCallback(view::CallRenderViewGL::ClassName(), InputCall::FunctionName(InputCall::FnOnChar),
        &AbstractView::OnCharCallback);
    this->_lhsRenderSlot.SetCallback(view::CallRenderViewGL::ClassName(),
        InputCall::FunctionName(InputCall::FnOnMouseButton), &AbstractView::OnMouseButtonCallback);
    this->_lhsRenderSlot.SetCallback(view::CallRenderViewGL::ClassName(),
        InputCall::FunctionName(InputCall::FnOnMouseMove), &AbstractView::OnMouseMoveCallback);
    this->_lhsRenderSlot.SetCallback(view::CallRenderViewGL::ClassName(),
        InputCall::FunctionName(InputCall::FnOnMouseScroll), &AbstractView::OnMouseScrollCallback);
    // AbstractCallRender
    this->_lhsRenderSlot.SetCallback(view::CallRenderViewGL::ClassName(),
        AbstractCallRender::FunctionName(AbstractCallRender::FnRender), &AbstractView::OnRenderView);
    this->_lhsRenderSlot.SetCallback(view::CallRenderViewGL::ClassName(),
        AbstractCallRender::FunctionName(AbstractCallRender::FnGetExtents), &AbstractView::GetExtents);
    // CallRenderViewGL
    this->_lhsRenderSlot.SetCallback(view::CallRenderViewGL::ClassName(),
        view::CallRenderViewGL::FunctionName(view::CallRenderViewGL::CALL_RESETVIEW), &AbstractView::OnResetView);
    this->MakeSlotAvailable(&this->_lhsRenderSlot);

    this->_rhsRenderSlot.SetCompatibleCall<CallRender2DGLDescription>();
    this->MakeSlotAvailable(&this->_rhsRenderSlot);
}


/*
 * view::View2DGL::~View2DGL
 */
view::View2DGL::~View2DGL(void) {
    this->Release();
}


/*
 * view::View2DGL::GetCameraSyncNumber
 */
unsigned int view::View2DGL::GetCameraSyncNumber(void) const {
    return this->_viewUpdateCnt;
}


/*
 * view::View2DGL::Render
 */
view::ImageWrapper view::View2DGL::Render(double time, double instanceTime, bool present_fbo) {

    AbstractView::beforeRender(time,instanceTime);

    CallRender2DGL* cr2d = this->_rhsRenderSlot.CallAs<CallRender2DGL>();

    if (cr2d != NULL) {

        // clear fbo before sending it down the rendering call
        // the view is the owner of this fbo and therefore responsible
        // for clearing it at the beginning of a render frame
        this->_fbo->bind();
        auto bgcol = this->BkgndColour();
        glClearColor(bgcol.r, bgcol.g, bgcol.b, bgcol.a);
        glClearDepth(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        cr2d->SetFramebufferObject(_fbo);
        cr2d->SetCamera(_camera);

        (*cr2d)(AbstractCallRender::FnRender);

        // after render
        AbstractView::afterRender();
    }

    if (present_fbo) {
        // Blit the final image to the default framebuffer of the window.
        // Technically, the view's fbo should always match the size of the window so a blit is fine.
        // Eventually, presenting the fbo will become the frontends job.
        // Bind and blit framebuffer.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        _fbo->bindToRead(0);
        glBlitFramebuffer(0, 0, _fbo->getWidth(), _fbo->getHeight(), 0, 0, _fbo->getWidth(), _fbo->getHeight(),
            GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    }

    ImageWrapper::DataChannels channels =
        ImageWrapper::DataChannels::RGBA8; // vislib::graphics::gl::FramebufferObject seems to use RGBA8
    unsigned int fbo_color_buffer_gl_handle =
        _fbo->getColorAttachment(0)->getTextureHandle(); // IS THIS SAFE?? IS THIS THE COLOR BUFFER??
    size_t fbo_width = _fbo->getWidth();
    size_t fbo_height = _fbo->getHeight();

    return frontend_resources::wrap_image({fbo_width, fbo_height}, fbo_color_buffer_gl_handle, channels);
}


/*
 * view::View2DGL::ResetView
 */
void view::View2DGL::ResetView(void) {
    if (_cameraIsMutable) { // check if view is in control of the camera
        CallRender2DGL* cr2d = this->_rhsRenderSlot.CallAs<CallRender2DGL>();
        if ((cr2d != nullptr) && (_fbo != nullptr) && ((*cr2d)(AbstractCallRender::FnGetExtents))) {
            Camera::OrthographicParameters cam_intrinsics;
            cam_intrinsics.near_plane = 0.1f;
            cam_intrinsics.far_plane = 100.0f;
            cam_intrinsics.frustrum_height = cr2d->GetBoundingBoxes().BoundingBox().Height();
            cam_intrinsics.aspect = static_cast<float>(_fbo->getWidth()) / static_cast<float>(_fbo->getHeight());
            cam_intrinsics.image_plane_tile =
                Camera::ImagePlaneTile(); // view is in control -> no tiling -> use default tile values

            if ((static_cast<float>(_fbo->getWidth()) / static_cast<float>(_fbo->getHeight())) <
                (static_cast<float>(cr2d->GetBoundingBoxes().BoundingBox().Width()) / cr2d->GetBoundingBoxes().BoundingBox().Height()))
            {
                cam_intrinsics.frustrum_height = cr2d->GetBoundingBoxes().BoundingBox().Width() / cam_intrinsics.aspect;
            }

            Camera::Pose cam_pose;
            cam_pose.position = glm::vec3(
                0.5f * (cr2d->GetBoundingBoxes().BoundingBox().Right() + cr2d->GetBoundingBoxes().BoundingBox().Left()),
                 0.5f * (cr2d->GetBoundingBoxes().BoundingBox().Top() + cr2d->GetBoundingBoxes().BoundingBox().Bottom()), 1.0f);
            cam_pose.direction = glm::vec3(0.0, 0.0, -1.0);
            cam_pose.up = glm::vec3(0.0, 1.0, 0.0);

            _camera = Camera(cam_pose, cam_intrinsics);

        } else {
            Camera::OrthographicParameters cam_intrinsics;
            cam_intrinsics.near_plane = 0.1f;
            cam_intrinsics.far_plane = 100.0f;
            cam_intrinsics.frustrum_height = 1.0f;
            cam_intrinsics.aspect = 1.0f;
            cam_intrinsics.image_plane_tile =
                Camera::ImagePlaneTile(); // view is in control -> no tiling -> use default tile values

            Camera::Pose cam_pose;
            cam_pose.position = glm::vec3(0.0f, 0.0f, 1.0f);
            cam_pose.direction = glm::vec3(0.0, 0.0, -1.0);
            cam_pose.up = glm::vec3(0.0, 1.0, 0.0);

            _camera = Camera(cam_pose, cam_intrinsics);
        }

        this->_viewUpdateCnt++;

    } else {
        // TODO print warning
    }
}


/*
 * view::View2DGL::Resize
 */
void view::View2DGL::Resize(unsigned int width, unsigned int height) {
    if ((this->_fbo->getWidth() != width) || (this->_fbo->getHeight() != height)) {

        glBindFramebuffer(GL_FRAMEBUFFER, 0); // better safe then sorry, "unbind" fbo before delting one
        try {
            _fbo = std::make_shared<glowl::FramebufferObject>(width, height);
            _fbo->createColorAttachment(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);

            // TODO: check completness and throw if not?
        } catch (glowl::FramebufferObjectException const& exc) {
            megamol::core::utility::log::Log::DefaultLog.WriteError(
                "[View3DGL] Unable to create framebuffer object: %s\n", exc.what());
        }
    }
}


/*
 * view::View2DGL::OnRenderView
 */
bool view::View2DGL::OnRenderView(Call& call) {
    view::CallRenderViewGL* crv = dynamic_cast<view::CallRenderViewGL*>(&call);
    if (crv == NULL) {
        return false;
    }

    // get time from incoming call
    double time = crv->Time();
    if (time < 0.0f) time = this->DefaultTime(crv->InstanceTime());
    double instanceTime = crv->InstanceTime();

    auto fbo = _fbo;
    _fbo = crv->GetFramebufferObject();

    auto cam_cpy = _camera;
    auto cam_pose = _camera.get<Camera::Pose>();
    auto cam_intrinsics = _camera.get<Camera::OrthographicParameters>();
    cam_intrinsics.aspect = static_cast<float>(_fbo->getWidth()) / static_cast<float>(_fbo->getHeight());
    _camera = Camera(cam_pose, cam_intrinsics);

    this->Render(time, instanceTime, false);

    _fbo = fbo;
    _camera = cam_cpy;

    return true;
}

/*
 * view::View2DGL::create
 */
bool view::View2DGL::create(void) {

    this->_firstImg = true;

    // intialize fbo with dummy size until the actual size is set during first call to Resize
    this->_fbo = std::make_shared<glowl::FramebufferObject>(1,1);

    return true;
}


/*
 * view::View2DGL::GetExtents
 */
bool view::View2DGL::GetExtents(Call& call) {
    view::CallRenderViewGL* crv = dynamic_cast<view::CallRenderViewGL*>(&call);
    if (crv == nullptr)
        return false;

    CallRender2DGL* cr2d = this->_rhsRenderSlot.CallAs<CallRender2DGL>();
    if (cr2d == nullptr) {
        return false;
    }
    cr2d->SetCamera(this->_camera);

    if (!(*cr2d)(CallRender2DGL::FnGetExtents))
        return false;

    crv->SetTimeFramesCount(cr2d->TimeFramesCount());
    crv->SetIsInSituTime(cr2d->IsInSituTime());
    return true;
}
