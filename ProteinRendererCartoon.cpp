/*
 * ProteinRendererCartoon.cpp
 *
 * Copyright (C) 2008 by Universitaet Stuttgart (VISUS). 
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"

#define _USE_MATH_DEFINES 1

#include "ProteinRendererCartoon.h"
#include "CoreInstance.h"
#include "param/EnumParam.h"
#include "param/BoolParam.h"
#include "utility/ShaderSourceFactory.h"
#include "vislib/assert.h"
#include "vislib/File.h"
#include "vislib/String.h"
#include "vislib/Quaternion.h"
#include "vislib/OutOfRangeException.h"
#include "vislib/Trace.h"
#include "vislib/ShaderSource.h"
#include "vislib/AbstractOpenGLShader.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <glh/glh_genext.h>
#include <math.h>
#include <time.h>

using namespace megamol::core;


/*
 * protein::ProteinRendererCartoon::ProteinRendererCartoon (CTOR)
 */
protein::ProteinRendererCartoon::ProteinRendererCartoon (void) : RendererModule (), 
	m_protDataCallerSlot ("getdata", "Connects the protein rendering with protein data storage"),
    m_callFrameCalleeSlot("callFrame", "Connects the protein rendering with frame call from RMS renderer"),
    m_renderingModeParam("renderingMode", "Rendering Mode"), 
    m_coloringModeParam("coloringMode", "Coloring Mode"), 
    m_smoothCartoonColoringParam("smoothCartoonColoring", "Use smooth coloring with Cartoon representation")
{
    this->m_protDataCallerSlot.SetCompatibleCall<CallProteinDataDescription>();
    this->MakeSlotAvailable(&this->m_protDataCallerSlot);

    protein::CallFrameDescription dfd;
	this->m_callFrameCalleeSlot.SetCallback (dfd.ClassName(), "CallFrame", &ProteinRendererCartoon::ProcessFrameRequest);
    this->MakeSlotAvailable(&this->m_callFrameCalleeSlot);

	// check if geom-shader is supported
	if( this->m_cartoonShader.AreExtensionsAvailable())
		this->m_geomShaderSupported = true;
	else
		this->m_geomShaderSupported = false;

	// --- set the coloring mode ---

	//this->SetColoringMode(ELEMENT);
	//this->SetColoringMode(AMINOACID);
	this->SetColoringMode(STRUCTURE);
	//this->SetColoringMode(VALUE);
	//this->SetColoringMode(CHAIN_ID);
	//this->SetColoringMode(RAINBOW);
    param::EnumParam *cm = new param::EnumParam(int(this->m_currentColoringMode));

	cm->SetTypePair(ELEMENT, "Element");
	cm->SetTypePair(AMINOACID, "AminoAcid");
	cm->SetTypePair(STRUCTURE, "SecondaryStructure");
	//cm->SetTypePair(VALUE, "Value");
	cm->SetTypePair(CHAIN_ID, "ChainID");
	//cm->SetTypePair(RAINBOW, "Rainbow");

    this->m_coloringModeParam << cm;

	// --- set the render mode ---

	//SetRenderMode(CARTOON);
	//SetRenderMode(CARTOON_SIMPLE);
    SetRenderMode(CARTOON_CPU);
	//SetRenderMode(CARTOON_GPU);
    param::EnumParam *rm = new param::EnumParam(int(this->m_currentRenderMode));

	if( this->m_geomShaderSupported )
	{
		rm->SetTypePair(CARTOON, "Cartoon Hybrid");
		rm->SetTypePair(CARTOON_SIMPLE, "Cartoon Hybrid (simple)");
	    //rm->SetTypePair(CARTOON_GPU, "Cartoon GPU");
	}
    rm->SetTypePair(CARTOON_CPU, "Cartoon CPU");

    this->m_renderingModeParam << rm;

	// --- set smooth coloring for cartoon rendering ---
	m_smoothCartoonColoringMode = false;
    this->m_smoothCartoonColoringParam.SetParameter(new param::BoolParam(this->m_smoothCartoonColoringMode));

    this->MakeSlotAvailable(&this->m_coloringModeParam);
    this->MakeSlotAvailable(&this->m_renderingModeParam); 
    this->MakeSlotAvailable(&this->m_smoothCartoonColoringParam);

	// --- set the radius for the cartoon rednering mode ---
	this->m_radiusCartoon = 0.2f;

	// --- initialize all pointers and variables for cartoon ---
	this->m_vertTube = new float[1];
	this->m_colorsParamsTube = new float[1];
	this->m_vertArrow = new float[1];
	this->m_colorsParamsArrow = new float[1];
	this->m_vertHelix = new float[1];
	this->m_colorsParamsHelix = new float[1];
	this->m_normalArrow = new float[1];
	this->m_normalHelix = new float[1];
	this->m_normalTube = new float[1];

	// hybrid CARTOON render mode was not prepared yet
	this->m_prepareCartoonHybrid = true;
	// CPU CARTOON render mode was not prepared yet
	this->m_prepareCartoonCPU = true;

	// set default value for spline segments per amino acid
	this->m_numberOfSplineSeg = 6;
	// set default value for tube segments
	this->m_numberOfTubeSeg = 6;

	// fill amino acid color table
	this->FillAminoAcidColorTable();

    this->m_renderRMSData = false;
    this->m_frameLabel = NULL;
}


/*
 * protein::ProteinRendererCartoon::~ProteinRendererCartoon (DTOR)
 */
protein::ProteinRendererCartoon::~ProteinRendererCartoon (void) 
{
    delete this->m_frameLabel;
    this->Release ();
}


/*
 * protein::ProteinRendererCartoon::release
 */
void protein::ProteinRendererCartoon::release (void) 
{

}


/*
 * protein::ProteinRendererCartoon::create
 */
bool protein::ProteinRendererCartoon::create(void)
{
    using vislib::sys::Log;
	glh_init_extensions( "GL_ARB_vertex_shader GL_ARB_vertex_program GL_ARB_shader_objects");

	if( this->m_geomShaderSupported )
	{
		glh_init_extensions( "GL_EXT_gpu_shader4 GL_EXT_geometry_shader4 GL_EXT_bindable_uniform");
		glh_init_extensions( "GL_VERSION_2_0");
	}

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable( GL_VERTEX_PROGRAM_POINT_SIZE_ARB);
	glEnable( GL_VERTEX_PROGRAM_TWO_SIDE);

	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE_ARB);

	using namespace vislib::graphics::gl;

    ShaderSource vertSrc;
    ShaderSource fragSrc;
	ShaderSource geomSrc;

	////////////////////////////////////////////////////
	// load the shader sources for the cartoon shader //
	////////////////////////////////////////////////////

	if (this->m_geomShaderSupported) {
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::cartoon::vertex", vertSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load vertex shader source for cartoon shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::cartoon::geometry", geomSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load geometry shader source for cartoon shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::cartoon::fragment", fragSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load fragment shader source for cartoon shader");
            return false;
        }
		this->m_cartoonShader.Compile( vertSrc.Code(), vertSrc.Count(), geomSrc.Code(), geomSrc.Count(), fragSrc.Code(), fragSrc.Count());
		// setup geometry shader
		this->m_cartoonShader.SetProgramParameter( GL_GEOMETRY_INPUT_TYPE_EXT , GL_TRIANGLES_ADJACENCY_EXT);
		this->m_cartoonShader.SetProgramParameter( GL_GEOMETRY_OUTPUT_TYPE_EXT, GL_TRIANGLE_STRIP);
		this->m_cartoonShader.SetProgramParameter( GL_GEOMETRY_VERTICES_OUT_EXT, 200);
		this->m_cartoonShader.Link();

        /////////////////////////////////////////////////
        // load the shader sources for the tube shader //
        /////////////////////////////////////////////////
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::cartoon::vertex", vertSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load vertex shader source for cartoon shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::tubeGeometry", geomSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load geometry shader source for tube shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::cartoon::fragment", fragSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load fragment shader source for cartoon shader");
            return false;
        }
        this->m_tubeShader.Compile( vertSrc.Code(), vertSrc.Count(), geomSrc.Code(), geomSrc.Count(), fragSrc.Code(), fragSrc.Count());
        this->m_tubeShader.SetProgramParameter( GL_GEOMETRY_INPUT_TYPE_EXT , GL_TRIANGLES_ADJACENCY_EXT);
        this->m_tubeShader.SetProgramParameter( GL_GEOMETRY_OUTPUT_TYPE_EXT, GL_TRIANGLE_STRIP);
        this->m_tubeShader.SetProgramParameter( GL_GEOMETRY_VERTICES_OUT_EXT, 200);
        this->m_tubeShader.Link();

        //////////////////////////////////////////////////
        // load the shader sources for the arrow shader //
        //////////////////////////////////////////////////
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::cartoon::vertex", vertSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load vertex shader source for cartoon shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::arrowGeometry", geomSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load geometry shader source for arrow shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::cartoon::fragment", fragSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load fragment shader source for cartoon shader");
            return false;
        }
        this->m_arrowShader.Compile( vertSrc.Code(), vertSrc.Count(), geomSrc.Code(), geomSrc.Count(), fragSrc.Code(), fragSrc.Count());
        this->m_arrowShader.SetProgramParameter( GL_GEOMETRY_INPUT_TYPE_EXT , GL_TRIANGLES_ADJACENCY_EXT);
        this->m_arrowShader.SetProgramParameter( GL_GEOMETRY_OUTPUT_TYPE_EXT, GL_TRIANGLE_STRIP);
        this->m_arrowShader.SetProgramParameter( GL_GEOMETRY_VERTICES_OUT_EXT, 200);
        this->m_arrowShader.Link();

        /////////////////////////////////////////////////
        // load the shader sources for the helix shader //
        /////////////////////////////////////////////////
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::cartoon::vertex", vertSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load vertex shader source for cartoon shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::helixGeometry", geomSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load geometry shader source for helix shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::cartoon::fragment", fragSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load fragment shader source for cartoon shader");
            return false;
        }
        this->m_helixShader.Compile( vertSrc.Code(), vertSrc.Count(), geomSrc.Code(), geomSrc.Count(), fragSrc.Code(), fragSrc.Count());
        this->m_helixShader.SetProgramParameter( GL_GEOMETRY_INPUT_TYPE_EXT , GL_TRIANGLES_ADJACENCY_EXT);
        this->m_helixShader.SetProgramParameter( GL_GEOMETRY_OUTPUT_TYPE_EXT, GL_TRIANGLE_STRIP);
        this->m_helixShader.SetProgramParameter( GL_GEOMETRY_VERTICES_OUT_EXT, 200);
        this->m_helixShader.Link();
		
        /////////////////////////////////////////////////
        // load the shader sources for the tube shader //
        /////////////////////////////////////////////////
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::simple::vertex", vertSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load vertex shader source for simple cartoon shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::simple::tubeGeometry", geomSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load geometry shader source for simple tube shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::simple::fragment", fragSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load fragment shader source for simple cartoon shader");
            return false;
        }
        this->m_tubeSimpleShader.Compile( vertSrc.Code(), vertSrc.Count(), geomSrc.Code(), geomSrc.Count(), fragSrc.Code(), fragSrc.Count());
        this->m_tubeSimpleShader.SetProgramParameter( GL_GEOMETRY_INPUT_TYPE_EXT , GL_TRIANGLES_ADJACENCY_EXT);
        this->m_tubeSimpleShader.SetProgramParameter( GL_GEOMETRY_OUTPUT_TYPE_EXT, GL_TRIANGLE_STRIP);
        this->m_tubeSimpleShader.SetProgramParameter( GL_GEOMETRY_VERTICES_OUT_EXT, 200);
        this->m_tubeSimpleShader.Link();

        //////////////////////////////////////////////////
        // load the shader sources for the arrow shader //
        //////////////////////////////////////////////////
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::simple::vertex", vertSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load vertex shader source for simple cartoon shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::simple::arrowGeometry", geomSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load geometry shader source for simple arrow shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::simple::fragment", fragSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load fragment shader source for simple cartoon shader");
            return false;
        }
        this->m_arrowSimpleShader.Compile( vertSrc.Code(), vertSrc.Count(), geomSrc.Code(), geomSrc.Count(), fragSrc.Code(), fragSrc.Count());
        this->m_arrowSimpleShader.SetProgramParameter( GL_GEOMETRY_INPUT_TYPE_EXT , GL_TRIANGLES_ADJACENCY_EXT);
        this->m_arrowSimpleShader.SetProgramParameter( GL_GEOMETRY_OUTPUT_TYPE_EXT, GL_TRIANGLE_STRIP);
        this->m_arrowSimpleShader.SetProgramParameter( GL_GEOMETRY_VERTICES_OUT_EXT, 200);
        this->m_arrowSimpleShader.Link();

        /////////////////////////////////////////////////
        // load the shader sources for the helix shader //
        /////////////////////////////////////////////////
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::simple::vertex", vertSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load vertex shader source for simple cartoon shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::simple::helixGeometry", geomSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load geometry shader source for simple helix shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::simple::fragment", fragSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load fragment shader source for simple cartoon shader");
            return false;
        }
        this->m_helixSimpleShader.Compile( vertSrc.Code(), vertSrc.Count(), geomSrc.Code(), geomSrc.Count(), fragSrc.Code(), fragSrc.Count());
        this->m_helixSimpleShader.SetProgramParameter( GL_GEOMETRY_INPUT_TYPE_EXT , GL_TRIANGLES_ADJACENCY_EXT);
        this->m_helixSimpleShader.SetProgramParameter( GL_GEOMETRY_OUTPUT_TYPE_EXT, GL_TRIANGLE_STRIP);
        this->m_helixSimpleShader.SetProgramParameter( GL_GEOMETRY_VERTICES_OUT_EXT, 200);
        this->m_helixSimpleShader.Link();
		
        /////////////////////////////////////////////////////////
        // load the shader sources for the spline arrow shader //
        /////////////////////////////////////////////////////////
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::spline::vertex", vertSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load vertex shader source for spline cartoon shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::spline::arrowGeometry", geomSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load geometry shader source for spline arrow shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::spline::fragment", fragSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load fragment shader source for spline cartoon shader");
            return false;
        }
        this->m_arrowSplineShader.Compile( vertSrc.Code(), vertSrc.Count(), geomSrc.Code(), geomSrc.Count(), fragSrc.Code(), fragSrc.Count());
        this->m_arrowSplineShader.SetProgramParameter( GL_GEOMETRY_INPUT_TYPE_EXT , GL_LINES_ADJACENCY_EXT);
        this->m_arrowSplineShader.SetProgramParameter( GL_GEOMETRY_OUTPUT_TYPE_EXT, GL_TRIANGLE_STRIP);
        this->m_arrowSplineShader.SetProgramParameter( GL_GEOMETRY_VERTICES_OUT_EXT, 150);
        this->m_arrowSplineShader.Link();

        ////////////////////////////////////////////////////////
        // load the shader sources for the spline tube shader //
        ////////////////////////////////////////////////////////
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::spline::vertex", vertSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load vertex shader source for spline cartoon shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::spline::tubeGeometry", geomSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load geometry shader source for spline tube shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::spline::fragment", fragSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load fragment shader source for spline cartoon shader");
            return false;
        }
        this->m_tubeSplineShader.Compile( vertSrc.Code(), vertSrc.Count(), geomSrc.Code(), geomSrc.Count(), fragSrc.Code(), fragSrc.Count());
        this->m_tubeSplineShader.SetProgramParameter( GL_GEOMETRY_INPUT_TYPE_EXT , GL_LINES_ADJACENCY_EXT);
        this->m_tubeSplineShader.SetProgramParameter( GL_GEOMETRY_OUTPUT_TYPE_EXT, GL_TRIANGLE_STRIP);
        this->m_tubeSplineShader.SetProgramParameter( GL_GEOMETRY_VERTICES_OUT_EXT, 150);
        this->m_tubeSplineShader.Link();

        ////////////////////////////////////////////////////////
        // load the shader sources for the spline helix shader //
        ////////////////////////////////////////////////////////
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::spline::vertex", vertSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load vertex shader source for spline cartoon shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::spline::helixGeometry", geomSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load geometry shader source for spline helix shader");
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::spline::fragment", fragSrc)) {
            Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load fragment shader source for spline cartoon shader");
            return false;
        }
        this->m_helixSplineShader.Compile( vertSrc.Code(), vertSrc.Count(), geomSrc.Code(), geomSrc.Count(), fragSrc.Code(), fragSrc.Count());
        this->m_helixSplineShader.SetProgramParameter( GL_GEOMETRY_INPUT_TYPE_EXT , GL_LINES_ADJACENCY_EXT);
        this->m_helixSplineShader.SetProgramParameter( GL_GEOMETRY_OUTPUT_TYPE_EXT, GL_TRIANGLE_STRIP);
        this->m_helixSplineShader.SetProgramParameter( GL_GEOMETRY_VERTICES_OUT_EXT, 150);
        this->m_helixSplineShader.Link();

	}

	//////////////////////////////////////////////////////
	// load the shader files for the per pixel lighting //
	//////////////////////////////////////////////////////
	// vertex shader
    if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::perpixellight::vertex", vertSrc)) {
        Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load vertex shader source for perpixellight shader");
        return false;
    }
    if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource("protein::cartoon::perpixellight::fragment", fragSrc)) {
        Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to load fragment shader source for perpixellight shader");
        return false;
    }
    this->m_lightShader.Create(vertSrc.Code(), vertSrc.Count(), fragSrc.Code(), fragSrc.Count());

    return true;
}


