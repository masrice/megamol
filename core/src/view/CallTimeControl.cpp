/*
 * CallTimeControl.cpp
 *
 * Copyright (C) 2012 by CGV (TU Dresden)
 * Alle Rechte vorbehalten.
 */

#include "mmcore/view/CallTimeControl.h"
#include "stdafx.h"

using namespace megamol::core;


/*
 * view::CallTimeControl::CallTimeControl
 */
view::CallTimeControl::CallTimeControl(void) : Call(), m(NULL) {
    // intentionally empty
}


/*
 * view::CallTimeControl::~CallTimeControl
 */
view::CallTimeControl::~CallTimeControl(void) {
    this->m = NULL; // do not delete
}
