#pragma once

#include <string>
#include "PrecisionTypes.h"

inline std::string precisionToString(
    PrecisionType p) {

    switch(p) {

        case FP64:
            return "double";

        case FP32:
            return "float";

        case FP16:
            return "__fp16";

        case BF16:
            return "__bf16";
    }

    return "float";
}