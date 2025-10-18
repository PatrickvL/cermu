/*
 * fam65xx_core.cpp - Core Function Implementations
 *
 * This file provides the actual implementations of the core fam65xx functions
 * to prevent multiple definition errors when headers are included in multiple
 * translation units.
 */

#define AIEMUC_IMPL
#include "fam65xx.hpp"

// The implementations are already defined in fam65xx_impl.hpp
// This file just ensures they get compiled once and pulls in all dependencies.