/**********************************************************************
 * 'render'-functions
 **********************************************************************/

/*
 * protein::ProteinRendererCartoon::Render
 */
bool protein::ProteinRendererCartoon::Render(Call& call)
{
	// get pointer to CallProteinData
	protein::CallProteinData *protein = this->m_protDataCallerSlot.CallAs<protein::CallProteinData>();

    if (protein == NULL) 
        return false;

    // decide to use already loaded frame request from CallFrame or 'normal' rendering
    if(this->m_callFrameCalleeSlot.GetStatus() == AbstractSlot::STATUS_CONNECTED)
    {
        if(!this->m_renderRMSData)
            return false;
    }
    else
    {
        if(!(*protein)()) 
            return false;
    }

	// get camera information
	this->m_cameraInfo = dynamic_cast<view::CallRender*>(&call)->GetCameraParameters();

    // parameter refresh
    if (this->m_renderingModeParam.IsDirty()) 
	{
		this->SetRenderMode(static_cast<CartoonRenderMode>(int(this->m_renderingModeParam.Param<param::EnumParam>()->Value())));
		this->m_renderingModeParam.ResetDirty();
    }
    if (this->m_coloringModeParam.IsDirty()) 
	{
		this->SetColoringMode(static_cast<ColoringMode>(int(this->m_coloringModeParam.Param<param::EnumParam>()->Value())));
		this->m_coloringModeParam.ResetDirty();
    }
    if (this->m_smoothCartoonColoringParam.IsDirty()) 
	{
		this->m_smoothCartoonColoringMode = this->m_smoothCartoonColoringParam.Param<param::BoolParam>()->Value();
		this->m_smoothCartoonColoringParam.ResetDirty();
		if (this->m_currentRenderMode == CARTOON) 
		{
            //cartoonSplineCreated = false;
        }
    }

	// make the atom color table if necessary
	this->MakeColorTable(protein);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    glPushMatrix();

    float scale, xoff, yoff, zoff;
    vislib::math::Point<float, 3> bbc = protein->BoundingBox().CalcCenter();

    xoff = -bbc.X();
    yoff = -bbc.Y();
    zoff = -bbc.Z();

    scale = 2.0f / vislib::math::Max(vislib::math::Max(protein->BoundingBox().Width(),
        protein->BoundingBox().Height()), protein->BoundingBox().Depth());

    glScalef(scale, scale, scale);
    glTranslatef(xoff, yoff, zoff);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);

	if( ( m_currentRenderMode == CARTOON || m_currentRenderMode == CARTOON_SIMPLE ) && this->m_geomShaderSupported )
	{
		// ------------------------------------------------------------
		// --- CARTOON                                              ---
		// --- Hybrid Implementation using GLSL geometry shaders    ---
		// ------------------------------------------------------------
		this->RenderCartoonHybrid(protein);
	}

	if( m_currentRenderMode == CARTOON_CPU )
	{
		// ------------------------------------------------------------
		// --- CARTOON_CPU                                          ---
		// --- render the protein using OpenGL primitives           ---
		// ------------------------------------------------------------
		this->RenderCartoonCPU(protein);
	}

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);

	glPopMatrix();

    // render label if RMS is used
    if(this->m_renderRMSData)
        this->DrawLabel(protein->GetRequestedRMSFrame());

    return true;
}


/*
 * protein::ProteinRendererCartoon::ProcessFrameRequest
 */
bool protein::ProteinRendererCartoon::ProcessFrameRequest(Call& call)
{
	// get pointer to CallProteinData
	protein::CallProteinData *protein = this->m_protDataCallerSlot.CallAs<protein::CallProteinData>();

    // ensure that NetCDFData uses 'RMS' specific frame handling
    protein->SetRMSUse(true);

    // get pointer to frame call
    protein::CallFrame *pcf = dynamic_cast<protein::CallFrame*>(&call);

    if(pcf->NewRequest())
    {
        // pipe frame request from frame call to protein call
        protein->SetRequestedRMSFrame(pcf->GetFrameRequest());
        if(!(*protein)())
        {
            this->m_renderRMSData = false;
            return false;
        }
        this->m_renderRMSData = true;
    }

    return true;
}


/**
 * protein::ProteinRendererCartoon::DrawLabel
 */
void protein::ProteinRendererCartoon::DrawLabel(unsigned int frameID)
{
    using namespace vislib::graphics;
    char frameChar[10];

    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

        glTranslatef(-1.0f, 1.0f, 1.0f);

        glColor3f(1.0, 1.0, 1.0);
        if (this->m_frameLabel == NULL) 
        {
            this->m_frameLabel = new vislib::graphics::gl::SimpleFont();
            if(!this->m_frameLabel->Initialise())
            {
                vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_WARN, "ProteinRenderer: Problems to initalise the Font");
            }
        }
#ifdef _WIN32
        _itoa_s(frameID, frameChar, 10, 10);
#else  /* _WIN32 */
        vislib::StringA tmp; /* worst idea ever, but linux does not deserve anything better! */
        tmp.Format("%i", frameID);
        memcpy(frameChar, tmp.PeekBuffer(), 10);
#endif /* _WIN32 */

        this->m_frameLabel->DrawString(0.0f, 0.0f, 0.1f, true, (vislib::StringA("Frame: ") + frameChar).PeekBuffer() , AbstractFont::ALIGN_LEFT_TOP);

    glPopMatrix();

    glPopAttrib();
}


/*
 * protein::ProteinRendererCartoon::RenderCartoonHybrid
 */
