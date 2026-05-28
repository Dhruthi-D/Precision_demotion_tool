#include "PropagationEngine.h"

// ==========================================
// Generalized Precision Selection Engine
// ==========================================

PrecisionType choosePrecision(
    double error) {

    // ======================================
    // Extremely safe
    // ======================================

    if(error < 1e-4)
        return FP16;

    // ======================================
    // Moderate precision needed
    // ======================================

    if(error < 1e-2)
        return BF16;

    // ======================================
    // Higher precision needed
    // ======================================

    if(error < 1)
        return FP32;

    // ======================================
    // Very high precision needed
    // ======================================

    return FP64;
}

PrecisionType choosePrecision(
    double error,
    double tolerance) {

    if(tolerance <= 0)
        return FP64;

    // Keep a safety margin: local estimated error should consume only
    // part of the user-visible output budget.
    if(error <= tolerance * 0.25)
        return FP16;

    if(error <= tolerance * 0.5)
        return BF16;

    if(error <= tolerance)
        return FP32;

    return FP64;
}
