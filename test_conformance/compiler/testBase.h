//
// Copyright (c) 2017 The Khronos Group Inc.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#ifndef _testBase_h
#define _testBase_h

#include <sys/stat.h>
#include <vector>

#include <CL/cl.h>

#include "harness/conversions.h"
#include "harness/testHarness.h"
#include "harness/typeWrappers.h"

// This is a macro rather than a function to be able to use and act like the
// existing test_error macro.
//
// Not all compiler tests need to use this macro, only those that don't use the
// test harness compiler helpers.
#define check_compiler_available(DEVICE)                                       \
    {                                                                          \
        cl_bool compilerAvailable = CL_FALSE;                                  \
        cl_int error = clGetDeviceInfo((DEVICE), CL_DEVICE_COMPILER_AVAILABLE, \
                                       sizeof(compilerAvailable),              \
                                       &compilerAvailable, NULL);              \
        test_error(error, "Unable to query CL_DEVICE_COMPILER_AVAILABLE");     \
        if (compilerAvailable == CL_FALSE)                                     \
        {                                                                      \
            log_info("Skipping test - no compiler is available.\n");           \
            return TEST_SKIPPED_ITSELF;                                        \
        }                                                                      \
    }


#endif // _testBase_h
