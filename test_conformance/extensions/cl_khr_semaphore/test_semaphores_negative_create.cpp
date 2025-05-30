//
// Copyright (c) 2024 The Khronos Group Inc.
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

#include "semaphore_base.h"

#include "harness/errorHelpers.h"
#include <array>
#include <chrono>
#include <system_error>
#include <thread>

namespace {

// CL_INVALID_CONTEXT if context is nullptr.
struct CreateInvalidContext : public SemaphoreTestBase
{
    CreateInvalidContext(cl_device_id device, cl_context context,
                         cl_command_queue queue, cl_int nelems)
        : SemaphoreTestBase(device, context, queue, nelems)
    {}

    cl_int Run() override
    {
        // Create semaphore
        cl_semaphore_properties_khr sema_props[] = {
            (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_KHR,
            (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_BINARY_KHR, 0
        };

        cl_int err = CL_SUCCESS;
        semaphore =
            clCreateSemaphoreWithPropertiesKHR(nullptr, sema_props, &err);
        test_failure_error(
            err, CL_INVALID_CONTEXT,
            "Unexpected clCreateSemaphoreWithPropertiesKHR return");

        return CL_SUCCESS;
    }
};

// (1) property name in sema_props is not a supported property name,
// (2) value specified for a supported property name is not valid,
// (3) the same property name is specified more than once.
struct CreateInvalidProperty : public SemaphoreTestBase
{
    CreateInvalidProperty(cl_device_id device, cl_context context,
                          cl_command_queue queue, cl_int nelems)
        : SemaphoreTestBase(device, context, queue, nelems)
    {}

    cl_int Run() override
    {
        cl_int err = CL_SUCCESS;
        // Create semaphore with invalid properties:
        // 1) Property name in sema_props is not a supported property name
        {
            cl_semaphore_properties_khr invalid_property_name = ~0UL;
            cl_semaphore_properties_khr sema_props[] = {
                invalid_property_name,
                (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_BINARY_KHR, 0
            };

            semaphore =
                clCreateSemaphoreWithPropertiesKHR(context, sema_props, &err);

            if (err != CL_INVALID_PROPERTY && err != CL_INVALID_VALUE)
            {
                log_error("Unexpected clCreateSemaphoreWithPropertiesKHR "
                          "result, expected "
                          "CL_INVALID_PROPERTY or CL_INVALID_VALUE, got %s\n",
                          IGetErrorString(err));
                return TEST_FAIL;
            }
        }

        // 2) Value specified for a supported property name is not valid
        {
            cl_semaphore_properties_khr invalid_property_value = ~0UL;
            cl_semaphore_properties_khr sema_props[] = {
                (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_KHR,
                invalid_property_value, 0
            };

            semaphore =
                clCreateSemaphoreWithPropertiesKHR(context, sema_props, &err);

            if (err != CL_INVALID_PROPERTY && err != CL_INVALID_VALUE)
            {
                log_error("Unexpected clCreateSemaphoreWithPropertiesKHR "
                          "result, expected "
                          "CL_INVALID_PROPERTY or CL_INVALID_VALUE, got %s\n",
                          IGetErrorString(err));
                return TEST_FAIL;
            }
        }

        // 3) The same property name is specified more than once
        {
            cl_semaphore_properties_khr sema_props[] = {
                (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_KHR,
                (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_BINARY_KHR,
                (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_KHR, 0
            };

            semaphore =
                clCreateSemaphoreWithPropertiesKHR(context, sema_props, &err);
            test_failure_error(
                err, CL_INVALID_PROPERTY,
                "Unexpected clCreateSemaphoreWithPropertiesKHR return");
        }

        return TEST_PASS;
    }
};

// Context is a multiple device context and sema_props does not
// specify CL_SEMAPHORE_DEVICE_HANDLE_LIST_KHR.
struct CreateInvalidMultiDeviceProperty : public SemaphoreTestBase
{
    CreateInvalidMultiDeviceProperty(cl_device_id device, cl_context context,
                                     cl_command_queue queue, cl_int nelems)
        : SemaphoreTestBase(device, context, queue, nelems)
    {}

    cl_int Run() override
    {
        // partition device and create new context if possible
        size_t size = 0;

        // query multi-device context and perform objects comparability test
        cl_int err = clGetDeviceInfo(device, CL_DEVICE_PARTITION_PROPERTIES, 0,
                                     nullptr, &size);
        test_error_fail(err, "clGetDeviceInfo failed");

        if ((size / sizeof(cl_device_partition_property)) == 0)
        {
            log_info("Can't partition device, test not supported\n");
            return TEST_SKIPPED_ITSELF;
        }

        std::vector<cl_device_partition_property> supported_props(
            size / sizeof(cl_device_partition_property), 0);
        err = clGetDeviceInfo(device, CL_DEVICE_PARTITION_PROPERTIES,
                              supported_props.size()
                                  * sizeof(cl_device_partition_property),
                              supported_props.data(), nullptr);
        test_error_fail(err, "clGetDeviceInfo failed");

        if (supported_props.front() == 0)
        {
            log_info("Can't partition device, test not supported\n");
            return TEST_SKIPPED_ITSELF;
        }

        cl_uint maxComputeUnits = 0;
        err =
            clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS,
                            sizeof(maxComputeUnits), &maxComputeUnits, nullptr);
        test_error_ret(err, "Unable to get maximal number of compute units",
                       TEST_FAIL);

        std::vector<std::array<cl_device_partition_property, 5>>
            partition_props = {
                { CL_DEVICE_PARTITION_EQUALLY, (cl_int)maxComputeUnits / 2, 0,
                  0, 0 },
                { CL_DEVICE_PARTITION_BY_COUNTS, 1, (cl_int)maxComputeUnits - 1,
                  CL_DEVICE_PARTITION_BY_COUNTS_LIST_END, 0 },
                { CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN,
                  CL_DEVICE_AFFINITY_DOMAIN_NEXT_PARTITIONABLE, 0, 0, 0 }
            };

        std::unique_ptr<SubDevicesScopeGuarded> scope_guard;
        cl_uint num_devices = 0;
        for (auto &sup_prop : supported_props)
        {
            for (auto &prop : partition_props)
            {
                if (sup_prop == prop[0])
                {
                    // how many sub-devices can we create?
                    err = clCreateSubDevices(device, prop.data(), 0, nullptr,
                                             &num_devices);
                    test_error_fail(err, "clCreateSubDevices failed");
                    if (num_devices < 2) continue;

                    // get the list of subDevices
                    scope_guard.reset(new SubDevicesScopeGuarded(num_devices));
                    err = clCreateSubDevices(device, prop.data(), num_devices,
                                             scope_guard->sub_devices.data(),
                                             &num_devices);
                    test_error_fail(err, "clCreateSubDevices failed");
                    break;
                }
            }
            if (scope_guard.get() != nullptr) break;
        }

        if (scope_guard.get() == nullptr)
        {
            log_info("Can't partition device, test not supported\n");
            return TEST_SKIPPED_ITSELF;
        }

        /* Create a multi device context */
        clContextWrapper multi_device_context = clCreateContext(
            NULL, (cl_uint)num_devices, scope_guard->sub_devices.data(),
            nullptr, nullptr, &err);
        test_error_fail(err, "Unable to create testing context");

        cl_semaphore_properties_khr sema_props[] = {
            (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_KHR,
            (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_BINARY_KHR, 0
        };

        // Try to create semaphore with multi device context - expect
        // CL_INVALID_PROPERTY error
        semaphore = clCreateSemaphoreWithPropertiesKHR(multi_device_context,
                                                       sema_props, &err);
        test_failure_error(
            err, CL_INVALID_PROPERTY,
            "Unexpected clCreateSemaphoreWithPropertiesKHR return");

        return CL_SUCCESS;
    }
};

// (1) CL_SEMAPHORE_DEVICE_HANDLE_LIST_KHR is specified as part of sema_props,
// but it does not identify exactly one valid device,
// (2) device identified by CL_SEMAPHORE_DEVICE_HANDLE_LIST_KHR is not one of
// the devices within context.
struct CreateInvalidDevice : public SemaphoreTestBase
{
    CreateInvalidDevice(cl_device_id device, cl_context context,
                        cl_command_queue queue, cl_int nelems)
        : SemaphoreTestBase(device, context, queue, nelems)
    {}

    cl_int Run() override
    {
        // create sub devices if possible
        size_t size = 0;

        // query multi-device context and perform objects comparability test
        cl_int err = clGetDeviceInfo(device, CL_DEVICE_PARTITION_PROPERTIES, 0,
                                     nullptr, &size);
        test_error_fail(err, "clGetDeviceInfo failed");

        if ((size / sizeof(cl_device_partition_property)) == 0)
        {
            log_info("Can't partition device, test not supported\n");
            return TEST_SKIPPED_ITSELF;
        }

        std::vector<cl_device_partition_property> supported_props(
            size / sizeof(cl_device_partition_property), 0);
        err = clGetDeviceInfo(device, CL_DEVICE_PARTITION_PROPERTIES,
                              supported_props.size()
                                  * sizeof(cl_device_partition_property),
                              supported_props.data(), nullptr);
        test_error_fail(err, "clGetDeviceInfo failed");

        if (supported_props.front() == 0)
        {
            log_info("Can't partition device, test not supported\n");
            return TEST_SKIPPED_ITSELF;
        }

        cl_uint maxComputeUnits = 0;
        err =
            clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS,
                            sizeof(maxComputeUnits), &maxComputeUnits, nullptr);
        test_error_ret(err, "Unable to get maximal number of compute units",
                       TEST_FAIL);

        std::vector<std::array<cl_device_partition_property, 5>>
            partition_props = {
                { CL_DEVICE_PARTITION_EQUALLY, (cl_int)maxComputeUnits / 2, 0,
                  0, 0 },
                { CL_DEVICE_PARTITION_BY_COUNTS, 1, (cl_int)maxComputeUnits - 1,
                  CL_DEVICE_PARTITION_BY_COUNTS_LIST_END, 0 },
                { CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN,
                  CL_DEVICE_AFFINITY_DOMAIN_NEXT_PARTITIONABLE, 0, 0, 0 }
            };

        std::unique_ptr<SubDevicesScopeGuarded> scope_guard;
        cl_uint num_devices = 0;
        for (auto &sup_prop : supported_props)
        {
            for (auto &prop : partition_props)
            {
                if (sup_prop == prop[0])
                {
                    // how many sub-devices can we create?
                    err = clCreateSubDevices(device, prop.data(), 0, nullptr,
                                             &num_devices);
                    test_error_fail(err, "clCreateSubDevices failed");
                    if (num_devices < 2) continue;

                    // get the list of subDevices
                    scope_guard.reset(new SubDevicesScopeGuarded(num_devices));
                    err = clCreateSubDevices(device, prop.data(), num_devices,
                                             scope_guard->sub_devices.data(),
                                             &num_devices);
                    test_error_fail(err, "clCreateSubDevices failed");
                    break;
                }
            }
            if (scope_guard.get() != nullptr) break;
        }

        if (scope_guard.get() == nullptr)
        {
            log_info("Can't partition device, test not supported\n");
            return TEST_SKIPPED_ITSELF;
        }

        // CL_SEMAPHORE_DEVICE_HANDLE_LIST_KHR is specified as part of
        // sema_props, but it does not identify exactly one valid device;
        {
            cl_semaphore_properties_khr sema_props[] = {
                (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_KHR,
                (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_BINARY_KHR,
                (cl_semaphore_properties_khr)
                    CL_SEMAPHORE_DEVICE_HANDLE_LIST_KHR,
                (cl_semaphore_properties_khr)device,
                (cl_semaphore_properties_khr)scope_guard->sub_devices.front(),
                (cl_semaphore_properties_khr)
                    CL_SEMAPHORE_DEVICE_HANDLE_LIST_END_KHR,
                0
            };

            // Try to create semaphore with more than one valid device,
            // expect CL_INVALID_DEVICE error
            semaphore =
                clCreateSemaphoreWithPropertiesKHR(context, sema_props, &err);
            test_failure_error(
                err, CL_INVALID_DEVICE,
                "Unexpected clCreateSemaphoreWithPropertiesKHR return");
        }

        // or if a device identified by CL_SEMAPHORE_DEVICE_HANDLE_LIST_KHR
        // is not one of the devices within context.
        {
            /* Create new context with sub-device */
            clContextWrapper new_context = clCreateContext(
                NULL, (cl_uint)1, scope_guard->sub_devices.data(), nullptr,
                nullptr, &err);
            test_error_ret(err, "Unable to create testing context", CL_SUCCESS);

            cl_semaphore_properties_khr sema_props[] = {
                (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_KHR,
                (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_BINARY_KHR,
                (cl_semaphore_properties_khr)
                    CL_SEMAPHORE_DEVICE_HANDLE_LIST_KHR,
                (cl_semaphore_properties_khr)device,
                (cl_semaphore_properties_khr)
                    CL_SEMAPHORE_DEVICE_HANDLE_LIST_END_KHR,
                0
            };

            // Try to create semaphore with device not one of the devices within
            // context, expect CL_INVALID_DEVICE error
            semaphore = clCreateSemaphoreWithPropertiesKHR(new_context,
                                                           sema_props, &err);
            test_failure_error(
                err, CL_INVALID_DEVICE,
                "Unexpected clCreateSemaphoreWithPropertiesKHR return");
        }

        return CL_SUCCESS;
    }
};

// CL_INVALID_DEVICE if one or more devices identified by properties
// CL_SEMAPHORE_DEVICE_HANDLE_LIST_KHR cannot import the requested
// external semaphore handle type.
struct CreateImportExternalWithInvalidDevice : public SemaphoreTestBase
{
    CreateImportExternalWithInvalidDevice(cl_device_id device,
                                          cl_context context,
                                          cl_command_queue queue, cl_int nelems)
        : SemaphoreTestBase(device, context, queue, nelems),
          semaphore_second(this)
    {}

    cl_int Run() override
    {
        if (!is_extension_available(device,
                                    "cl_khr_external_semaphore_opaque_fd"))
        {
            log_info(
                "cl_khr_external_semaphore_opaque_fd is not supported on this "
                "platform. Skipping test.\n");
            return TEST_SKIPPED_ITSELF;
        }

        // The idea is to find two devices, one supporting cl_khr_semaphore,
        // other not. Then create semaphore with valid device and import to new
        // semaphore with incapable device.
        cl_platform_id platform_id = 0;
        cl_uint num_devices = 0;
        // find out what platform the harness is using.
        cl_int err =
            clGetDeviceInfo(device, CL_DEVICE_PLATFORM, sizeof(cl_platform_id),
                            &platform_id, nullptr);
        test_error(err, "clGetDeviceInfo failed");

        cl_uint num_platforms = 0;
        err = clGetPlatformIDs(16, nullptr, &num_platforms);
        test_error(err, "clGetPlatformIDs failed");

        std::vector<cl_platform_id> platforms(num_platforms);

        err = clGetPlatformIDs(num_platforms, platforms.data(), &num_platforms);
        test_error(err, "clGetPlatformIDs failed");

        cl_device_id invalid_device = nullptr;
        for (int p = 0; p < (int)num_platforms; p++)
        {
            if (platform_id == platforms[p]) continue;

            err = clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_ALL, 0, nullptr,
                                 &num_devices);
            test_error(err, "clGetDeviceIDs failed");

            std::vector<cl_device_id> devices(num_devices);
            err = clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_ALL, num_devices,
                                 devices.data(), nullptr);
            test_error(err, "clGetDeviceIDs failed");

            // try to find invalid device
            for (auto did : devices)
            {
                if (!is_extension_available(did, "cl_khr_semaphore"))
                {
                    invalid_device = did;
                    break;
                }
            }

            if (invalid_device != nullptr) break;
        }

        if (invalid_device == nullptr)
        {
            log_info("Can't find needed resources. Skipping test.\n");
            return TEST_SKIPPED_ITSELF;
        }

        // Create ooo queue
        clCommandQueueWrapper queue = clCreateCommandQueue(
            context, device, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);
        test_error(err, "Could not create command queue");

        // Create semaphore
        cl_semaphore_properties_khr sema_1_props[] = {
            (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_KHR,
            (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_BINARY_KHR,
            (cl_semaphore_properties_khr)CL_SEMAPHORE_EXPORT_HANDLE_TYPES_KHR,
            (cl_semaphore_properties_khr)CL_SEMAPHORE_HANDLE_OPAQUE_FD_KHR,
            (cl_semaphore_properties_khr)
                CL_SEMAPHORE_EXPORT_HANDLE_TYPES_LIST_END_KHR,
            0
        };
        semaphore =
            clCreateSemaphoreWithPropertiesKHR(context, sema_1_props, &err);
        test_error(err, "Could not create semaphore");

        // Signal semaphore
        clEventWrapper signal_event;
        err = clEnqueueSignalSemaphoresKHR(queue, 1, semaphore, nullptr, 0,
                                           nullptr, &signal_event);
        test_error(err, "Could not signal semaphore");

        // Extract sync fd
        int handle = -1;
        size_t handle_size;
        err = clGetSemaphoreHandleForTypeKHR(
            semaphore, device, CL_SEMAPHORE_HANDLE_OPAQUE_FD_KHR,
            sizeof(handle), &handle, &handle_size);
        test_error(err, "Could not extract semaphore handle");
        test_assert_error(sizeof(handle) == handle_size, "Invalid handle size");
        test_assert_error(handle >= 0, "Invalid handle");

        /* Create invalid device context */
        clContextWrapper invalid_device_context =
            clCreateContext(NULL, 1, &invalid_device, nullptr, nullptr, &err);
        test_error_ret(err, "Unable to create testing context", CL_SUCCESS);

        // Create semaphore from sync fd
        cl_semaphore_properties_khr sema_2_props[] = {
            (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_KHR,
            (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_BINARY_KHR,
            (cl_semaphore_properties_khr)CL_SEMAPHORE_HANDLE_OPAQUE_FD_KHR,
            (cl_semaphore_properties_khr)handle,
            (cl_semaphore_properties_khr)CL_SEMAPHORE_DEVICE_HANDLE_LIST_KHR,
            (cl_semaphore_properties_khr)invalid_device,
            (cl_semaphore_properties_khr)
                CL_SEMAPHORE_DEVICE_HANDLE_LIST_END_KHR,
            0
        };

        semaphore_second = clCreateSemaphoreWithPropertiesKHR(
            invalid_device_context, sema_2_props, &err);
        test_failure_error(
            err, CL_INVALID_DEVICE,
            "Unexpected clCreateSemaphoreWithPropertiesKHR return");

        return TEST_PASS;
    }

    clSemaphoreWrapper semaphore_second = nullptr;
};

// (1) if sema_props is NULL,
// (2) if sema_props do not specify <property, value> pairs for minimum set of
// properties (i.e. CL_SEMAPHORE_TYPE_KHR) required for successful creation of a
// cl_semaphore_khr,
// (3) one or more devices identified by properties
// CL_SEMAPHORE_DEVICE_HANDLE_LIST_KHR cannot import the requested external
// semaphore handle type.
struct CreateInvalidValue : public SemaphoreTestBase
{
    CreateInvalidValue(cl_device_id device, cl_context context,
                       cl_command_queue queue, cl_int nelems)
        : SemaphoreTestBase(device, context, queue, nelems)
    {}

    cl_int Run() override
    {
        cl_int err = CL_SUCCESS;

        // (1)
        semaphore = clCreateSemaphoreWithPropertiesKHR(context, nullptr, &err);
        test_failure_error(
            err, CL_INVALID_VALUE,
            "Unexpected clCreateSemaphoreWithPropertiesKHR return");

        // (2)
        cl_semaphore_properties_khr sema_props[] = { 0 };
        semaphore =
            clCreateSemaphoreWithPropertiesKHR(context, sema_props, &err);
        test_failure_error(
            err, CL_INVALID_VALUE,
            "Unexpected clCreateSemaphoreWithPropertiesKHR return");


        // (3)
        {
            if (!is_extension_available(device,
                                        "cl_khr_external_semaphore_opaque_fd"))
            {
                log_info("cl_khr_external_semaphore_opaque_fd is not supported "
                         "on this "
                         "platform. Skipping test.\n");
                return TEST_SKIPPED_ITSELF;
            }

            // Create semaphore
            cl_semaphore_properties_khr sema_1_props[] = {
                (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_KHR,
                (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_BINARY_KHR,
                (cl_semaphore_properties_khr)
                    CL_SEMAPHORE_EXPORT_HANDLE_TYPES_KHR,
                (cl_semaphore_properties_khr)CL_SEMAPHORE_HANDLE_OPAQUE_FD_KHR,
                (cl_semaphore_properties_khr)CL_SEMAPHORE_HANDLE_OPAQUE_FD_KHR,
                (cl_semaphore_properties_khr)
                    CL_SEMAPHORE_EXPORT_HANDLE_TYPES_LIST_END_KHR,
                0
            };
            semaphore =
                clCreateSemaphoreWithPropertiesKHR(context, sema_1_props, &err);
            test_failure_error(
                err, CL_INVALID_VALUE,
                "Unexpected clCreateSemaphoreWithPropertiesKHR return");
        }

        return CL_SUCCESS;
    }
};

// props_list specifies a cl_external_semaphore_handle_type_khr followed by
// a handle as well as CL_SEMAPHORE_EXPORT_HANDLE_TYPES_KHR.
struct CreateInvalidOperation : public SemaphoreTestBase
{
    CreateInvalidOperation(cl_device_id device, cl_context context,
                           cl_command_queue queue, cl_int nelems)
        : SemaphoreTestBase(device, context, queue, nelems),
          semaphore_second(this)
    {}

    cl_int Run() override
    {
        if (!is_extension_available(device,
                                    "cl_khr_external_semaphore_opaque_fd"))
        {
            log_info(
                "cl_khr_external_semaphore_opaque_fd is not supported on this "
                "platform. Skipping test.\n");
            return TEST_SKIPPED_ITSELF;
        }

        cl_int err = CL_SUCCESS;
        clCommandQueueWrapper queue = clCreateCommandQueue(
            context, device, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);
        test_error(err, "Could not create command queue");

        // Create semaphore
        cl_semaphore_properties_khr sema_1_props[] = {
            (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_KHR,
            (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_BINARY_KHR,
            (cl_semaphore_properties_khr)CL_SEMAPHORE_EXPORT_HANDLE_TYPES_KHR,
            (cl_semaphore_properties_khr)CL_SEMAPHORE_HANDLE_OPAQUE_FD_KHR,
            (cl_semaphore_properties_khr)
                CL_SEMAPHORE_EXPORT_HANDLE_TYPES_LIST_END_KHR,
            0
        };
        semaphore =
            clCreateSemaphoreWithPropertiesKHR(context, sema_1_props, &err);
        test_error(err, "Could not create semaphore");

        // Signal semaphore
        clEventWrapper signal_event;
        err = clEnqueueSignalSemaphoresKHR(queue, 1, semaphore, nullptr, 0,
                                           nullptr, &signal_event);
        test_error(err, "Could not signal semaphore");

        // Extract sync fd
        int handle = -1;
        size_t handle_size;
        err = clGetSemaphoreHandleForTypeKHR(
            semaphore, device, CL_SEMAPHORE_HANDLE_OPAQUE_FD_KHR,
            sizeof(handle), &handle, &handle_size);
        test_error(err, "Could not extract semaphore handle");
        test_assert_error(sizeof(handle) == handle_size, "Invalid handle size");
        test_assert_error(handle >= 0, "Invalid handle");

        // Create semaphore from sync fd. Exporting a semaphore handle from
        // a semaphore that was created by importing an external semaphore
        // handle is not permitted.
        cl_semaphore_properties_khr sema_2_props[] = {
            (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_KHR,
            (cl_semaphore_properties_khr)CL_SEMAPHORE_TYPE_BINARY_KHR,
            (cl_semaphore_properties_khr)CL_SEMAPHORE_HANDLE_OPAQUE_FD_KHR,
            (cl_semaphore_properties_khr)(handle),
            (cl_semaphore_properties_khr)CL_SEMAPHORE_EXPORT_HANDLE_TYPES_KHR,
            (cl_semaphore_properties_khr)CL_SEMAPHORE_HANDLE_OPAQUE_FD_KHR,
            (cl_semaphore_properties_khr)
                CL_SEMAPHORE_EXPORT_HANDLE_TYPES_LIST_END_KHR,
            0
        };

        semaphore_second =
            clCreateSemaphoreWithPropertiesKHR(context, sema_2_props, &err);
        test_failure_error(
            err, CL_INVALID_OPERATION,
            "Unexpected clCreateSemaphoreWithPropertiesKHR return");

        return CL_SUCCESS;
    }

    clSemaphoreWrapper semaphore_second = nullptr;
};

}

// Confirm that creation semaphore with nullptr context would return
// CL_INVALID_CONTEXT
REGISTER_TEST_VERSION(semaphores_negative_create_invalid_context, Version(1, 2))
{
    return MakeAndRunTest<CreateInvalidContext>(device, context, queue,
                                                num_elements);
}

// Confirm that creation semaphore with invalid properties return
// CL_INVALID_PROPERTY
REGISTER_TEST_VERSION(semaphores_negative_create_invalid_property,
                      Version(1, 2))
{
    return MakeAndRunTest<CreateInvalidProperty>(device, context, queue,
                                                 num_elements);
}

// Confirm that creation semaphore with multi device property return
// CL_INVALID_PROPERTY
REGISTER_TEST_VERSION(semaphores_negative_create_multi_device_property,
                      Version(1, 2))
{
    return MakeAndRunTest<CreateInvalidMultiDeviceProperty>(
        device, context, queue, num_elements);
}

// Confirm that creation semaphore with invalid device(s) return
// CL_INVALID_DEVICE
REGISTER_TEST_VERSION(semaphores_negative_create_invalid_device, Version(1, 2))
{
    return MakeAndRunTest<CreateInvalidDevice>(device, context, queue,
                                               num_elements);
}

// Confirm that creation semaphore with invalid device(s) return
// CL_INVALID_DEVICE
REGISTER_TEST_VERSION(semaphores_negative_create_import_invalid_device,
                      Version(1, 2))
{
    return MakeAndRunTest<CreateImportExternalWithInvalidDevice>(
        device, context, queue, num_elements);
}

// Confirm that creation semaphore with invalid props values return
// CL_INVALID_VALUE
REGISTER_TEST_VERSION(semaphores_negative_create_invalid_value, Version(1, 2))
{
    return MakeAndRunTest<CreateInvalidValue>(device, context, queue,
                                              num_elements);
}

// Confirm that creation semaphore with invalid props values return
// CL_INVALID_VALUE
REGISTER_TEST_VERSION(semaphores_negative_create_invalid_operation,
                      Version(1, 2))
{
    return MakeAndRunTest<CreateInvalidOperation>(device, context, queue,
                                                  num_elements);
}
