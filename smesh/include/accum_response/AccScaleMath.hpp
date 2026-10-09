// **********************************************************************
// smesh/include/accum_response/AccScaleMath.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026
/*
Default Original accumulator scale arithmetic for one selected element.
This models the arithmetic only; pipeline timing is a separate block concern.
*/
#pragma once

#include "AccScaleLane.hpp"

namespace smesh {

Elem scaleAccumValue(Acc value, u32 scale_bits);
AccScaleResult scaleAccumulatorElement(const AccScaleElem& input);

} // namespace smesh
