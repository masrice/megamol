/*
 * VolumeMeshRenderer.h
 *
 * Copyright (C) 2012 by Universitaet Stuttgart (VIS).
 * Alle Rechte vorbehalten.
 */

#ifndef MEGAMOLCORE_MAPPABLECATEGORYFLOAT_H_INCLUDED
#define MEGAMOLCORE_MAPPABLECATEGORYFLOAT_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "protein_calls/DiagramCall.h"
#include "vislib/Pair.h"

namespace megamol {
namespace protein_cuda {

class MappableCategoryFloat : public protein_calls::DiagramCall::DiagramMappable {
public:
    MappableCategoryFloat(int instance = 0);
    ~MappableCategoryFloat(void);

    virtual int GetAbscissaeCount() const;
    virtual int GetDataCount() const;
    virtual bool IsCategoricalAbscissa(const SIZE_T abscissa) const;
    virtual bool GetAbscissaValue(const SIZE_T index, const SIZE_T abscissaIndex, vislib::StringA* category) const;
    virtual bool GetAbscissaValue(const SIZE_T index, const SIZE_T abscissaIndex, float* value) const;
    virtual float GetOrdinateValue(const SIZE_T index) const;
    virtual vislib::Pair<float, float> GetAbscissaRange(const SIZE_T abscissaIndex) const;
    virtual vislib::Pair<float, float> GetOrdinateRange() const;

private:
    int instance;
};

} /* namespace protein_cuda */
} /* namespace megamol */

#endif /* MEGAMOLCORE_MAPPABLECATEGORYFLOAT_H_INCLUDED */
