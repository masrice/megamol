/*
 * FlagCallsGL.h
 *
 * Author: Guido Reina and others
 * Copyright (C) 2016-2021 by Universitaet Stuttgart (VISUS).
 * All rights reserved.
 */

#pragma once

#include "mmcore/CallGeneric.h"
#include "mmcore/FlagStorage.h"
#include "mmcore/factories/CallAutoDescription.h"
#include "mmcore_gl/UniFlagStorage.h"


namespace megamol {
namespace core_gl {

class MEGAMOLCORE_API FlagCallRead_GL
        : public core::GenericVersionedCall<std::shared_ptr<FlagCollection_GL>, core::EmptyMetaData> {
public:
    inline FlagCallRead_GL() = default;
    ~FlagCallRead_GL() = default;

    static const char* ClassName(void) {
        return "FlagCallRead_GL";
    }
    static const char* Description(void) {
        return "Call that transports a buffer object representing a FlagStorage in a shader storage buffer for "
               "reading";
    }
};

class MEGAMOLCORE_API FlagCallWrite_GL
        : public core::GenericVersionedCall<std::shared_ptr<FlagCollection_GL>, core::EmptyMetaData> {
public:
    inline FlagCallWrite_GL() = default;
    ~FlagCallWrite_GL() = default;

    static const char* ClassName(void) {
        return "FlagCallWrite_GL";
    }
    static const char* Description(void) {
        return "Call that transports a buffer object representing a FlagStorage in a shader storage buffer for "
               "writing";
    }
};

/** Description class typedef */
typedef megamol::core::factories::CallAutoDescription<FlagCallRead_GL> FlagCallRead_GLDescription;
typedef megamol::core::factories::CallAutoDescription<FlagCallWrite_GL> FlagCallWrite_GLDescription;

} // namespace core_gl
} /* end namespace megamol */
