/*
 * LinearTransferFunctionParam.cpp
 *
 * Copyright (C) 2019 by Universitaet Stuttgart (VIS).
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "mmcore/param/LinearTransferFunctionParam.h"


using namespace megamol::core::param;


/**
 * LinearTransferFunctionParam::LinearTransferFunctionParam
 */
LinearTransferFunctionParam::LinearTransferFunctionParam(const std::string& initVal) : AbstractParam()
{
    if (this->CheckTransferFunctionString(initVal)) {
        this->val = initVal;
    }
    else {
        vislib::sys::Log::DefaultLog.WriteError("[LinearTransferFunctionParam] No valid parameter value for constructor given.");
    }
}


/**
 * LinearTransferFunctionParam::LinearTransferFunctionParam
 */
LinearTransferFunctionParam::LinearTransferFunctionParam(const char *initVal) : AbstractParam()
{
    if (this->CheckTransferFunctionString(std::string(initVal))) {
        this->val = std::string(initVal);
    }
    else {
        vislib::sys::Log::DefaultLog.WriteError("[LinearTransferFunctionParam] No valid parameter value for constructor given.");
    }
}


/**
 * LinearTransferFunctionParam::LinearTransferFunctionParam
 */
LinearTransferFunctionParam::LinearTransferFunctionParam(const vislib::StringA& initVal) : AbstractParam()
{
    if (this->CheckTransferFunctionString(std::string(initVal.PeekBuffer()))) {
        this->val = std::string(initVal.PeekBuffer());
    }
    else {
        vislib::sys::Log::DefaultLog.WriteError("[LinearTransferFunctionParam] No valid parameter value for constructor given.");
    }
}


/**
 * LinearTransferFunctionParam::~LinearTransferFunctionParam
 */
LinearTransferFunctionParam::~LinearTransferFunctionParam(void) {}


/**
 * LinearTransferFunctionParam::Definition
 */
void LinearTransferFunctionParam::Definition(vislib::RawStorage& outDef) const {
    outDef.AssertSize(6);
    memcpy(outDef.AsAt<char>(0), "MMTFFNC", 6);
}


/**
 * LinearTransferFunctionParam::ParseValue
 */
bool LinearTransferFunctionParam::ParseValue(vislib::TString const& v) {

    try {
        if (this->CheckTransferFunctionString(std::string(v.PeekBuffer()))) {
            this->val = std::string(v.PeekBuffer());
            return true;
        }
    } catch (...) {
    }

    return false;
}


/**
 * LinearTransferFunctionParam::SetValue
 */
void LinearTransferFunctionParam::SetValue(const std::string& v, bool setDirty) {

    if (v != this->val) {
        if (this->CheckTransferFunctionString(v)) {
            this->val = v;
            if (setDirty) this->setDirty();
        }
    }
}


/**
 * LinearTransferFunctionParam::ValueString
 */
vislib::TString LinearTransferFunctionParam::ValueString(void) const {
    return vislib::TString(this->val.c_str());
}


/**
 * LinearTransferFunctionParam::ParseTransferFunction
 */
bool LinearTransferFunctionParam::ParseTransferFunction(const std::string &in_tfs, TFType &out_data, InterpolationMode &out_interpolmode, UINT &out_texsize) {

    TFType tmp_data;
    std::string tmp_interpolmode_str;
    InterpolationMode tmp_interpolmode;
    UINT tmp_texsize;

    if (!in_tfs.empty()) {

        // Check for valid JSON string
        if (!LinearTransferFunctionParam::CheckTransferFunctionString(in_tfs)) {
            return false;
        }

        nlohmann::json json = nlohmann::json::parse(in_tfs);

        // Get texture size
        json.at("TextureSize").get_to(tmp_texsize);

        // Get interpolation method
        json.at("Interpolation").get_to(tmp_interpolmode_str);
        if (tmp_interpolmode_str == "LINEAR") {
            tmp_interpolmode = InterpolationMode::LINEAR;
        }
        else if (tmp_interpolmode_str == "GAUSS") {
            tmp_interpolmode = InterpolationMode::GAUSS;
        }

        // Get nodes data
        UINT tf_size = (UINT)json.at("Nodes").size();
        tmp_data.resize(tf_size);
        for (UINT i = 0; i < tf_size; ++i) {
            json.at("Nodes")[i].get_to(tmp_data[i]);
        }
    }
    else { // Loading default values for empty transfer function
        tmp_data.clear();
        std::array<float, 5> zero = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        std::array<float, 5> one = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
        tmp_data.emplace_back(zero);
        tmp_data.emplace_back(one);
        tmp_interpolmode = InterpolationMode::LINEAR;
        tmp_texsize = 128;
    }

    if (!LinearTransferFunctionParam::CheckTransferFunctionData(tmp_data, tmp_interpolmode, tmp_texsize)) {
        return false;
    }
    out_data = tmp_data;
    out_interpolmode = tmp_interpolmode;
    out_texsize = tmp_texsize;

    return true;
}


/**
 * LinearTransferFunctionParam::DumpTransferFunction
 */