void protein::ProteinRendererCartoon::RenderCartoonHybrid( 
	const CallProteinData *prot)
{
	// return if geometry shaders are not supported
	if( !this->m_geomShaderSupported )
		return;
	// prepare hybrid cartoon representation, if necessary
	if( this->m_prepareCartoonHybrid )
	{
		unsigned int cntChain, cntS, cntAA, idx, idxAA;
		protein::CallProteinData::Chain chain;
		// B-Spline
		BSpline bSpline;
		// control points for the first (center) b-spline
		std::vector<vislib::math::Vector<float, 3> > controlPoints;
		// control points for the second (direction) b-spline
		std::vector<vislib::math::Vector<float, 3> > controlPointsDir;
		// temporary vectors
		vislib::math::Vector<float, 3> vecCA, vecC, vecO, vecTmp, vecTmpOld;
		// temporary color
		const unsigned char *color;
		// temporary color vector
		vislib::math::Vector<float, 3> colorVec;

		// coordinates of the first (center) b-spline (result of the spline computation)
		std::vector<std::vector<vislib::math::Vector<float, 3> > > bSplineCoords;
		// coordinates of the second (direction) b-spline (result of the spline computation)
		std::vector<std::vector<vislib::math::Vector<float, 3> > > bSplineCoordsDir;
		// secondary structure type for b-spline
		std::vector<std::vector<protein::CallProteinData::SecStructure::ElementType> > bSplineSecStruct;
		// color of secondary structure b-spline
		std::vector<std::vector<vislib::math::Vector<float, 3> > > cartoonColor;

		// set the number of segments to create
		bSpline.setN( m_numberOfSplineSeg);

		// resize result vector for coordinates of first b-spline segments
		bSplineCoords.resize( prot->ProteinChainCount());
		// resize result vector for coordinates of second b-spline segments
		bSplineCoordsDir.resize( prot->ProteinChainCount());
		// resize vector for secondary structure
		bSplineSecStruct.resize( prot->ProteinChainCount());
		// resize color vector
		cartoonColor.resize( prot->ProteinChainCount());

		// --- compute the b-splines ---
		// loop over all chains
		for( cntChain = 0; cntChain < prot->ProteinChainCount(); cntChain++ )
		{
			chain = prot->ProteinChain( cntChain);
			controlPoints.clear();
			controlPointsDir.clear();
			// loop over all secondary structure elements
			for( cntS = 0; cntS < chain.SecondaryStructureCount(); cntS++ )
			{
				// loop over all amino acids in the current sec struct
				for( cntAA = 0; cntAA < chain.SecondaryStructure()[cntS].AminoAcidCount(); cntAA++ )
				{
					// add sec struct type
					bSplineSecStruct[cntChain].push_back( chain.SecondaryStructure()[cntS].Type());

					// compute absolute index of current amino acid
					idxAA = cntAA + chain.SecondaryStructure()[cntS].FirstAminoAcidIndex();
					// get the index of the C-alpha atom
					idx = chain.AminoAcid()[idxAA].CAlphaIndex() + chain.AminoAcid()[idxAA].FirstAtomIndex();
					// get the coordinates of the C-alpha atom
					vecCA.SetX( prot->ProteinAtomPositions()[idx*3+0]);
					vecCA.SetY( prot->ProteinAtomPositions()[idx*3+1]);
					vecCA.SetZ( prot->ProteinAtomPositions()[idx*3+2]);
					// add the C-alpha coords to the list of control points
					controlPoints.push_back( vecCA);

					// add the color of the C-alpha atom to the color vector
					color = this->GetProteinAtomColor( idx);
					colorVec.SetX( float((int)color[0])/255.0f);
					colorVec.SetY( float((int)color[1])/255.0f);
					colorVec.SetZ( float((int)color[2])/255.0f);
					cartoonColor[cntChain].push_back( colorVec);
					
					// get the index of the C atom
					idx = chain.AminoAcid()[idxAA].CCarbIndex() + chain.AminoAcid()[idxAA].FirstAtomIndex();
					// get the coordinates of the C-alpha atom
					vecC.SetX( prot->ProteinAtomPositions()[idx*3+0]);
					vecC.SetY( prot->ProteinAtomPositions()[idx*3+1]);
					vecC.SetZ( prot->ProteinAtomPositions()[idx*3+2]);

					// get the index of the O atom
					idx = chain.AminoAcid()[idxAA].OIndex() + chain.AminoAcid()[idxAA].FirstAtomIndex();
					// get the coordinates of the C-alpha atom
					vecO.SetX( prot->ProteinAtomPositions()[idx*3+0]);
					vecO.SetY( prot->ProteinAtomPositions()[idx*3+1]);
					vecO.SetZ( prot->ProteinAtomPositions()[idx*3+2]);

					// compute control point of the second b-spline
					vecTmp = vecO - vecC;
					vecTmp.Normalise();
					// check, if vector should be flipped
					if( cntS > 0 && vecTmpOld.Dot( vecTmp) < 0.0f )
						vecTmp = vecTmp * -1.0f;
					vecTmpOld = vecTmp;
					// add control point for the second b-spline to the list of control points
					controlPointsDir.push_back( vecTmp + vecCA);
				}
			}
			// set the control points, compute the first spline and fetch the result
			bSpline.setBackbone( controlPoints);
			if( bSpline.computeSpline() )
				bSpline.getResult( bSplineCoords[cntChain]);
			else
				return; // --> return if spline could not be computed

			// set the control points, compute the second spline and fetch the result
			bSpline.setBackbone( controlPointsDir);
			if( bSpline.computeSpline() )
				bSpline.getResult( bSplineCoordsDir[cntChain]);
			else
				return; // --> return if spline could not be computed
		}

		// --- START store the vertices, colors and parameters ---
		m_totalCountTube = 0;
		m_totalCountArrow = 0;
		m_totalCountHelix = 0;
		for( unsigned int i = 0; i < bSplineCoords.size(); i++ )
		{
			for( unsigned int j = 2; j < bSplineCoords[i].size()-1; j++ )
			{
				if( bSplineSecStruct[i][j/m_numberOfSplineSeg] == protein::CallProteinData::SecStructure::TYPE_SHEET )
					m_totalCountArrow++;
				else if( bSplineSecStruct[i][j/m_numberOfSplineSeg] == protein::CallProteinData::SecStructure::TYPE_HELIX )
					m_totalCountHelix++;
				else
					m_totalCountTube++;
			}
		}

		if( this->m_vertTube )
			delete[] this->m_vertTube;
		if( this->m_colorsParamsTube )
			delete[] this->m_colorsParamsTube;
		if( this->m_vertHelix )
			delete[] this->m_vertHelix;
		if( this->m_colorsParamsHelix )
			delete[] this->m_colorsParamsHelix;
		if( this->m_vertArrow )
			delete[] this->m_vertArrow;
		if( this->m_colorsParamsArrow )
			delete[] this->m_colorsParamsArrow;
		this->m_vertTube = new float[m_totalCountTube*6*3];
		this->m_colorsParamsTube = new float[m_totalCountTube*6*3];
		this->m_vertArrow = new float[m_totalCountArrow*6*3];
		this->m_colorsParamsArrow = new float[m_totalCountArrow*6*3];
		this->m_vertHelix = new float[m_totalCountHelix*6*3];
		this->m_colorsParamsHelix = new float[m_totalCountHelix*6*3];

		// auxiliary variables
		float start, end, f1, f2, type;
		unsigned int counterTube = 0;
		unsigned int counterArrow = 0;
		unsigned int counterHelix = 0;
		vislib::math::Vector<float,3> col1, col2;
		// compute the inner b-spline (backbone)
		for( unsigned int i = 0; i < bSplineCoords.size(); i++ )
		{
			for( unsigned int j = 2; j < bSplineCoords[i].size()-1; j++ )
			{
				start = end = -1.0f;
				f1 = f2 = 1.0f;
				// set end caps --> if it is the first segment and the last sec struct was different
				if( j/m_numberOfSplineSeg > 0 )
				{
					if( bSplineSecStruct[i][j/m_numberOfSplineSeg] != bSplineSecStruct[i][j/m_numberOfSplineSeg-1] && 
						j%m_numberOfSplineSeg == 0 )
						end = 1.0f;
				}
				else if( j == 2 )
					end = 1.0f;
				// set start caps --> if its the last segment and the next sec struct is different
				if( j/m_numberOfSplineSeg < bSplineSecStruct[i].size()-1 )
				{
					if( bSplineSecStruct[i][j/m_numberOfSplineSeg] != bSplineSecStruct[i][j/m_numberOfSplineSeg+1] && 
						j%m_numberOfSplineSeg == m_numberOfSplineSeg-1 )
						start = 1.0f;
				}
				else if( j == bSplineCoords[i].size()-2 )
					start = 1.0f;
				// set inParams --> set type and stretch factors of arrow head segments for the sheet
				if( bSplineSecStruct[i][j/m_numberOfSplineSeg] == protein::CallProteinData::SecStructure::TYPE_SHEET )
				{
					type = 1.0f;
					if( bSplineSecStruct[i][j/m_numberOfSplineSeg+1] != protein::CallProteinData::SecStructure::TYPE_SHEET )
					{
						if(  j%m_numberOfSplineSeg == 0 )
							end = 1.0f;
						f1 = 1.0f - float(j%m_numberOfSplineSeg)/float(m_numberOfSplineSeg-1)+1.0f/float(m_numberOfSplineSeg-1)+0.2f;
						f2 = 1.0f - float(j%m_numberOfSplineSeg)/float(m_numberOfSplineSeg-1)+0.2f;
					}
				}
				else if( bSplineSecStruct[i][j/m_numberOfSplineSeg] == protein::CallProteinData::SecStructure::TYPE_HELIX )
					type = 2.0f;
				else
					type = 0.0f;
				// get the colors
				if( this->m_smoothCartoonColoringMode && j/m_numberOfSplineSeg > 0 )
				{
					col1 = cartoonColor[i][j/m_numberOfSplineSeg]*float(j%m_numberOfSplineSeg)/float(m_numberOfSplineSeg-1)
						+ cartoonColor[i][j/m_numberOfSplineSeg-1]*float((m_numberOfSplineSeg-1)-j%m_numberOfSplineSeg)/float(m_numberOfSplineSeg-1);
					int k = j+1;
					if( j%m_numberOfSplineSeg == m_numberOfSplineSeg-1 )
						k = m_numberOfSplineSeg-1;
					col2 = cartoonColor[i][j/m_numberOfSplineSeg]*float(k%m_numberOfSplineSeg)/float(m_numberOfSplineSeg-1)
						+ cartoonColor[i][j/m_numberOfSplineSeg-1]*float((m_numberOfSplineSeg-1)-k%m_numberOfSplineSeg)/float(m_numberOfSplineSeg-1);
				}
				else
				{
					col1 = cartoonColor[i][j/m_numberOfSplineSeg];
					col2 = cartoonColor[i][j/m_numberOfSplineSeg];
				}

				// store information in the apropriate arrays
				if( bSplineSecStruct[i][j/m_numberOfSplineSeg] == protein::CallProteinData::SecStructure::TYPE_SHEET )
				{
					this->m_colorsParamsArrow[counterArrow*6*3+0] = col1.GetX();
					this->m_colorsParamsArrow[counterArrow*6*3+1] = col1.GetY();
					this->m_colorsParamsArrow[counterArrow*6*3+2] = col1.GetZ();
					this->m_colorsParamsArrow[counterArrow*6*3+3] = m_radiusCartoon;
					this->m_colorsParamsArrow[counterArrow*6*3+4] = f1;
					this->m_colorsParamsArrow[counterArrow*6*3+5] = f2;
					this->m_colorsParamsArrow[counterArrow*6*3+6] = type;
					this->m_colorsParamsArrow[counterArrow*6*3+7] = start;
					this->m_colorsParamsArrow[counterArrow*6*3+8] = end;
					this->m_colorsParamsArrow[counterArrow*6*3+9]  = col2.GetX();
					this->m_colorsParamsArrow[counterArrow*6*3+10] = col2.GetY();
					this->m_colorsParamsArrow[counterArrow*6*3+11] = col2.GetZ();
					this->m_colorsParamsArrow[counterArrow*6*3+12] = 0.0f;
					this->m_colorsParamsArrow[counterArrow*6*3+13] = 0.0f;
					this->m_colorsParamsArrow[counterArrow*6*3+14] = 0.0f;
					this->m_colorsParamsArrow[counterArrow*6*3+15] = 0.0f;
					this->m_colorsParamsArrow[counterArrow*6*3+16] = 0.0f;
					this->m_colorsParamsArrow[counterArrow*6*3+17] = 0.0f;
					this->m_vertArrow[counterArrow*6*3+0] = bSplineCoords[i][j-2].GetX();
					this->m_vertArrow[counterArrow*6*3+1] = bSplineCoords[i][j-2].GetY();
					this->m_vertArrow[counterArrow*6*3+2] = bSplineCoords[i][j-2].GetZ();
					this->m_vertArrow[counterArrow*6*3+3] = bSplineCoordsDir[i][j-1].GetX();
					this->m_vertArrow[counterArrow*6*3+4] = bSplineCoordsDir[i][j-1].GetY();
					this->m_vertArrow[counterArrow*6*3+5] = bSplineCoordsDir[i][j-1].GetZ();
					this->m_vertArrow[counterArrow*6*3+6] = bSplineCoords[i][j-1].GetX();
					this->m_vertArrow[counterArrow*6*3+7] = bSplineCoords[i][j-1].GetY();
					this->m_vertArrow[counterArrow*6*3+8] = bSplineCoords[i][j-1].GetZ();
					this->m_vertArrow[counterArrow*6*3+9] = bSplineCoords[i][j].GetX();
					this->m_vertArrow[counterArrow*6*3+10] = bSplineCoords[i][j].GetY();
					this->m_vertArrow[counterArrow*6*3+11] = bSplineCoords[i][j].GetZ();
					this->m_vertArrow[counterArrow*6*3+12] = bSplineCoordsDir[i][j].GetX();
					this->m_vertArrow[counterArrow*6*3+13] = bSplineCoordsDir[i][j].GetY();
					this->m_vertArrow[counterArrow*6*3+14] = bSplineCoordsDir[i][j].GetZ();
					this->m_vertArrow[counterArrow*6*3+15] = bSplineCoords[i][j+1].GetX();
					this->m_vertArrow[counterArrow*6*3+16] = bSplineCoords[i][j+1].GetY();
					this->m_vertArrow[counterArrow*6*3+17] = bSplineCoords[i][j+1].GetZ();
					counterArrow++;
				}
				else if( bSplineSecStruct[i][j/m_numberOfSplineSeg] == protein::CallProteinData::SecStructure::TYPE_HELIX )
				{
					this->m_colorsParamsHelix[counterHelix*6*3+0] = col1.GetX();
					this->m_colorsParamsHelix[counterHelix*6*3+1] = col1.GetY();
					this->m_colorsParamsHelix[counterHelix*6*3+2] = col1.GetZ();
					this->m_colorsParamsHelix[counterHelix*6*3+3] = m_radiusCartoon;
					this->m_colorsParamsHelix[counterHelix*6*3+4] = f1;
					this->m_colorsParamsHelix[counterHelix*6*3+5] = f2;
					this->m_colorsParamsHelix[counterHelix*6*3+6] = type;
					this->m_colorsParamsHelix[counterHelix*6*3+7] = start;
					this->m_colorsParamsHelix[counterHelix*6*3+8] = end;
					this->m_colorsParamsHelix[counterHelix*6*3+9]  = col2.GetX();
					this->m_colorsParamsHelix[counterHelix*6*3+10] = col2.GetY();
					this->m_colorsParamsHelix[counterHelix*6*3+11] = col2.GetZ();
					this->m_colorsParamsHelix[counterHelix*6*3+12] = 0.0f;
					this->m_colorsParamsHelix[counterHelix*6*3+13] = 0.0f;
					this->m_colorsParamsHelix[counterHelix*6*3+14] = 0.0f;
					this->m_colorsParamsHelix[counterHelix*6*3+15] = 0.0f;
					this->m_colorsParamsHelix[counterHelix*6*3+16] = 0.0f;
					this->m_colorsParamsHelix[counterHelix*6*3+17] = 0.0f;
					this->m_vertHelix[counterHelix*6*3+0] = bSplineCoords[i][j-2].GetX();
					this->m_vertHelix[counterHelix*6*3+1] = bSplineCoords[i][j-2].GetY();
					this->m_vertHelix[counterHelix*6*3+2] = bSplineCoords[i][j-2].GetZ();
					this->m_vertHelix[counterHelix*6*3+3] = bSplineCoordsDir[i][j-1].GetX();
					this->m_vertHelix[counterHelix*6*3+4] = bSplineCoordsDir[i][j-1].GetY();
					this->m_vertHelix[counterHelix*6*3+5] = bSplineCoordsDir[i][j-1].GetZ();
					this->m_vertHelix[counterHelix*6*3+6] = bSplineCoords[i][j-1].GetX();
					this->m_vertHelix[counterHelix*6*3+7] = bSplineCoords[i][j-1].GetY();
					this->m_vertHelix[counterHelix*6*3+8] = bSplineCoords[i][j-1].GetZ();
					this->m_vertHelix[counterHelix*6*3+9] = bSplineCoords[i][j].GetX();
					this->m_vertHelix[counterHelix*6*3+10] = bSplineCoords[i][j].GetY();
					this->m_vertHelix[counterHelix*6*3+11] = bSplineCoords[i][j].GetZ();
					this->m_vertHelix[counterHelix*6*3+12] = bSplineCoordsDir[i][j].GetX();
					this->m_vertHelix[counterHelix*6*3+13] = bSplineCoordsDir[i][j].GetY();
					this->m_vertHelix[counterHelix*6*3+14] = bSplineCoordsDir[i][j].GetZ();
					this->m_vertHelix[counterHelix*6*3+15] = bSplineCoords[i][j+1].GetX();
					this->m_vertHelix[counterHelix*6*3+16] = bSplineCoords[i][j+1].GetY();
					this->m_vertHelix[counterHelix*6*3+17] = bSplineCoords[i][j+1].GetZ();
					counterHelix++;
				}
				else
				{
					this->m_colorsParamsTube[counterTube*6*3+0] = col1.GetX();
					this->m_colorsParamsTube[counterTube*6*3+1] = col1.GetY();
					this->m_colorsParamsTube[counterTube*6*3+2] = col1.GetZ();
					this->m_colorsParamsTube[counterTube*6*3+3] = m_radiusCartoon;
					this->m_colorsParamsTube[counterTube*6*3+4] = f1;
					this->m_colorsParamsTube[counterTube*6*3+5] = f2;
					this->m_colorsParamsTube[counterTube*6*3+6] = type;
					this->m_colorsParamsTube[counterTube*6*3+7] = start;
					this->m_colorsParamsTube[counterTube*6*3+8] = end;
					this->m_colorsParamsTube[counterTube*6*3+9]  = col2.GetX();
					this->m_colorsParamsTube[counterTube*6*3+10] = col2.GetY();
					this->m_colorsParamsTube[counterTube*6*3+11] = col2.GetZ();
					this->m_colorsParamsTube[counterTube*6*3+12] = 0.0f;
					this->m_colorsParamsTube[counterTube*6*3+13] = 0.0f;
					this->m_colorsParamsTube[counterTube*6*3+14] = 0.0f;
					this->m_colorsParamsTube[counterTube*6*3+15] = 0.0f;
					this->m_colorsParamsTube[counterTube*6*3+16] = 0.0f;
					this->m_colorsParamsTube[counterTube*6*3+17] = 0.0f;
					this->m_vertTube[counterTube*6*3+0] = bSplineCoords[i][j-2].GetX();
					this->m_vertTube[counterTube*6*3+1] = bSplineCoords[i][j-2].GetY();
					this->m_vertTube[counterTube*6*3+2] = bSplineCoords[i][j-2].GetZ();
					this->m_vertTube[counterTube*6*3+3] = bSplineCoordsDir[i][j-1].GetX();
					this->m_vertTube[counterTube*6*3+4] = bSplineCoordsDir[i][j-1].GetY();
					this->m_vertTube[counterTube*6*3+5] = bSplineCoordsDir[i][j-1].GetZ();
					this->m_vertTube[counterTube*6*3+6] = bSplineCoords[i][j-1].GetX();
					this->m_vertTube[counterTube*6*3+7] = bSplineCoords[i][j-1].GetY();
					this->m_vertTube[counterTube*6*3+8] = bSplineCoords[i][j-1].GetZ();
					this->m_vertTube[counterTube*6*3+9] = bSplineCoords[i][j].GetX();
					this->m_vertTube[counterTube*6*3+10] = bSplineCoords[i][j].GetY();
					this->m_vertTube[counterTube*6*3+11] = bSplineCoords[i][j].GetZ();
					this->m_vertTube[counterTube*6*3+12] = bSplineCoordsDir[i][j].GetX();
					this->m_vertTube[counterTube*6*3+13] = bSplineCoordsDir[i][j].GetY();
					this->m_vertTube[counterTube*6*3+14] = bSplineCoordsDir[i][j].GetZ();
					this->m_vertTube[counterTube*6*3+15] = bSplineCoords[i][j+1].GetX();
					this->m_vertTube[counterTube*6*3+16] = bSplineCoords[i][j+1].GetY();
					this->m_vertTube[counterTube*6*3+17] = bSplineCoords[i][j+1].GetZ();
					counterTube++;
				}
			}
		}

		// --- END store vertex/color/inparams ---

		// set cartoon as created
		this->m_prepareCartoonHybrid = false;
	}

	float spec[4] = { 1.0f, 1.0f, 1.0f, 1.0f};
	glMaterialfv( GL_FRONT_AND_BACK, GL_SPECULAR, spec);
	glMaterialf( GL_FRONT_AND_BACK, GL_SHININESS, 50.0f);
	glEnable( GL_COLOR_MATERIAL);

	// enable tube shader
	if( m_currentRenderMode == CARTOON )
		this->m_tubeShader.Enable();
	else
		this->m_tubeSimpleShader.Enable();
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer( 3, GL_FLOAT, 0, this->m_vertTube);
	glColorPointer( 3, GL_FLOAT, 0, this->m_colorsParamsTube);
	glDrawArrays( GL_TRIANGLES_ADJACENCY_EXT, 0, m_totalCountTube*6);
	// disable tube shader
	if( m_currentRenderMode == CARTOON )
		this->m_tubeShader.Disable();
	else
		this->m_tubeSimpleShader.Disable();

	// enable arrow shader
	if( m_currentRenderMode == CARTOON )
		this->m_arrowShader.Enable();
	else
		this->m_arrowSimpleShader.Enable();
	glVertexPointer( 3, GL_FLOAT, 0, this->m_vertArrow);
	glColorPointer( 3, GL_FLOAT, 0, this->m_colorsParamsArrow);
	glDrawArrays( GL_TRIANGLES_ADJACENCY_EXT, 0, m_totalCountArrow*6);
	// disable arrow shader
	if( m_currentRenderMode == CARTOON )
		this->m_arrowShader.Disable();
	else
		this->m_arrowSimpleShader.Disable();
	
	// enable helix shader
	if( m_currentRenderMode == CARTOON )
		this->m_helixShader.Enable();
	else
		this->m_helixSimpleShader.Enable();
	glVertexPointer( 3, GL_FLOAT, 0, this->m_vertHelix);
	glColorPointer( 3, GL_FLOAT, 0, this->m_colorsParamsHelix);
	glDrawArrays( GL_TRIANGLES_ADJACENCY_EXT, 0, m_totalCountHelix*6);
	// disable helix shader
	if( m_currentRenderMode == CARTOON )
		this->m_helixShader.Disable();
	else
		this->m_helixSimpleShader.Disable();

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
	glDisable( GL_COLOR_MATERIAL);
}


