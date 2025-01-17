/*
 * FEMGPURenderTaskDataSource.h
 *
 * Copyright (C) 2019 by Universitaet Stuttgart (VISUS).
 * All rights reserved.
 */

#ifndef FEM_GPU_TASK_DATA_SOURCE_H_INCLUDED
#define FEM_GPU_TASK_DATA_SOURCE_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "mesh/AbstractGPURenderTaskDataSource.h"

namespace megamol {
namespace archvis {

class FEMRenderTaskDataSource : public mesh::AbstractGPURenderTaskDataSource {
public:
    /**
     * Answer the name of this module.
     *
     * @return The name of this module.
     */
    static const char* ClassName(void) {
        return "FEMGPURenderTasksDataSource";
    }

    /**
     * Answer a human readable description of this module.
     *
     * @return A human readable description of this module.
     */
    static const char* Description(void) {
        return "Data source for loading render tasks based on FEM data";
    }

    /**
     * Answers whether this module is available on the current system.
     *
     * @return 'true' if the module is available, 'false' otherwise.
     */
    static bool IsAvailable(void) {
        return true;
    }


    FEMRenderTaskDataSource();
    ~FEMRenderTaskDataSource();

protected:
    /**
     * Implementation of 'Create'.
     *
     * @return 'true' on success, 'false' otherwise.
     */
    virtual bool create(void);

    virtual bool getDataCallback(core::Call& caller);

private:
    uint32_t m_version;

    megamol::core::CallerSlot m_fem_callerSlot;

    /** In-place material collection (initialized with gltf btf) */
    std::shared_ptr<mesh::GPUMaterialCollection> m_material_collection;
};

} // namespace archvis
} // namespace megamol

#endif // !FEM_GPU_RENDER_TASK_DATA_SOURCE_H_INCLUDED
