#pragma once

#include <string>
#include "PrecisionTypes.h"

struct PrecisionInfo {

    std::string variable;

    double estimatedError;

    PrecisionType assignedType;
};