/*
 * protein::ProteinRendererCartoon::RenderCartoonCPU
 */
void protein::ProteinRendererCartoon::RenderCartoonCPU( 
    const CallProteinData *prot) {
//{
//	// return if geometry shaders are not supported
//	if( !this->m_geomShaderSupported )
//		return;

	// prepare hybrid cartoon representation, if necessary
	if( this->m_prepareCartoonCPU )
	{
		unsigned int cntChain, cntS, cntAA, idx, idxAA;
		protein::CallProteinData::Chain chain;
		// B-Spline
		BSpline bSpline;
		// control points for the first (center) b-spline
		std::vector<vislib::math::Vector<float, 3> > controlPoints;
		// control points for the second (direction) b-spline
		std::vector<vislib::math::Vector<float, 3> > controlPointsDir;
		// temporary vectors
		vislib::math::Vector<float, 3> vecCA, vecC, vecO, vecTmp, vecTmpOld;
		// temporary color
		const unsigned char *color;
		// temporary color vector
		vislib::math::Vector<float, 3> colorVec;

		// coordinates of the first (center) b-spline (result of the spline computation)
		std::vector<std::vector<vislib::math::Vector<float, 3> > > bSplineCoords;
		// coordinates of the second (direction) b-spline (result of the spline computation)
		std::vector<std::vector<vislib::math::Vector<float, 3> > > bSplineCoordsDir;
		// secondary structure type for b-spline
		std::vector<std::vector<protein::CallProteinData::SecStructure::ElementType> > bSplineSecStruct;
		// color of secondary structure b-spline
		std::vector<std::vector<vislib::math::Vector<float, 3> > > cartoonColor;

		// set the number of segments to create
		bSpline.setN( m_numberOfSplineSeg);

		// resize result vector for coordinates of first b-spline segments
		bSplineCoords.resize( prot->ProteinChainCount());
		// resize result vector for coordinates of second b-spline segments
		bSplineCoordsDir.resize( prot->ProteinChainCount());
		// resize vector for secondary structure
		bSplineSecStruct.resize( prot->ProteinChainCount());
		// resize color vector
		cartoonColor.resize( prot->ProteinChainCount());

		// --- compute the b-splines ---
		// loop over all chains
		for( cntChain = 0; cntChain < prot->ProteinChainCount(); cntChain++ )
		{
			chain = prot->ProteinChain( cntChain);
			controlPoints.clear();
			controlPointsDir.clear();
			// loop over all secondary structure elements
			for( cntS = 0; cntS < chain.SecondaryStructureCount(); cntS++ )
			{
				// loop over all amino acids in the current sec struct
				for( cntAA = 0; cntAA < chain.SecondaryStructure()[cntS].AminoAcidCount(); cntAA++ )
				{
					// add sec struct type
					bSplineSecStruct[cntChain].push_back( chain.SecondaryStructure()[cntS].Type());

					// compute absolute index of current amino acid
					idxAA = cntAA + chain.SecondaryStructure()[cntS].FirstAminoAcidIndex();
					// get the index of the C-alpha atom
					idx = chain.AminoAcid()[idxAA].CAlphaIndex() + chain.AminoAcid()[idxAA].FirstAtomIndex();
					// get the coordinates of the C-alpha atom
					vecCA.SetX( prot->ProteinAtomPositions()[idx*3+0]);
					vecCA.SetY( prot->ProteinAtomPositions()[idx*3+1]);
					vecCA.SetZ( prot->ProteinAtomPositions()[idx*3+2]);
					// add the C-alpha coords to the list of control points
					controlPoints.push_back( vecCA);

					// add the color of the C-alpha atom to the color vector
					color = this->GetProteinAtomColor( idx);
					colorVec.SetX( float((int)color[0])/255.0f);
					colorVec.SetY( float((int)color[1])/255.0f);
					colorVec.SetZ( float((int)color[2])/255.0f);
					cartoonColor[cntChain].push_back( colorVec);
					
					// get the index of the C atom
					idx = chain.AminoAcid()[idxAA].CCarbIndex() + chain.AminoAcid()[idxAA].FirstAtomIndex();
					// get the coordinates of the C-alpha atom
					vecC.SetX( prot->ProteinAtomPositions()[idx*3+0]);
					vecC.SetY( prot->ProteinAtomPositions()[idx*3+1]);
					vecC.SetZ( prot->ProteinAtomPositions()[idx*3+2]);

					// get the index of the O atom
					idx = chain.AminoAcid()[idxAA].OIndex() + chain.AminoAcid()[idxAA].FirstAtomIndex();
					// get the coordinates of the C-alpha atom
					vecO.SetX( prot->ProteinAtomPositions()[idx*3+0]);
					vecO.SetY( prot->ProteinAtomPositions()[idx*3+1]);
					vecO.SetZ( prot->ProteinAtomPositions()[idx*3+2]);

					// compute control point of the second b-spline
					vecTmp = vecO - vecC;
					vecTmp.Normalise();
					// check, if vector should be flipped
					if( cntS > 0 && vecTmpOld.Dot( vecTmp) < 0.0f )
						vecTmp = vecTmp * -1.0f;
					vecTmpOld = vecTmp;
					// add control point for the second b-spline to the list of control points
					controlPointsDir.push_back( vecTmp + vecCA);
				}
			}
			// set the control points, compute the first spline and fetch the result
			bSpline.setBackbone( controlPoints);
			if( bSpline.computeSpline() )
				bSpline.getResult( bSplineCoords[cntChain]);
			else
				return; // --> return if spline could not be computed

			// set the control points, compute the second spline and fetch the result
			bSpline.setBackbone( controlPointsDir);
			if( bSpline.computeSpline() )
				bSpline.getResult( bSplineCoordsDir[cntChain]);
			else
				return; // --> return if spline could not be computed
		}

		// --- START store the vertices, colors and parameters ---
		m_totalCountTube = 0;
		m_totalCountArrow = 0;
		m_totalCountHelix = 0;
		for( unsigned int i = 0; i < bSplineCoords.size(); i++ )
		{
			for( unsigned int j = 2; j < bSplineCoords[i].size()-1; j++ )
			{
				if( bSplineSecStruct[i][j/m_numberOfSplineSeg] == protein::CallProteinData::SecStructure::TYPE_SHEET )
					m_totalCountArrow++;
				else if( bSplineSecStruct[i][j/m_numberOfSplineSeg] == protein::CallProteinData::SecStructure::TYPE_HELIX )
					m_totalCountHelix++;
				else
					m_totalCountTube++;
			}
		}

		if( this->m_vertTube )
			delete[] this->m_vertTube;
		if( this->m_colorsParamsTube )
			delete[] this->m_colorsParamsTube;
		if( this->m_vertHelix )
			delete[] this->m_vertHelix;
		if( this->m_colorsParamsHelix )
			delete[] this->m_colorsParamsHelix;
		if( this->m_vertArrow )
			delete[] this->m_vertArrow;
		if( this->m_colorsParamsArrow )
			delete[] this->m_colorsParamsArrow;
		if( this->m_normalTube )
			delete[] this->m_normalTube;
		if( this->m_normalHelix )
			delete[] this->m_normalHelix;
		if( this->m_normalArrow )
			delete[] this->m_normalArrow;
		this->m_vertTube = new float[m_totalCountTube*3*4*m_numberOfTubeSeg];
		this->m_colorsParamsTube = new float[m_totalCountTube*3*4*m_numberOfTubeSeg];
		// 4 3D-Punkte pro Quad, 4 Quads pro Segment, d.h. 16 3D-Punkte pro Segment
		this->m_vertArrow = new float[m_totalCountArrow*3*16];
		this->m_colorsParamsArrow = new float[m_totalCountArrow*3*16];
		this->m_vertHelix = new float[m_totalCountHelix*3*16];
		this->m_colorsParamsHelix = new float[m_totalCountHelix*3*16];
		this->m_normalTube = new float[m_totalCountTube*3*4*m_numberOfTubeSeg];
		this->m_normalHelix = new float[m_totalCountHelix*3*16];
		this->m_normalArrow = new float[m_totalCountArrow*3*16];

		// auxiliary variables
		float start, end, f1, f2, type;
		unsigned int counterTube = 0;
		unsigned int counterArrow = 0;
		unsigned int counterHelix = 0;
		vislib::math::Vector<float,3> col1, col2;

		vislib::math::Vector<float,3> v0;
		vislib::math::Vector<float,3> v1;
		vislib::math::Vector<float,3> v2;
		vislib::math::Vector<float,3> v3;
		vislib::math::Vector<float,3> v4;
		vislib::math::Vector<float,3> v5;
		vislib::math::Vector<float,3> dir20;
		vislib::math::Vector<float,3> dir12;
		vislib::math::Vector<float,3> dir32;
		vislib::math::Vector<float,3> dir43;
		vislib::math::Vector<float,3> dir53;
		vislib::math::Vector<float,3> res1;
		vislib::math::Vector<float,3> res2;
		float scale;
		float stretch1;
		float stretch2;
		vislib::math::Vector<float,3> ortho1;
		vislib::math::Vector<float,3> ortho2;
		vislib::math::Vector<float,3> dir1;
		vislib::math::Vector<float,3> dir2;
		vislib::math::Vector<float,3> norm1;
		vislib::math::Vector<float,3> norm2;
		vislib::math::Quaternion<float> q1;
		vislib::math::Quaternion<float> q2;
		// angle for the rotation
		float alpha;

		// compute the geometry
		for( unsigned int i = 0; i < bSplineCoords.size(); i++ )
		{
			for( unsigned int j = 2; j < bSplineCoords[i].size()-1; j++ )
			{
				start = end = -1.0f;
				f1 = f2 = 1.0f;
				// set end caps --> if it is the first segment and the last sec struct was different
				if( j/m_numberOfSplineSeg > 0 )
				{
					if( bSplineSecStruct[i][j/m_numberOfSplineSeg] != bSplineSecStruct[i][j/m_numberOfSplineSeg-1] && j%m_numberOfSplineSeg == 0 )
						end = 1.0f;
				}
				else if( j == 2 )
					end = 1.0f;
				// set start caps --> if its the last segment and the next sec struct is different
				if( j/m_numberOfSplineSeg < bSplineSecStruct[i].size()-1 )
				{
					if( bSplineSecStruct[i][j/m_numberOfSplineSeg] != bSplineSecStruct[i][j/m_numberOfSplineSeg+1] && j%m_numberOfSplineSeg == m_numberOfSplineSeg-1 )
						start = 1.0f;
				}
				else if( j == bSplineCoords[i].size()-2 )
					start = 1.0f;
				// set inParams --> set type and stretch factors of arrow head segments for the sheet
				if( bSplineSecStruct[i][j/m_numberOfSplineSeg] == protein::CallProteinData::SecStructure::TYPE_SHEET )
				{
					type = 1.0f;
					if( bSplineSecStruct[i][j/m_numberOfSplineSeg+1] != protein::CallProteinData::SecStructure::TYPE_SHEET )
					{
						if(  j%m_numberOfSplineSeg == 0 )
							end = 1.0f;
						f1 = 1.0f - float(j%m_numberOfSplineSeg)/float(m_numberOfSplineSeg-1)+1.0f/float(m_numberOfSplineSeg-1)+0.2f;
						f2 = 1.0f - float(j%m_numberOfSplineSeg)/float(m_numberOfSplineSeg-1)+0.2f;
					}
				}
				else if( bSplineSecStruct[i][j/m_numberOfSplineSeg] == protein::CallProteinData::SecStructure::TYPE_HELIX )
					type = 2.0f;
				else
					type = 0.0f;
				// get the colors
				if( this->m_smoothCartoonColoringMode && j/m_numberOfSplineSeg > 0 )
				{
					col1 = cartoonColor[i][j/m_numberOfSplineSeg]*float(j%m_numberOfSplineSeg)/float(m_numberOfSplineSeg-1)
						+ cartoonColor[i][j/m_numberOfSplineSeg-1]*float((m_numberOfSplineSeg-1)-j%m_numberOfSplineSeg)/float(m_numberOfSplineSeg-1);
					int k = j+1;
					if( j%m_numberOfSplineSeg == m_numberOfSplineSeg-1 )
						k = m_numberOfSplineSeg-1;
					col2 = cartoonColor[i][j/m_numberOfSplineSeg]*float(k%m_numberOfSplineSeg)/float(m_numberOfSplineSeg-1)
						+ cartoonColor[i][j/m_numberOfSplineSeg-1]*float((m_numberOfSplineSeg-1)-k%m_numberOfSplineSeg)/float(m_numberOfSplineSeg-1);
				}
				else
				{
					col1 = cartoonColor[i][j/m_numberOfSplineSeg];
					col2 = cartoonColor[i][j/m_numberOfSplineSeg];
				}

				// -------------------------------------
				// --- START computation from shader ---
				// -------------------------------------

				// get all vertex positions
				v0 = bSplineCoords[i][j-2];
				v1 = bSplineCoordsDir[i][j-1];
				v2 = bSplineCoords[i][j-1];
				v3 = bSplineCoords[i][j];
				v4 = bSplineCoordsDir[i][j];
				v5 = bSplineCoords[i][j+1];
				// compute all needed directions
				dir20 = v2 - v0;
				dir12 = v1 - v2;
				dir32 = v3 - v2;
				dir43 = v4 - v3;
				dir53 = v5 - v3;
				// scale factor for the width of the tube
				
				//	this->m_colorsParamsTube[counterTube*6*3+7] = start;
				//	this->m_colorsParamsTube[counterTube*6*3+8] = end;
				scale = m_radiusCartoon;
				stretch1 = f1;
				stretch2 = f2;
				ortho1 = ( dir20 + dir32);
				ortho1.Normalise();
				ortho2 = ( dir32 + dir53);
				ortho2.Normalise();
				dir1 = ( dir12.Cross( ortho1));
				dir1.Normalise();
				dir2 = ( dir43.Cross( ortho2));
				dir2.Normalise();

				dir1 = ( dir1.Cross( ortho1));
				dir1.Normalise();
				dir1 = dir1*stretch1;
				dir2 = ( dir2.Cross( ortho2));
				dir2.Normalise();
				dir2 = dir2*stretch2;
				norm1 = ( dir1.Cross( ortho1));
				norm1.Normalise();
				norm2 = ( dir2.Cross( ortho2));
				norm2.Normalise();

				// -----------------------------------
				// --- END computation from shader ---
				// -----------------------------------

				// store information in the apropriate arrays
				if( bSplineSecStruct[i][j/m_numberOfSplineSeg] == protein::CallProteinData::SecStructure::TYPE_SHEET )
				{
					this->m_vertArrow[counterArrow*3*16+0] = (v2 - dir1 + norm1*scale).GetX();
					this->m_vertArrow[counterArrow*3*16+1] = (v2 - dir1 + norm1*scale).GetY();
					this->m_vertArrow[counterArrow*3*16+2] = (v2 - dir1 + norm1*scale).GetZ();
					this->m_vertArrow[counterArrow*3*16+3] = (v2 + dir1 + norm1*scale).GetX();
					this->m_vertArrow[counterArrow*3*16+4] = (v2 + dir1 + norm1*scale).GetY();
					this->m_vertArrow[counterArrow*3*16+5] = (v2 + dir1 + norm1*scale).GetZ();
					this->m_vertArrow[counterArrow*3*16+6] = (v3 + dir2 + norm2*scale).GetX();
					this->m_vertArrow[counterArrow*3*16+7] = (v3 + dir2 + norm2*scale).GetY();
					this->m_vertArrow[counterArrow*3*16+8] = (v3 + dir2 + norm2*scale).GetZ();
					this->m_vertArrow[counterArrow*3*16+9] = (v3 - dir2 + norm2*scale).GetX();
					this->m_vertArrow[counterArrow*3*16+10] = (v3 - dir2 + norm2*scale).GetY();
					this->m_vertArrow[counterArrow*3*16+11] = (v3 - dir2 + norm2*scale).GetZ();
					this->m_vertArrow[counterArrow*3*16+12] = (v2 - dir1 - norm1*scale).GetX();
					this->m_vertArrow[counterArrow*3*16+13] = (v2 - dir1 - norm1*scale).GetY();
					this->m_vertArrow[counterArrow*3*16+14] = (v2 - dir1 - norm1*scale).GetZ();
					this->m_vertArrow[counterArrow*3*16+15] = (v2 + dir1 - norm1*scale).GetX();
					this->m_vertArrow[counterArrow*3*16+16] = (v2 + dir1 - norm1*scale).GetY();
					this->m_vertArrow[counterArrow*3*16+17] = (v2 + dir1 - norm1*scale).GetZ();
					this->m_vertArrow[counterArrow*3*16+18] = (v3 + dir2 - norm2*scale).GetX();
					this->m_vertArrow[counterArrow*3*16+19] = (v3 + dir2 - norm2*scale).GetY();
					this->m_vertArrow[counterArrow*3*16+20] = (v3 + dir2 - norm2*scale).GetZ();
					this->m_vertArrow[counterArrow*3*16+21] = (v3 - dir2 - norm2*scale).GetX();
					this->m_vertArrow[counterArrow*3*16+22] = (v3 - dir2 - norm2*scale).GetY();
					this->m_vertArrow[counterArrow*3*16+23] = (v3 - dir2 - norm2*scale).GetZ();
					this->m_vertArrow[counterArrow*3*16+24] = (v2 + dir1 + norm1*scale).GetX();
					this->m_vertArrow[counterArrow*3*16+25] = (v2 + dir1 + norm1*scale).GetY();
					this->m_vertArrow[counterArrow*3*16+26] = (v2 + dir1 + norm1*scale).GetZ();
					this->m_vertArrow[counterArrow*3*16+27] = (v2 + dir1 - norm1*scale).GetX();
					this->m_vertArrow[counterArrow*3*16+28] = (v2 + dir1 - norm1*scale).GetY();
					this->m_vertArrow[counterArrow*3*16+29] = (v2 + dir1 - norm1*scale).GetZ();
					this->m_vertArrow[counterArrow*3*16+30] = (v3 + dir2 - norm2*scale).GetX();
					this->m_vertArrow[counterArrow*3*16+31] = (v3 + dir2 - norm2*scale).GetY();
					this->m_vertArrow[counterArrow*3*16+32] = (v3 + dir2 - norm2*scale).GetZ();
					this->m_vertArrow[counterArrow*3*16+33] = (v3 + dir2 + norm2*scale).GetX();
					this->m_vertArrow[counterArrow*3*16+34] = (v3 + dir2 + norm2*scale).GetY();
					this->m_vertArrow[counterArrow*3*16+35] = (v3 + dir2 + norm2*scale).GetZ();
					this->m_vertArrow[counterArrow*3*16+36] = (v2 - dir1 + norm1*scale).GetX();
					this->m_vertArrow[counterArrow*3*16+37] = (v2 - dir1 + norm1*scale).GetY();
					this->m_vertArrow[counterArrow*3*16+38] = (v2 - dir1 + norm1*scale).GetZ();
					this->m_vertArrow[counterArrow*3*16+39] = (v2 - dir1 - norm1*scale).GetX();
					this->m_vertArrow[counterArrow*3*16+40] = (v2 - dir1 - norm1*scale).GetY();
					this->m_vertArrow[counterArrow*3*16+41] = (v2 - dir1 - norm1*scale).GetZ();
					this->m_vertArrow[counterArrow*3*16+42] = (v3 - dir2 - norm2*scale).GetX();
					this->m_vertArrow[counterArrow*3*16+43] = (v3 - dir2 - norm2*scale).GetY();
					this->m_vertArrow[counterArrow*3*16+44] = (v3 - dir2 - norm2*scale).GetZ();
					this->m_vertArrow[counterArrow*3*16+45] = (v3 - dir2 + norm2*scale).GetX();
					this->m_vertArrow[counterArrow*3*16+46] = (v3 - dir2 + norm2*scale).GetY();
					this->m_vertArrow[counterArrow*3*16+47] = (v3 - dir2 + norm2*scale).GetZ();
					this->m_colorsParamsArrow[counterArrow*3*16+0] = col1.GetX();
					this->m_colorsParamsArrow[counterArrow*3*16+1] = col1.GetY();
					this->m_colorsParamsArrow[counterArrow*3*16+2] = col1.GetZ();
					this->m_colorsParamsArrow[counterArrow*3*16+3] = col1.GetX();
					this->m_colorsParamsArrow[counterArrow*3*16+4] = col1.GetY();
					this->m_colorsParamsArrow[counterArrow*3*16+5] = col1.GetZ();
					this->m_colorsParamsArrow[counterArrow*3*16+6] = col2.GetX();
					this->m_colorsParamsArrow[counterArrow*3*16+7] = col2.GetY();
					this->m_colorsParamsArrow[counterArrow*3*16+8] = col2.GetZ();
					this->m_colorsParamsArrow[counterArrow*3*16+9] = col2.GetX();
					this->m_colorsParamsArrow[counterArrow*3*16+10] = col2.GetY();
					this->m_colorsParamsArrow[counterArrow*3*16+11] = col2.GetZ();
					this->m_colorsParamsArrow[counterArrow*3*16+12] = col1.GetX();
					this->m_colorsParamsArrow[counterArrow*3*16+13] = col1.GetY();
					this->m_colorsParamsArrow[counterArrow*3*16+14] = col1.GetZ();
					this->m_colorsParamsArrow[counterArrow*3*16+15] = col1.GetX();
					this->m_colorsParamsArrow[counterArrow*3*16+16] = col1.GetY();
					this->m_colorsParamsArrow[counterArrow*3*16+17] = col1.GetZ();
					this->m_colorsParamsArrow[counterArrow*3*16+18] = col2.GetX();
					this->m_colorsParamsArrow[counterArrow*3*16+19] = col2.GetY();
					this->m_colorsParamsArrow[counterArrow*3*16+20] = col2.GetZ();
					this->m_colorsParamsArrow[counterArrow*3*16+21] = col2.GetX();
					this->m_colorsParamsArrow[counterArrow*3*16+22] = col2.GetY();
					this->m_colorsParamsArrow[counterArrow*3*16+23] = col2.GetZ();
					this->m_colorsParamsArrow[counterArrow*3*16+24] = col1.GetX();
					this->m_colorsParamsArrow[counterArrow*3*16+25] = col1.GetY();
					this->m_colorsParamsArrow[counterArrow*3*16+26] = col1.GetZ();
					this->m_colorsParamsArrow[counterArrow*3*16+27] = col1.GetX();
					this->m_colorsParamsArrow[counterArrow*3*16+28] = col1.GetY();
					this->m_colorsParamsArrow[counterArrow*3*16+29] = col1.GetZ();
					this->m_colorsParamsArrow[counterArrow*3*16+30] = col2.GetX();
					this->m_colorsParamsArrow[counterArrow*3*16+31] = col2.GetY();
					this->m_colorsParamsArrow[counterArrow*3*16+32] = col2.GetZ();
					this->m_colorsParamsArrow[counterArrow*3*16+33] = col2.GetX();
					this->m_colorsParamsArrow[counterArrow*3*16+34] = col2.GetY();
					this->m_colorsParamsArrow[counterArrow*3*16+35] = col2.GetZ();
					this->m_colorsParamsArrow[counterArrow*3*16+36] = col1.GetX();
					this->m_colorsParamsArrow[counterArrow*3*16+37] = col1.GetY();
					this->m_colorsParamsArrow[counterArrow*3*16+38] = col1.GetZ();
					this->m_colorsParamsArrow[counterArrow*3*16+39] = col1.GetX();
					this->m_colorsParamsArrow[counterArrow*3*16+40] = col1.GetY();
					this->m_colorsParamsArrow[counterArrow*3*16+41] = col1.GetZ();
					this->m_colorsParamsArrow[counterArrow*3*16+42] = col2.GetX();
					this->m_colorsParamsArrow[counterArrow*3*16+43] = col2.GetY();
					this->m_colorsParamsArrow[counterArrow*3*16+44] = col2.GetZ();
					this->m_colorsParamsArrow[counterArrow*3*16+45] = col2.GetX();
					this->m_colorsParamsArrow[counterArrow*3*16+46] = col2.GetY();
					this->m_colorsParamsArrow[counterArrow*3*16+47] = col2.GetZ();
					norm1.Normalise();
					norm2.Normalise();
					dir1.Normalise();
					dir2.Normalise();
					this->m_normalArrow[counterArrow*3*16+0] = (norm1).GetX();
					this->m_normalArrow[counterArrow*3*16+1] = (norm1).GetY();
					this->m_normalArrow[counterArrow*3*16+2] = (norm1).GetZ();
					this->m_normalArrow[counterArrow*3*16+3] = (norm1).GetX();
					this->m_normalArrow[counterArrow*3*16+4] = (norm1).GetY();
					this->m_normalArrow[counterArrow*3*16+5] = (norm1).GetZ();
					this->m_normalArrow[counterArrow*3*16+6] = (norm2).GetX();
					this->m_normalArrow[counterArrow*3*16+7] = (norm2).GetY();
					this->m_normalArrow[counterArrow*3*16+8] = (norm2).GetZ();
					this->m_normalArrow[counterArrow*3*16+9] = (norm2).GetX();
					this->m_normalArrow[counterArrow*3*16+10] = (norm2).GetY();
					this->m_normalArrow[counterArrow*3*16+11] = (norm2).GetZ();
					this->m_normalArrow[counterArrow*3*16+12] = (-norm1).GetX();
					this->m_normalArrow[counterArrow*3*16+13] = (-norm1).GetY();
					this->m_normalArrow[counterArrow*3*16+14] = (-norm1).GetZ();
					this->m_normalArrow[counterArrow*3*16+15] = (-norm1).GetX();
					this->m_normalArrow[counterArrow*3*16+16] = (-norm1).GetY();
					this->m_normalArrow[counterArrow*3*16+17] = (-norm1).GetZ();
					this->m_normalArrow[counterArrow*3*16+18] = (-norm2).GetX();
					this->m_normalArrow[counterArrow*3*16+19] = (-norm2).GetY();
					this->m_normalArrow[counterArrow*3*16+20] = (-norm2).GetZ();
					this->m_normalArrow[counterArrow*3*16+21] = (-norm2).GetX();
					this->m_normalArrow[counterArrow*3*16+22] = (-norm2).GetY();
					this->m_normalArrow[counterArrow*3*16+23] = (-norm2).GetZ();
					this->m_normalArrow[counterArrow*3*16+24] = (dir1).GetX();
					this->m_normalArrow[counterArrow*3*16+25] = (dir1).GetY();
					this->m_normalArrow[counterArrow*3*16+26] = (dir1).GetZ();
					this->m_normalArrow[counterArrow*3*16+27] = (dir1).GetX();
					this->m_normalArrow[counterArrow*3*16+28] = (dir1).GetY();
					this->m_normalArrow[counterArrow*3*16+29] = (dir1).GetZ();
					this->m_normalArrow[counterArrow*3*16+30] = (dir2).GetX();
					this->m_normalArrow[counterArrow*3*16+31] = (dir2).GetY();
					this->m_normalArrow[counterArrow*3*16+32] = (dir2).GetZ();
					this->m_normalArrow[counterArrow*3*16+33] = (dir2).GetX();
					this->m_normalArrow[counterArrow*3*16+34] = (dir2).GetY();
					this->m_normalArrow[counterArrow*3*16+35] = (dir2).GetZ();
					this->m_normalArrow[counterArrow*3*16+36] = (-dir1).GetX();
					this->m_normalArrow[counterArrow*3*16+37] = (-dir1).GetY();
					this->m_normalArrow[counterArrow*3*16+38] = (-dir1).GetZ();
					this->m_normalArrow[counterArrow*3*16+39] = (-dir1).GetX();
					this->m_normalArrow[counterArrow*3*16+40] = (-dir1).GetY();
					this->m_normalArrow[counterArrow*3*16+41] = (-dir1).GetZ();
					this->m_normalArrow[counterArrow*3*16+42] = (-dir2).GetX();
					this->m_normalArrow[counterArrow*3*16+43] = (-dir2).GetY();
					this->m_normalArrow[counterArrow*3*16+44] = (-dir2).GetZ();
					this->m_normalArrow[counterArrow*3*16+45] = (-dir2).GetX();
					this->m_normalArrow[counterArrow*3*16+46] = (-dir2).GetY();
					this->m_normalArrow[counterArrow*3*16+47] = (-dir2).GetZ();
					counterArrow++;
				}
				else if( bSplineSecStruct[i][j/m_numberOfSplineSeg] == protein::CallProteinData::SecStructure::TYPE_HELIX )
				{
					this->m_vertHelix[counterHelix*3*16+0] = (v2 - dir1 + norm1*scale).GetX();
					this->m_vertHelix[counterHelix*3*16+1] = (v2 - dir1 + norm1*scale).GetY();
					this->m_vertHelix[counterHelix*3*16+2] = (v2 - dir1 + norm1*scale).GetZ();
					this->m_vertHelix[counterHelix*3*16+3] = (v2 + dir1 + norm1*scale).GetX();
					this->m_vertHelix[counterHelix*3*16+4] = (v2 + dir1 + norm1*scale).GetY();
					this->m_vertHelix[counterHelix*3*16+5] = (v2 + dir1 + norm1*scale).GetZ();
					this->m_vertHelix[counterHelix*3*16+6] = (v3 + dir2 + norm2*scale).GetX();
					this->m_vertHelix[counterHelix*3*16+7] = (v3 + dir2 + norm2*scale).GetY();
					this->m_vertHelix[counterHelix*3*16+8] = (v3 + dir2 + norm2*scale).GetZ();
					this->m_vertHelix[counterHelix*3*16+9] = (v3 - dir2 + norm2*scale).GetX();
					this->m_vertHelix[counterHelix*3*16+10] = (v3 - dir2 + norm2*scale).GetY();
					this->m_vertHelix[counterHelix*3*16+11] = (v3 - dir2 + norm2*scale).GetZ();
					this->m_vertHelix[counterHelix*3*16+12] = (v2 - dir1 - norm1*scale).GetX();
					this->m_vertHelix[counterHelix*3*16+13] = (v2 - dir1 - norm1*scale).GetY();
					this->m_vertHelix[counterHelix*3*16+14] = (v2 - dir1 - norm1*scale).GetZ();
					this->m_vertHelix[counterHelix*3*16+15] = (v2 + dir1 - norm1*scale).GetX();
					this->m_vertHelix[counterHelix*3*16+16] = (v2 + dir1 - norm1*scale).GetY();
					this->m_vertHelix[counterHelix*3*16+17] = (v2 + dir1 - norm1*scale).GetZ();
					this->m_vertHelix[counterHelix*3*16+18] = (v3 + dir2 - norm2*scale).GetX();
					this->m_vertHelix[counterHelix*3*16+19] = (v3 + dir2 - norm2*scale).GetY();
					this->m_vertHelix[counterHelix*3*16+20] = (v3 + dir2 - norm2*scale).GetZ();
					this->m_vertHelix[counterHelix*3*16+21] = (v3 - dir2 - norm2*scale).GetX();
					this->m_vertHelix[counterHelix*3*16+22] = (v3 - dir2 - norm2*scale).GetY();
					this->m_vertHelix[counterHelix*3*16+23] = (v3 - dir2 - norm2*scale).GetZ();
					this->m_vertHelix[counterHelix*3*16+24] = (v2 + dir1 + norm1*scale).GetX();
					this->m_vertHelix[counterHelix*3*16+25] = (v2 + dir1 + norm1*scale).GetY();
					this->m_vertHelix[counterHelix*3*16+26] = (v2 + dir1 + norm1*scale).GetZ();
					this->m_vertHelix[counterHelix*3*16+27] = (v2 + dir1 - norm1*scale).GetX();
					this->m_vertHelix[counterHelix*3*16+28] = (v2 + dir1 - norm1*scale).GetY();
					this->m_vertHelix[counterHelix*3*16+29] = (v2 + dir1 - norm1*scale).GetZ();
					this->m_vertHelix[counterHelix*3*16+30] = (v3 + dir2 - norm2*scale).GetX();
					this->m_vertHelix[counterHelix*3*16+31] = (v3 + dir2 - norm2*scale).GetY();
					this->m_vertHelix[counterHelix*3*16+32] = (v3 + dir2 - norm2*scale).GetZ();
					this->m_vertHelix[counterHelix*3*16+33] = (v3 + dir2 + norm2*scale).GetX();
					this->m_vertHelix[counterHelix*3*16+34] = (v3 + dir2 + norm2*scale).GetY();
					this->m_vertHelix[counterHelix*3*16+35] = (v3 + dir2 + norm2*scale).GetZ();
					this->m_vertHelix[counterHelix*3*16+36] = (v2 - dir1 + norm1*scale).GetX();
					this->m_vertHelix[counterHelix*3*16+37] = (v2 - dir1 + norm1*scale).GetY();
					this->m_vertHelix[counterHelix*3*16+38] = (v2 - dir1 + norm1*scale).GetZ();
					this->m_vertHelix[counterHelix*3*16+39] = (v2 - dir1 - norm1*scale).GetX();
					this->m_vertHelix[counterHelix*3*16+40] = (v2 - dir1 - norm1*scale).GetY();
					this->m_vertHelix[counterHelix*3*16+41] = (v2 - dir1 - norm1*scale).GetZ();
					this->m_vertHelix[counterHelix*3*16+42] = (v3 - dir2 - norm2*scale).GetX();
					this->m_vertHelix[counterHelix*3*16+43] = (v3 - dir2 - norm2*scale).GetY();
					this->m_vertHelix[counterHelix*3*16+44] = (v3 - dir2 - norm2*scale).GetZ();
					this->m_vertHelix[counterHelix*3*16+45] = (v3 - dir2 + norm2*scale).GetX();
					this->m_vertHelix[counterHelix*3*16+46] = (v3 - dir2 + norm2*scale).GetY();
					this->m_vertHelix[counterHelix*3*16+47] = (v3 - dir2 + norm2*scale).GetZ();
					this->m_colorsParamsHelix[counterHelix*3*16+0] = col1.GetX();
					this->m_colorsParamsHelix[counterHelix*3*16+1] = col1.GetY();
					this->m_colorsParamsHelix[counterHelix*3*16+2] = col1.GetZ();
					this->m_colorsParamsHelix[counterHelix*3*16+3] = col1.GetX();
					this->m_colorsParamsHelix[counterHelix*3*16+4] = col1.GetY();
					this->m_colorsParamsHelix[counterHelix*3*16+5] = col1.GetZ();
					this->m_colorsParamsHelix[counterHelix*3*16+6] = col2.GetX();
					this->m_colorsParamsHelix[counterHelix*3*16+7] = col2.GetY();
					this->m_colorsParamsHelix[counterHelix*3*16+8] = col2.GetZ();
					this->m_colorsParamsHelix[counterHelix*3*16+9] = col2.GetX();
					this->m_colorsParamsHelix[counterHelix*3*16+10] = col2.GetY();
					this->m_colorsParamsHelix[counterHelix*3*16+11] = col2.GetZ();
					this->m_colorsParamsHelix[counterHelix*3*16+12] = col1.GetX();
					this->m_colorsParamsHelix[counterHelix*3*16+13] = col1.GetY();
					this->m_colorsParamsHelix[counterHelix*3*16+14] = col1.GetZ();
					this->m_colorsParamsHelix[counterHelix*3*16+15] = col1.GetX();
					this->m_colorsParamsHelix[counterHelix*3*16+16] = col1.GetY();
					this->m_colorsParamsHelix[counterHelix*3*16+17] = col1.GetZ();
					this->m_colorsParamsHelix[counterHelix*3*16+18] = col2.GetX();
					this->m_colorsParamsHelix[counterHelix*3*16+19] = col2.GetY();
					this->m_colorsParamsHelix[counterHelix*3*16+20] = col2.GetZ();
					this->m_colorsParamsHelix[counterHelix*3*16+21] = col2.GetX();
					this->m_colorsParamsHelix[counterHelix*3*16+22] = col2.GetY();
					this->m_colorsParamsHelix[counterHelix*3*16+23] = col2.GetZ();
					this->m_colorsParamsHelix[counterHelix*3*16+24] = col1.GetX();
					this->m_colorsParamsHelix[counterHelix*3*16+25] = col1.GetY();
					this->m_colorsParamsHelix[counterHelix*3*16+26] = col1.GetZ();
					this->m_colorsParamsHelix[counterHelix*3*16+27] = col1.GetX();
					this->m_colorsParamsHelix[counterHelix*3*16+28] = col1.GetY();
					this->m_colorsParamsHelix[counterHelix*3*16+29] = col1.GetZ();
					this->m_colorsParamsHelix[counterHelix*3*16+30] = col2.GetX();
					this->m_colorsParamsHelix[counterHelix*3*16+31] = col2.GetY();
					this->m_colorsParamsHelix[counterHelix*3*16+32] = col2.GetZ();
					this->m_colorsParamsHelix[counterHelix*3*16+33] = col2.GetX();
					this->m_colorsParamsHelix[counterHelix*3*16+34] = col2.GetY();
					this->m_colorsParamsHelix[counterHelix*3*16+35] = col2.GetZ();
					this->m_colorsParamsHelix[counterHelix*3*16+36] = col1.GetX();
					this->m_colorsParamsHelix[counterHelix*3*16+37] = col1.GetY();
					this->m_colorsParamsHelix[counterHelix*3*16+38] = col1.GetZ();
					this->m_colorsParamsHelix[counterHelix*3*16+39] = col1.GetX();
					this->m_colorsParamsHelix[counterHelix*3*16+40] = col1.GetY();
					this->m_colorsParamsHelix[counterHelix*3*16+41] = col1.GetZ();
					this->m_colorsParamsHelix[counterHelix*3*16+42] = col2.GetX();
					this->m_colorsParamsHelix[counterHelix*3*16+43] = col2.GetY();
					this->m_colorsParamsHelix[counterHelix*3*16+44] = col2.GetZ();
					this->m_colorsParamsHelix[counterHelix*3*16+45] = col2.GetX();
					this->m_colorsParamsHelix[counterHelix*3*16+46] = col2.GetY();
					this->m_colorsParamsHelix[counterHelix*3*16+47] = col2.GetZ();
					norm1.Normalise();
					norm2.Normalise();
					dir1.Normalise();
					dir2.Normalise();
					this->m_normalHelix[counterHelix*3*16+0] = (norm1).GetX();
					this->m_normalHelix[counterHelix*3*16+1] = (norm1).GetY();
					this->m_normalHelix[counterHelix*3*16+2] = (norm1).GetZ();
					this->m_normalHelix[counterHelix*3*16+3] = (norm1).GetX();
					this->m_normalHelix[counterHelix*3*16+4] = (norm1).GetY();
					this->m_normalHelix[counterHelix*3*16+5] = (norm1).GetZ();
					this->m_normalHelix[counterHelix*3*16+6] = (norm2).GetX();
					this->m_normalHelix[counterHelix*3*16+7] = (norm2).GetY();
					this->m_normalHelix[counterHelix*3*16+8] = (norm2).GetZ();
					this->m_normalHelix[counterHelix*3*16+9] = (norm2).GetX();
					this->m_normalHelix[counterHelix*3*16+10] = (norm2).GetY();
					this->m_normalHelix[counterHelix*3*16+11] = (norm2).GetZ();
					this->m_normalHelix[counterHelix*3*16+12] = (-norm1).GetX();
					this->m_normalHelix[counterHelix*3*16+13] = (-norm1).GetY();
					this->m_normalHelix[counterHelix*3*16+14] = (-norm1).GetZ();
					this->m_normalHelix[counterHelix*3*16+15] = (-norm1).GetX();
					this->m_normalHelix[counterHelix*3*16+16] = (-norm1).GetY();
					this->m_normalHelix[counterHelix*3*16+17] = (-norm1).GetZ();
					this->m_normalHelix[counterHelix*3*16+18] = (-norm2).GetX();
					this->m_normalHelix[counterHelix*3*16+19] = (-norm2).GetY();
					this->m_normalHelix[counterHelix*3*16+20] = (-norm2).GetZ();
					this->m_normalHelix[counterHelix*3*16+21] = (-norm2).GetX();
					this->m_normalHelix[counterHelix*3*16+22] = (-norm2).GetY();
					this->m_normalHelix[counterHelix*3*16+23] = (-norm2).GetZ();
					this->m_normalHelix[counterHelix*3*16+24] = (dir1).GetX();
					this->m_normalHelix[counterHelix*3*16+25] = (dir1).GetY();
					this->m_normalHelix[counterHelix*3*16+26] = (dir1).GetZ();
					this->m_normalHelix[counterHelix*3*16+27] = (dir1).GetX();
					this->m_normalHelix[counterHelix*3*16+28] = (dir1).GetY();
					this->m_normalHelix[counterHelix*3*16+29] = (dir1).GetZ();
					this->m_normalHelix[counterHelix*3*16+30] = (dir2).GetX();
					this->m_normalHelix[counterHelix*3*16+31] = (dir2).GetY();
					this->m_normalHelix[counterHelix*3*16+32] = (dir2).GetZ();
					this->m_normalHelix[counterHelix*3*16+33] = (dir2).GetX();
					this->m_normalHelix[counterHelix*3*16+34] = (dir2).GetY();
					this->m_normalHelix[counterHelix*3*16+35] = (dir2).GetZ();
					this->m_normalHelix[counterHelix*3*16+36] = (-dir1).GetX();
					this->m_normalHelix[counterHelix*3*16+37] = (-dir1).GetY();
					this->m_normalHelix[counterHelix*3*16+38] = (-dir1).GetZ();
					this->m_normalHelix[counterHelix*3*16+39] = (-dir1).GetX();
					this->m_normalHelix[counterHelix*3*16+40] = (-dir1).GetY();
					this->m_normalHelix[counterHelix*3*16+41] = (-dir1).GetZ();
					this->m_normalHelix[counterHelix*3*16+42] = (-dir2).GetX();
					this->m_normalHelix[counterHelix*3*16+43] = (-dir2).GetY();
					this->m_normalHelix[counterHelix*3*16+44] = (-dir2).GetZ();
					this->m_normalHelix[counterHelix*3*16+45] = (-dir2).GetX();
					this->m_normalHelix[counterHelix*3*16+46] = (-dir2).GetY();
					this->m_normalHelix[counterHelix*3*16+47] = (-dir2).GetZ();
					counterHelix++;
				}
				else
				{
					dir1 = dir1 * scale;
					dir2 = dir2 * scale;

					for( unsigned int k = 0; k < m_numberOfTubeSeg; k++ )
					{
						alpha = (float(2.0*M_PI)/float(m_numberOfTubeSeg))*float(k);
						q1.Set( alpha, ortho1);
						q2.Set( alpha, ortho2);
						res1 = q1 * dir1;
						res2 = q2 * dir2;

						// v1
						this->m_vertTube[counterTube] = (v2 + res1).GetX();
						this->m_colorsParamsTube[counterTube] = col1.GetX();
						counterTube++;
						this->m_vertTube[counterTube] = (v2 + res1).GetY();
						this->m_colorsParamsTube[counterTube] = col1.GetY();
						counterTube++;
						this->m_vertTube[counterTube] = (v2 + res1).GetZ();
						this->m_colorsParamsTube[counterTube] = col1.GetZ();
						counterTube++;
						res1.Normalise();
						this->m_normalTube[counterTube-3] = res1.GetX();
						this->m_normalTube[counterTube-2] = res1.GetY();
						this->m_normalTube[counterTube-1] = res1.GetZ();
						// v2
						this->m_vertTube[counterTube] = (v3 + res2).GetX();
						this->m_colorsParamsTube[counterTube] = col2.GetX();
						counterTube++;
						this->m_vertTube[counterTube] = (v3 + res2).GetY();
						this->m_colorsParamsTube[counterTube] = col2.GetY();
						counterTube++;
						this->m_vertTube[counterTube] = (v3 + res2).GetZ();
						this->m_colorsParamsTube[counterTube] = col2.GetZ();
						counterTube++;
						res2.Normalise();
						this->m_normalTube[counterTube-3] = res2.GetX();
						this->m_normalTube[counterTube-2] = res2.GetY();
						this->m_normalTube[counterTube-1] = res2.GetZ();
						
						alpha = (float(2.0f*M_PI)/float(m_numberOfTubeSeg))*float(k+1);
						q1.Set( alpha, ortho1);
						q2.Set( alpha, ortho2);
						res1 = q1 * dir1;
						res2 = q2 * dir2;

						// v3
						this->m_vertTube[counterTube] = (v3 + res2).GetX();
						this->m_colorsParamsTube[counterTube] = col2.GetX();
						counterTube++;
						this->m_vertTube[counterTube] = (v3 + res2).GetY();
						this->m_colorsParamsTube[counterTube] = col2.GetY();
						counterTube++;
						this->m_vertTube[counterTube] = (v3 + res2).GetZ();
						this->m_colorsParamsTube[counterTube] = col2.GetZ();
						counterTube++;
						res2.Normalise();
						this->m_normalTube[counterTube-3] = res2.GetX();
						this->m_normalTube[counterTube-2] = res2.GetY();
						this->m_normalTube[counterTube-1] = res2.GetZ();
						// v4
						this->m_vertTube[counterTube] = (v2 + res1).GetX();
						this->m_colorsParamsTube[counterTube] = col1.GetX();
						counterTube++;
						this->m_vertTube[counterTube] = (v2 + res1).GetY();
						this->m_colorsParamsTube[counterTube] = col1.GetY();
						counterTube++;
						this->m_vertTube[counterTube] = (v2 + res1).GetZ();
						this->m_colorsParamsTube[counterTube] = col1.GetZ();
						counterTube++;
						res1.Normalise();
						this->m_normalTube[counterTube-3] = res1.GetX();
						this->m_normalTube[counterTube-2] = res1.GetY();
						this->m_normalTube[counterTube-1] = res1.GetZ();
					}
				}
			}
		}
		// --- END store vertex/color/inparams ---

		// set cartoon CPU as created
		this->m_prepareCartoonCPU = false;
	}

	float spec[4] = { 1.0f, 1.0f, 1.0f, 1.0f};
	glMaterialfv( GL_FRONT_AND_BACK, GL_SPECULAR, spec);
	glMaterialf( GL_FRONT_AND_BACK, GL_SHININESS, 50.0f);
	glEnable( GL_COLOR_MATERIAL);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

	this->m_lightShader.Enable();
    //glDisable(GL_LIGHTING);

	// tube
	glVertexPointer( 3, GL_FLOAT, 0, this->m_vertTube);
	glNormalPointer( GL_FLOAT, 0, this->m_normalTube);
	glColorPointer( 3, GL_FLOAT, 0, this->m_colorsParamsTube);
	glDrawArrays( GL_QUADS, 0, m_totalCountTube*4*m_numberOfTubeSeg);

	// arrow
	glVertexPointer( 3, GL_FLOAT, 0, this->m_vertArrow);
	glNormalPointer( GL_FLOAT, 0, this->m_normalArrow);
	glColorPointer( 3, GL_FLOAT, 0, this->m_colorsParamsArrow);
	glDrawArrays( GL_QUADS, 0, m_totalCountArrow*16);

	// helix
	glVertexPointer( 3, GL_FLOAT, 0, this->m_vertHelix);
	glColorPointer( 3, GL_FLOAT, 0, this->m_colorsParamsHelix);
	glNormalPointer( GL_FLOAT, 0, this->m_normalHelix);
	glDrawArrays( GL_QUADS, 0, m_totalCountHelix*16);

	this->m_lightShader.Disable();

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
	glDisable( GL_COLOR_MATERIAL);
}


