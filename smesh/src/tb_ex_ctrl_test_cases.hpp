// **********************************************************************
// smesh/src/tb_ex_ctrl_test_cases.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 17 2026
/*
Declarations of test scenarios.
*/
#pragma once

#include "tb_ex_ctrl_dsl.hpp"

#include <vector>

namespace smesh {
namespace tb {

ExCtrlTestCase makeBasicFlipStayTest();
ExCtrlTestCase makeComputePreloadOverlapTest();
std::vector<ExCtrlTestCase> exCtrlTestCases();

} // namespace tb
} // namespace smesh

