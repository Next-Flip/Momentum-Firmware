#pragma once

#include "widget.h"
#include <furi.h>
#include <m-array.h>

struct Widget {
    View* view;
    void* context;
};