/*
 * protein::ProteinRendererCartoon::MakeColorTable
 */
void protein::ProteinRendererCartoon::MakeColorTable( const CallProteinData *prot, bool forceRecompute)
{
	unsigned int i;
	unsigned int currentChain, currentAminoAcid, currentAtom, currentSecStruct;
	protein::CallProteinData::Chain chain;
	// if recomputation is forced: clear current color table
	if( forceRecompute )
		this->m_protAtomColorTable.Clear();
	// only compute color table if necessary
	if( this->m_protAtomColorTable.IsEmpty() )
	{
		if( this->m_currentColoringMode == ELEMENT )
		{
			for( i = 0; i < prot->ProteinAtomCount(); i++ )
			{
				this->m_protAtomColorTable.Add( prot->AtomTypes()[prot->ProteinAtomData()[i].TypeIndex()].Colour()[0]);
				this->m_protAtomColorTable.Add( prot->AtomTypes()[prot->ProteinAtomData()[i].TypeIndex()].Colour()[1]);
				this->m_protAtomColorTable.Add( prot->AtomTypes()[prot->ProteinAtomData()[i].TypeIndex()].Colour()[2]);
			}
		}
		else if( this->m_currentColoringMode == AMINOACID )
		{
			// loop over all chains
			for( currentChain = 0; currentChain < prot->ProteinChainCount(); currentChain++ )
			{
				chain = prot->ProteinChain( currentChain);
				// loop over all amino acids in the current chain
				for( currentAminoAcid = 0; currentAminoAcid < chain.AminoAcidCount(); currentAminoAcid++ )
				{
					// loop over all connections of the current amino acid
					for( currentAtom = 0; 
						 currentAtom < chain.AminoAcid()[currentAminoAcid].AtomCount();
						 currentAtom++ )
					{
						i = chain.AminoAcid()[currentAminoAcid].NameIndex()+1;
						i = i % (unsigned int)(this->m_aminoAcidColorTable.Count());
						this->m_protAtomColorTable.Add( 
							this->m_aminoAcidColorTable[i].GetX() );
						this->m_protAtomColorTable.Add( 
							this->m_aminoAcidColorTable[i].GetY() );
						this->m_protAtomColorTable.Add( 
							this->m_aminoAcidColorTable[i].GetZ() );
					}
				}
			}
		}
		else if( this->m_currentColoringMode == STRUCTURE )
		{
			// loop over all chains
			for( currentChain = 0; currentChain < prot->ProteinChainCount(); currentChain++ )
			{
				chain = prot->ProteinChain( currentChain);
				// loop over all secondary structure elements in this chain
				for( currentSecStruct = 0; 
					 currentSecStruct < chain.SecondaryStructureCount();
					 currentSecStruct++ )
				{
					i = chain.SecondaryStructure()[currentSecStruct].AtomCount();
					// loop over all atoms in this secondary structure element
					for( currentAtom = 0; currentAtom < i; currentAtom++ )
					{
						if( chain.SecondaryStructure()[currentSecStruct].Type() ==
							protein::CallProteinData::SecStructure::TYPE_HELIX )
						{
							this->m_protAtomColorTable.Add( 255);
							this->m_protAtomColorTable.Add( 0);
							this->m_protAtomColorTable.Add( 0);
						}
						else if( chain.SecondaryStructure()[currentSecStruct].Type() ==
							protein::CallProteinData::SecStructure::TYPE_SHEET )
						{
							this->m_protAtomColorTable.Add( 0);
							this->m_protAtomColorTable.Add( 0);
							this->m_protAtomColorTable.Add( 255);
						}
						else if( chain.SecondaryStructure()[currentSecStruct].Type() ==
							protein::CallProteinData::SecStructure::TYPE_TURN )
						{
							this->m_protAtomColorTable.Add( 255);
							this->m_protAtomColorTable.Add( 255);
							this->m_protAtomColorTable.Add( 0);
						}
						else
						{
							this->m_protAtomColorTable.Add( 230);
							this->m_protAtomColorTable.Add( 230);
							this->m_protAtomColorTable.Add( 230);
						}
					}
				}
			}
		}
		else if( this->m_currentColoringMode == VALUE )
		{
		}
		else if( this->m_currentColoringMode == CHAIN_ID )
		{
			// loop over all chains
			for( currentChain = 0; currentChain < prot->ProteinChainCount(); currentChain++ )
			{
				chain = prot->ProteinChain( currentChain);
				// loop over all amino acids in the current chain
				for( currentAminoAcid = 0; currentAminoAcid < chain.AminoAcidCount(); currentAminoAcid++ )
				{
					// loop over all connections of the current amino acid
					for( currentAtom = 0; 
						 currentAtom < chain.AminoAcid()[currentAminoAcid].AtomCount();
						 currentAtom++ )
					{
						i = (currentChain + 1) % (unsigned int)(this->m_aminoAcidColorTable.Count());
						this->m_protAtomColorTable.Add( 
							this->m_aminoAcidColorTable[i].GetX() );
						this->m_protAtomColorTable.Add( 
							this->m_aminoAcidColorTable[i].GetY() );
						this->m_protAtomColorTable.Add( 
							this->m_aminoAcidColorTable[i].GetZ() );
					}
				}
			}
		}
		else if( this->m_currentColoringMode == RAINBOW )
		{
		}
	}
}