bool LinearTransferFunctionParam::DumpTransferFunction(std::string &out_tfs, const TFType &in_data, const InterpolationMode in_interpolmode, const UINT in_texsize) {

    nlohmann::json json;

    if (!LinearTransferFunctionParam::CheckTransferFunctionData(in_data, in_interpolmode, in_texsize)) {
        return false;
    }

    std::string interpolation_str;
    switch (in_interpolmode) {
    case (InterpolationMode::LINEAR):
        interpolation_str = "LINEAR";
        break;
    case (InterpolationMode::GAUSS):
        interpolation_str = "GAUSS";
        break;
    }

    json["Interpolation"] = interpolation_str;
    json["TextureSize"] = in_texsize;
    json["Nodes"] = in_data;

    out_tfs = json.dump(); // pass 'true' for pretty printing with newlines

    return true;
}


/**
 * LinearTransferFunctionParam::CheckTransferFunctionData
 */
bool LinearTransferFunctionParam::CheckTransferFunctionData(const TFType &data, const InterpolationMode interpolmode, const UINT texsize) {

    bool check = true;
    if (texsize < 1) {
        vislib::sys::Log::DefaultLog.WriteError(
            "[CheckTransferFunctionData] Texture size should be greater than 0.");
        check = false;
    }
    if (data.size() < 2) {
        vislib::sys::Log::DefaultLog.WriteError(
            "[CheckTransferFunctionData] There should be at least two nodes.");
        check = false;
    }
    float last_value = -1.0f;
    for (auto& a : data) {
        for (int i = 0; i < 5; ++i) {
            if (a[i] < 0.0f) {
                vislib::sys::Log::DefaultLog.WriteError(
                    "[CheckTransferFunctionData] Color values must be greater than or equal to 0.");
                check = false;
            }
            else if (a[i] > 1.0f) {
                vislib::sys::Log::DefaultLog.WriteError(
                    "[CheckTransferFunctionData] Color values must be less than or equal to 1.");
                check = false;
            }
        }
        if (last_value >= a[4]) {
            vislib::sys::Log::DefaultLog.WriteError(
                "[LinearTransferFunction] 'Values' should be sorted from 0 to 1 and all 'Values' must be distinct.");
            return false;
        }
        else {
            last_value = a[4];
        }
    }
    if (data.front()[4] != 0.0f) {
        vislib::sys::Log::DefaultLog.WriteError(
            "[CheckTransferFunctionData] First node should have 'Value' = 0.");
        check = false;
    }
    if (data.back()[4] != 1.0f) {
        vislib::sys::Log::DefaultLog.WriteError(
            "[CheckTransferFunctionData] Last node should have 'Value' = 1.");
        check = false;
    }

    return check;
}


/**
 * LinearTransferFunctionParam::CheckTransferFunctionData
 */
bool LinearTransferFunctionParam::CheckTransferFunctionString(const std::string &tfs) {

    bool check = true;
    if (!tfs.empty()) {

        nlohmann::json json = nlohmann::json::parse(tfs);

        // Check for valid JSON object
        if (!json.is_object()) {
            vislib::sys::Log::DefaultLog.WriteError(
                "[CheckTransferFunctionString] Given string is no valid JSON object.");
            return false;
        }

        // Check texture size
        if (!json.at("TextureSize").is_number_integer()) {
            vislib::sys::Log::DefaultLog.WriteError(
                "[CheckTransferFunctionString] Couldn't read 'TextureSize' as integer value.");
            check = false;
        }

        // Check interpolation mode
        if (json.at("Interpolation").is_string()) {
            std::string tmp_str;
            json.at("Interpolation").get_to(tmp_str);
            if ((tmp_str != "LINEAR") && (tmp_str != "GAUSS")) {
                vislib::sys::Log::DefaultLog.WriteError( 
                    "[CheckTransferFunctionString] Couldn't find 'Interpolation' mode.");
                check = false;
            }
        }
        else {
            vislib::sys::Log::DefaultLog.WriteError(
                "[CheckTransferFunctionString] Couldn't read 'Interpolation' as string value.");
            check = false;
        }

        // Check transfer function node data
        if (json.at("Nodes").is_array()) {
            UINT tmp_size = (UINT)json.at("Nodes").size();
            if (tmp_size < 2) {
                vislib::sys::Log::DefaultLog.WriteError(
                    "[CheckTransferFunctionString] There should be at least two entries in 'Nodes' array.");
                check = false;
            }
            for (UINT i = 0; i < tmp_size; ++i) {
                if (!json.at("Nodes")[i].is_array()) {
                    vislib::sys::Log::DefaultLog.WriteError(
                        "[CheckTransferFunctionString] Entries of 'Nodes' should be arrays.");
                    check = false;
                }
                else {
                    if (json.at("Nodes")[i].size() != 5) {
                        vislib::sys::Log::DefaultLog.WriteError(
                            "[CheckTransferFunctionString] Entries of 'Nodes' should be arrays of size 5.");
                        check = false;
                    }
                    else {
                        for (UINT k = 0; k < 5; ++k) {
                            if (!json.at("Nodes")[i][k].is_number_float()) {
                                vislib::sys::Log::DefaultLog.WriteError(
                                    "[CheckTransferFunctionString] Values in 'Nodes' arrays should be floating point numbers.");
                                check = false;
                           }
                        }
                    }

                }
            }
        }
        else {
            vislib::sys::Log::DefaultLog.WriteError(
                "[CheckTransferFunctionString] Couldn't read 'Nodes' as array.");
            check = false;
        }
    }

    return check;
}