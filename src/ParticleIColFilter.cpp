/*
 * ParticleIColFilter.h
 *
 * Copyright (C) 2015 by MegaMol Team
 * Alle Rechte vorbehalten.
 */
#include "stdafx.h"
#include "ParticleIColFilter.h"
#include "mmcore/param/FloatParam.h"
#include <algorithm>

using namespace megamol;
using namespace megamol::stdplugin;

datatools::ParticleIColFilter::ParticleIColFilter() : AbstractParticleManipulator("outData", "inData"),
        minValSlot("minVal", "The minimal color value of particles to be passed on"),
        maxValSlot("maxVal", "The maximal color value of particles to be passed on"),
        dataHash(0), frameId(0), parts(), data() {
    minValSlot.SetParameter(new core::param::FloatParam(0.0f));
    minValSlot.SetUpdateCallback(&ParticleIColFilter::reset);
    MakeSlotAvailable(&minValSlot);
    maxValSlot.SetParameter(new core::param::FloatParam(1.0f));
    maxValSlot.SetUpdateCallback(&ParticleIColFilter::reset);
    MakeSlotAvailable(&maxValSlot);
}

datatools::ParticleIColFilter::~ParticleIColFilter() {
    this->Release();
}

bool datatools::ParticleIColFilter::manipulateData(
        megamol::core::moldyn::MultiParticleDataCall& outData,
        megamol::core::moldyn::MultiParticleDataCall& inData) {

    if ((frameId != inData.FrameID()) || (dataHash != inData.DataHash()) || (inData.DataHash() == 0)) {
        frameId = inData.FrameID();
        dataHash = inData.DataHash();
        setData(inData);
    }
    inData.Unlock();

    outData.SetDataHash(dataHash);
    outData.SetFrameID(frameId);
    outData.SetParticleListCount(static_cast<unsigned int>(parts.size()));
    for (size_t i = 0; i < parts.size(); ++i) outData.AccessParticles(static_cast<unsigned int>(i)) = parts[i];
    outData.SetUnlocker(nullptr); // HAZARD: we could have one ...

    return true;
}

bool datatools::ParticleIColFilter::reset(core::param::ParamSlot&) {
    dataHash = 0;
    return true;
}

void datatools::ParticleIColFilter::setData(core::moldyn::MultiParticleDataCall& inDat) {
    unsigned int cnt = inDat.GetParticleListCount();
    parts.resize(cnt);
    data.resize(cnt);

    for (unsigned int i = 0; i < cnt; ++i) {
        setData(parts[i], data[i], inDat.AccessParticles(i));
    }

}

void datatools::ParticleIColFilter::setData(core::moldyn::MultiParticleDataCall::Particles& p, vislib::RawStorage& d, const core::moldyn::SimpleSphericalParticles& s) {
    using core::moldyn::MultiParticleDataCall;
    using core::moldyn::SimpleSphericalParticles;
    using vislib::RawStorage;

    p.SetCount(0);
    p.SetVertexData(SimpleSphericalParticles::VERTDATA_NONE, nullptr);
    p.SetColourData(SimpleSphericalParticles::COLDATA_NONE, nullptr);

    if (s.GetVertexDataType() == SimpleSphericalParticles::VERTDATA_NONE) return; // No data is no data
    if (s.GetColourDataType() != SimpleSphericalParticles::COLDATA_FLOAT_I) return; // for now, wrongly formated data is simply removed

    const size_t cnt = s.GetCount();

    const uint8_t* vp = reinterpret_cast<const uint8_t*>(s.GetVertexData());
    int v_size = 0;
    int v_step = s.GetVertexDataStride();
    switch (s.GetVertexDataType()) {
    case SimpleSphericalParticles::VERTDATA_FLOAT_XYZ: v_size = 12; break;
    case SimpleSphericalParticles::VERTDATA_FLOAT_XYZR: v_size = 16; break;
    case SimpleSphericalParticles::VERTDATA_SHORT_XYZ: v_size = 6; break;
    default: assert(false);
    }
    if (v_step < v_size) v_step = v_size;

    const uint8_t* cp = reinterpret_cast<const uint8_t*>(s.GetColourData());
    int c_size = 4;
    int c_step = s.GetColourDataStride();
    if (c_step < c_size) c_step = c_size;

    float minVal = minValSlot.Param<core::param::FloatParam>()->Value();
    float maxVal = maxValSlot.Param<core::param::FloatParam>()->Value();
    if (maxVal < minVal) std::swap(minVal, maxVal);

    // now count particles surviving
    size_t r_cnt = 0;
    for (size_t i = 0; i < cnt; ++i) {
        const float &c = *reinterpret_cast<const float*>(cp + c_step * i);
        if ((minVal <= c) && (c <= maxVal)) r_cnt++;
    }

    // now copying particles
    d.AssertSize(r_cnt * (v_size + c_size));
    const size_t c_off = r_cnt * v_size;
    p.SetCount(r_cnt);
    p.SetVertexData(s.GetVertexDataType(), d);
    p.SetColourData(SimpleSphericalParticles::COLDATA_FLOAT_I, d.At(c_off));
    p.SetColourMapIndexValues(s.GetMinColourIndexValue(), s.GetMaxColourIndexValue());

    r_cnt = 0;
    for (size_t i = 0; i < cnt; ++i) {
        const float *ci = reinterpret_cast<const float*>(cp + c_step * i);
        const void *vi = static_cast<const void*>(vp + v_step * i);
        if ((minVal <= *ci) && (*ci <= maxVal)) {
            ::memcpy(d.At(r_cnt * v_size), vi, v_size);
            ::memcpy(d.At(c_off + r_cnt * c_size), ci, c_size);
            r_cnt++;
        }
    }

}