/*
 * protein::ProteinRendererCartoon::RecomputeAll
 */
void protein::ProteinRendererCartoon::RecomputeAll()
{
	this->m_prepareCartoonHybrid = true;
	this->m_prepareCartoonCPU = true;

	this->m_protAtomColorTable.Clear();
}


/*
 * protein::ProteinRendererCartoon::Fillm_aminoAcidColorTable
 */
void protein::ProteinRendererCartoon::FillAminoAcidColorTable()
{
	this->m_aminoAcidColorTable.Clear();
	this->m_aminoAcidColorTable.SetCount( 25);
	this->m_aminoAcidColorTable[0].Set( 128, 128, 128);
	this->m_aminoAcidColorTable[1].Set( 255, 0, 0);
	this->m_aminoAcidColorTable[2].Set( 255, 255, 0);
	this->m_aminoAcidColorTable[3].Set( 0, 255, 0);
	this->m_aminoAcidColorTable[4].Set( 0, 255, 255);
	this->m_aminoAcidColorTable[5].Set( 0, 0, 255);
	this->m_aminoAcidColorTable[6].Set( 255, 0, 255);
	this->m_aminoAcidColorTable[7].Set( 128, 0, 0);
	this->m_aminoAcidColorTable[8].Set( 128, 128, 0);
	this->m_aminoAcidColorTable[9].Set( 0, 128, 0);
	this->m_aminoAcidColorTable[10].Set( 0, 128, 128);
	this->m_aminoAcidColorTable[11].Set( 0, 0, 128);
	this->m_aminoAcidColorTable[12].Set( 128, 0, 128);
	this->m_aminoAcidColorTable[13].Set( 255, 128, 0);
	this->m_aminoAcidColorTable[14].Set( 0, 128, 255);
	this->m_aminoAcidColorTable[15].Set( 255, 128, 255);
	this->m_aminoAcidColorTable[16].Set( 128, 64, 0);
	this->m_aminoAcidColorTable[17].Set( 255, 255, 128);
	this->m_aminoAcidColorTable[18].Set( 128, 255, 128);
	this->m_aminoAcidColorTable[19].Set( 192, 255, 0);
	this->m_aminoAcidColorTable[20].Set( 128, 0, 192);
	this->m_aminoAcidColorTable[21].Set( 255, 128, 128);
	this->m_aminoAcidColorTable[22].Set( 192, 255, 192);
	this->m_aminoAcidColorTable[23].Set( 192, 192, 128);
	this->m_aminoAcidColorTable[24].Set( 255, 192, 128);
}

