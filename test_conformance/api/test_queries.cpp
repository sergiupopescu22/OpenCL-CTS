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
#include "testBase.h"
#include "harness/imageHelpers.h"
#include "harness/propertyHelpers.h"
#include <stdlib.h>
#include <ctype.h>
#include <array>
#include <algorithm>
#include <cinttypes>
#include <memory>
#include <vector>

REGISTER_TEST(get_platform_info)
{
    cl_platform_id platform;
    cl_int error;
    char buffer[ 16384 ];
    size_t length;

    // Get the platform to use
    error = clGetPlatformIDs(1, &platform, NULL);
    test_error( error, "Unable to get platform" );

    // Platform profile should either be FULL_PROFILE or EMBEDDED_PROFILE
    error = clGetPlatformInfo(platform,  CL_PLATFORM_PROFILE, sizeof( buffer ), buffer, &length );
    test_error( error, "Unable to get platform profile string" );

    log_info("Returned CL_PLATFORM_PROFILE %s.\n", buffer);

    if( strcmp( buffer, "FULL_PROFILE" ) != 0 && strcmp( buffer, "EMBEDDED_PROFILE" ) != 0 )
    {
        log_error( "ERROR: Returned platform profile string is not a valid string by OpenCL 1.2! (Returned: %s)\n", buffer );
        return -1;
    }
    if( strlen( buffer )+1 != length )
    {
        log_error( "ERROR: Returned length of profile string is incorrect (actual length: %d, returned length: %d)\n",
                  (int)strlen( buffer )+1, (int)length );
        return -1;
    }

    // Check just length return
    error = clGetPlatformInfo(platform,  CL_PLATFORM_PROFILE, 0, NULL, &length );
    test_error( error, "Unable to get platform profile length" );
    if( strlen( (char *)buffer )+1 != length )
    {
        log_error( "ERROR: Returned length of profile string is incorrect (actual length: %d, returned length: %d)\n",
                  (int)strlen( (char *)buffer )+1, (int)length );
        return -1;
    }


    // Platform version should fit the regex "OpenCL *[0-9]+\.[0-9]+"
    error = clGetPlatformInfo(platform,  CL_PLATFORM_VERSION, sizeof( buffer ), buffer, &length );
    test_error( error, "Unable to get platform version string" );

    log_info("Returned CL_PLATFORM_VERSION %s.\n", buffer);

    if( memcmp( buffer, "OpenCL ", strlen( "OpenCL " ) ) != 0 )
    {
        log_error( "ERROR: Initial part of platform version string does not match required format! (returned: %s)\n", (char *)buffer );
        return -1;
    }
    char *p1 = (char *)buffer + strlen( "OpenCL " );
    while( *p1 == ' ' )
        p1++;
    char *p2 = p1;
    while( isdigit( *p2 ) )
        p2++;
    if( *p2 != '.' )
    {
        log_error( "ERROR: Numeric part of platform version string does not match required format! (returned: %s)\n", (char *)buffer );
        return -1;
    }
    char *p3 = p2 + 1;
    while( isdigit( *p3 ) )
        p3++;
    if( *p3 != ' ' )
    {
        log_error( "ERROR: space expected after minor version number! (returned: %s)\n", (char *)buffer );
        return -1;
    }
    *p2 = ' '; // Put in a space for atoi below.
    p2++;

    // make sure it is null terminated
    for( ; p3 != buffer + length; p3++ )
        if( *p3 == '\0' )
            break;
    if( p3 == buffer + length )
    {
        log_error( "ERROR: platform version string is not NUL terminated!\n" );
        return -1;
    }

    int major = atoi( p1 );
    int minor = atoi( p2 );
    int minor_revision = 2;
    if( major * 10 + minor < 10 + minor_revision )
    {
        log_error( "ERROR: OpenCL profile version returned is less than 1.%d!\n", minor_revision );
        return -1;
    }

    // Sanity checks on the returned values
    if( length != strlen( (char *)buffer ) + 1)
    {
        log_error( "ERROR: Returned length of version string does not match actual length (actual: %d, returned: %d)\n", (int)strlen( (char *)buffer )+1, (int)length );
        return -1;
    }

    // Check just length
    error = clGetPlatformInfo(platform,  CL_PLATFORM_VERSION, 0, NULL, &length );
    test_error( error, "Unable to get platform version length" );
    if( length != strlen( (char *)buffer )+1 )
    {
        log_error( "ERROR: Returned length of version string does not match actual length (actual: %d, returned: %d)\n", (int)strlen( buffer )+1, (int)length );
        return -1;
    }

    return 0;
}

template <typename T>
int sampler_param_test(cl_sampler sampler, cl_sampler_info param_name,
                       T expected, const char *name)
{
    size_t size;
    T val;
    int error = clGetSamplerInfo(sampler, param_name, sizeof(val), &val, &size);
    test_error(error, "Unable to get sampler info");
    if (val != expected)
    {
        test_fail("ERROR: Sampler %s did not validate!\n", name);
    }
    if (size != sizeof(val))
    {
        test_fail("ERROR: Returned size of sampler %s does not validate! "
                  "(expected %d, got %d)\n",
                  name, (int)sizeof(val), (int)size);
    }
    return 0;
}

static cl_bool normalized_coord_values[] = { CL_TRUE, CL_FALSE };
static cl_addressing_mode addressing_mode_values[] = {
    CL_ADDRESS_NONE, CL_ADDRESS_CLAMP_TO_EDGE, CL_ADDRESS_CLAMP,
    CL_ADDRESS_REPEAT, CL_ADDRESS_MIRRORED_REPEAT
};
static cl_filter_mode filter_mode_values[] = { CL_FILTER_NEAREST,
                                               CL_FILTER_LINEAR };

int test_sampler_params(cl_device_id device, cl_context context,
                        bool is_compatibility, size_t norm_coord_num,
                        size_t addr_mod_num, size_t filt_mod_num)
{
    cl_uint refCount;
    size_t size;
    int error;

    clSamplerWrapper sampler;
    cl_sampler_properties properties[] = {
        CL_SAMPLER_NORMALIZED_COORDS,
        normalized_coord_values[norm_coord_num],
        CL_SAMPLER_ADDRESSING_MODE,
        addressing_mode_values[addr_mod_num],
        CL_SAMPLER_FILTER_MODE,
        filter_mode_values[filt_mod_num],
        0
    };

    if (is_compatibility)
    {
        sampler =
            clCreateSampler(context, normalized_coord_values[norm_coord_num],
                            addressing_mode_values[addr_mod_num],
                            filter_mode_values[filt_mod_num], &error);
        test_error(error, "Unable to create sampler to test with");
    }
    else
    {
        sampler = clCreateSamplerWithProperties(context, properties, &error);
        test_error(error, "Unable to create sampler to test with");
    }

    error = clGetSamplerInfo(sampler, CL_SAMPLER_REFERENCE_COUNT,
                             sizeof(refCount), &refCount, &size);
    test_error(error, "Unable to get sampler ref count");
    test_assert_error(size == sizeof(refCount),
                      "Returned size of sampler refcount does not validate!\n");

    error = sampler_param_test(sampler, CL_SAMPLER_CONTEXT, context, "context");
    test_error(error, "param checking failed");

    error = sampler_param_test(sampler, CL_SAMPLER_ADDRESSING_MODE,
                               addressing_mode_values[addr_mod_num],
                               "addressing mode");
    test_error(error, "param checking failed");

    error = sampler_param_test(sampler, CL_SAMPLER_FILTER_MODE,
                               filter_mode_values[filt_mod_num], "filter mode");
    test_error(error, "param checking failed");

    error = sampler_param_test(sampler, CL_SAMPLER_NORMALIZED_COORDS,
                               normalized_coord_values[norm_coord_num],
                               "normalized coords");
    test_error(error, "param checking failed");

    Version version = get_device_cl_version(device);
    if (version >= Version(3, 0))
    {
        std::vector<cl_sampler_properties> test_properties(
            properties, properties + ARRAY_SIZE(properties));

        std::vector<cl_sampler_properties> check_properties;
        size_t set_size;

        error = clGetSamplerInfo(sampler, CL_SAMPLER_PROPERTIES, 0, NULL,
                                 &set_size);
        test_error(
            error,
            "clGetSamplerInfo failed asking for CL_SAMPLER_PROPERTIES size.");

        if (is_compatibility)
        {
            if (set_size != 0)
            {
                log_error(
                    "ERROR: CL_SAMPLER_PROPERTIES size is %zu, expected 0\n",
                    set_size);
                return TEST_FAIL;
            }
        }
        else
        {
            if (set_size
                != test_properties.size() * sizeof(cl_sampler_properties))
            {
                log_error(
                    "ERROR: CL_SAMPLER_PROPERTIES size is %zu, expected %zu.\n",
                    set_size,
                    test_properties.size() * sizeof(cl_sampler_properties));
                return TEST_FAIL;
            }

            cl_uint number_of_props = set_size / sizeof(cl_sampler_properties);
            check_properties.resize(number_of_props);
            error = clGetSamplerInfo(sampler, CL_SAMPLER_PROPERTIES, set_size,
                                     check_properties.data(), 0);
            test_error(
                error,
                "clGetSamplerInfo failed asking for CL_SAMPLER_PROPERTIES.");

            error = compareProperties(check_properties, test_properties);
            test_error(error, "checkProperties mismatch.");
        }
    }
    return 0;
}

int get_sampler_info_params(cl_device_id device, cl_context context,
                            bool is_compatibility)
{
    for (size_t norm_coord_num = 0;
         norm_coord_num < ARRAY_SIZE(normalized_coord_values); norm_coord_num++)
    {
        for (size_t addr_mod_num = 0;
             addr_mod_num < ARRAY_SIZE(addressing_mode_values); addr_mod_num++)
        {
            if ((normalized_coord_values[norm_coord_num] == CL_FALSE)
                && ((addressing_mode_values[addr_mod_num] == CL_ADDRESS_REPEAT)
                    || (addressing_mode_values[addr_mod_num]
                        == CL_ADDRESS_MIRRORED_REPEAT)))
            {
                continue;
            }
            for (size_t filt_mod_num = 0;
                 filt_mod_num < ARRAY_SIZE(filter_mode_values); filt_mod_num++)
            {
                int err = test_sampler_params(device, context, is_compatibility,
                                              norm_coord_num, addr_mod_num,
                                              filt_mod_num);
                test_error(err, "testing clGetSamplerInfo params failed");
            }
        }
    }
    return 0;
}
REGISTER_TEST_VERSION(get_sampler_info, Version(2, 0))
{
    int error;
    PASSIVE_REQUIRE_IMAGE_SUPPORT(device)

    error = get_sampler_info_params(device, context, false);
    test_error(error, "Test Failed");

    return 0;
}

REGISTER_TEST(get_sampler_info_compatibility)
{
    int error;
    PASSIVE_REQUIRE_IMAGE_SUPPORT(device)

    error = get_sampler_info_params(device, context, true);
    test_error(error, "Test Failed");

    return 0;
}

template <typename T>
int command_queue_param_test(cl_command_queue queue,
                             cl_command_queue_info param_name, T expected,
                             const char *name)
{
    size_t size;
    T val;
    int error =
        clGetCommandQueueInfo(queue, param_name, sizeof(val), &val, &size);
    test_error(error, "Unable to get command queue info");
    if (val != expected)
    {
        test_fail("ERROR: Command queue %s did not validate!\n", name);
    }
    if (size != sizeof(val))
    {
        test_fail("ERROR: Returned size of command queue %s does not validate! "
                  "(expected %d, got %d)\n",
                  name, (int)sizeof(val), (int)size);
    }
    return 0;
}

int check_get_command_queue_info_params(cl_device_id device, cl_context context,
                                        bool is_compatibility)
{
    const cl_command_queue_properties host_optional[] = {
        CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
        CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE
    };

    const cl_command_queue_properties device_required[] = {
        CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
        CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_ON_DEVICE
            | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
        CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT
            | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
        CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_ON_DEVICE
            | CL_QUEUE_ON_DEVICE_DEFAULT
            | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE
    };

    const size_t host_optional_size = ARRAY_SIZE(host_optional);
    const size_t device_required_size = ARRAY_SIZE(device_required);

    Version version = get_device_cl_version(device);

    const cl_device_info host_queue_query = version >= Version(2, 0)
        ? CL_DEVICE_QUEUE_ON_HOST_PROPERTIES
        : CL_DEVICE_QUEUE_PROPERTIES;

    cl_queue_properties host_queue_props = 0;
    int error =
        clGetDeviceInfo(device, host_queue_query, sizeof(host_queue_props),
                        &host_queue_props, NULL);
    test_error(error, "clGetDeviceInfo failed");
    log_info("CL_DEVICE_QUEUE_ON_HOST_PROPERTIES is %" PRIu64 "\n",
             host_queue_props);

    cl_queue_properties device_queue_props = 0;
    if (version >= Version(2, 0))
    {
        error = clGetDeviceInfo(device, CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES,
                                sizeof(device_queue_props), &device_queue_props,
                                NULL);
        test_error(error, "clGetDeviceInfo failed");
        log_info("CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES is %" PRIu64 "\n",
                 device_queue_props);
    }

    bool out_of_order_supported =
        host_queue_props & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;

    bool on_device_supported =
        (version >= Version(2, 0) && version < Version(3, 0))
        || (version >= Version(3, 0) && device_queue_props != 0);

    // test device queues if the device and the API under test support it
    bool test_on_device = on_device_supported && !is_compatibility;

    std::vector<cl_queue_properties> queue_props{ 0,
                                                  CL_QUEUE_PROFILING_ENABLE };

    if (out_of_order_supported)
    {
        queue_props.insert(queue_props.end(), &host_optional[0],
                           &host_optional[host_optional_size]);
    };

    cl_queue_properties queue_props_arg[] = { CL_QUEUE_PROPERTIES, 0, 0 };

    if (test_on_device)
    {
        queue_props.insert(queue_props.end(), &device_required[0],
                           &device_required[device_required_size]);
    };

    for (cl_queue_properties props : queue_props)
    {

        queue_props_arg[1] = props;

        clCommandQueueWrapper queue;
        if (is_compatibility)
        {
            queue = clCreateCommandQueue(context, device, props, &error);
            test_error(error, "Unable to create command queue to test with");
        }
        else
        {
            queue = clCreateCommandQueueWithProperties(context, device,
                                                       queue_props_arg, &error);
            test_error(error, "Unable to create command queue to test with");
        }

        cl_uint refCount;
        size_t size;
        error = clGetCommandQueueInfo(queue, CL_QUEUE_REFERENCE_COUNT,
                                      sizeof(refCount), &refCount, &size);
        test_error(error, "Unable to get command queue reference count");
        test_assert_error(size == sizeof(refCount),
                          "Returned size of command queue reference count does "
                          "not validate!\n");

        error = command_queue_param_test(queue, CL_QUEUE_CONTEXT, context,
                                         "context");
        test_error(error, "param checking failed");

        error =
            command_queue_param_test(queue, CL_QUEUE_DEVICE, device, "device");
        test_error(error, "param checking failed");

        error = command_queue_param_test(queue, CL_QUEUE_PROPERTIES,
                                         queue_props_arg[1], "properties");
        test_error(error, "param checking failed");
    }
    return 0;
}

REGISTER_TEST_VERSION(get_command_queue_info, Version(2, 0))
{
    int error = check_get_command_queue_info_params(device, context, false);
    test_error(error, "Test Failed");
    return 0;
}

REGISTER_TEST(get_command_queue_info_compatibility)
{
    int error = check_get_command_queue_info_params(device, context, true);
    test_error(error, "Test Failed");
    return 0;
}

REGISTER_TEST(get_context_info)
{
    int error;

    // query single device context and perform object comparability test
    cl_uint num_devices = 0;
    error = clGetContextInfo(context, CL_CONTEXT_NUM_DEVICES, sizeof(cl_uint),
                             &num_devices, nullptr);
    test_error(error, "clGetContextInfo CL_CONTEXT_NUM_DEVICES failed\n");

    test_assert_error(num_devices == 1,
                      "Context must contain exactly one device\n");

    cl_device_id comp_device = nullptr;
    error = clGetContextInfo(context, CL_CONTEXT_DEVICES, sizeof(cl_device_id),
                             &comp_device, nullptr);
    test_error(error, "clGetContextInfo CL_CONTEXT_DEVICES failed\n");

    test_assert_error(device == comp_device,
                      "Unexpected result returned by CL_CONTEXT_DEVICES query");

    // query context properties against valid size
    size_t size;
    cl_context_properties props;
    error = clGetContextInfo(context, CL_CONTEXT_PROPERTIES, sizeof(props),
                             &props, &size);
    test_error(error, "Unable to get context props");

    if (size == 0)
    {
        // Valid size
        return 0;
    }
    else if (size == sizeof(cl_context_properties))
    {
        // Data must be NULL
        if (props != 0)
        {
            log_error("ERROR: Returned properties is no NULL.\n");
            return -1;
        }
        // Valid data and size
        return 0;
    }
    // Size was not 0 or 1
    log_error("ERROR: Returned size of context props is not valid! (expected 0 "
              "or %d, got %d)\n",
              (int)sizeof(cl_context_properties), (int)size);
    return -1;
}

REGISTER_TEST(get_context_info_mult_devices)
{
    cl_int err = CL_SUCCESS;
    size_t size = 0;

    // query multi-device context and perform objects comparability test
    err = clGetDeviceInfo(device, CL_DEVICE_PARTITION_PROPERTIES, 0, nullptr,
                          &size);
    test_error_fail(err, "clGetDeviceInfo failed");

    if (size == 0)
    {
        log_info("Can't partition device, test not supported\n");
        return TEST_SKIPPED_ITSELF;
    }

    std::vector<cl_device_partition_property> supported_props(
        size / sizeof(cl_device_partition_property), 0);
    err = clGetDeviceInfo(device, CL_DEVICE_PARTITION_PROPERTIES,
                          supported_props.size()
                              * sizeof(cl_device_partition_property),
                          supported_props.data(), &size);
    test_error_fail(err, "clGetDeviceInfo failed");

    if (supported_props.empty() || supported_props.front() == 0)
    {
        log_info("Can't partition device, test not supported\n");
        return TEST_SKIPPED_ITSELF;
    }

    cl_uint maxComputeUnits = 0;
    err = clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS,
                          sizeof(maxComputeUnits), &maxComputeUnits, NULL);
    test_error_fail(err, "Unable to get maximal number of compute units");

    std::vector<std::array<cl_device_partition_property, 5>> partition_props = {
        { CL_DEVICE_PARTITION_EQUALLY, (cl_int)maxComputeUnits / 2, 0, 0, 0 },
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
        NULL, (cl_uint)num_devices, scope_guard->sub_devices.data(), nullptr,
        nullptr, &err);
    test_error_fail(err, "Unable to create testing context");

    err = clGetContextInfo(multi_device_context, CL_CONTEXT_NUM_DEVICES,
                           sizeof(cl_uint), &num_devices, nullptr);
    test_error_fail(err, "clGetContextInfo CL_CONTEXT_NUM_DEVICES failed\n");

    test_assert_error(num_devices == scope_guard->sub_devices.size(),
                      "Context must contain exact number of devices\n");

    std::vector<cl_device_id> devices(num_devices);
    err = clGetContextInfo(multi_device_context, CL_CONTEXT_DEVICES,
                           num_devices * sizeof(cl_device_id), devices.data(),
                           nullptr);
    test_error_fail(err, "clGetContextInfo CL_CONTEXT_DEVICES failed\n");

    test_assert_error(devices.size() == scope_guard->sub_devices.size(),
                      "Size of devices arrays must be in sync\n");

    for (cl_uint i = 0; i < devices.size(); i++)
    {
        bool found = false;
        for (auto &it : scope_guard->sub_devices)
        {
            if (it == devices[i])
            {
                found = true;
                break;
            }
        }
        test_error_fail(
            !found, "Unexpected result returned by CL_CONTEXT_DEVICES query");
    }
    return TEST_PASS;
}

void CL_CALLBACK mem_obj_destructor_callback( cl_mem, void *data )
{
    free( data );
}

#define TEST_DEVICE_PARAM( device, paramName, val, name, type, cast )    \
error = clGetDeviceInfo( device, paramName, sizeof( val ), &val, &size );        \
test_error( error, "Unable to get device " name );                            \
if( size != sizeof( val ) )                \
{                                        \
log_error( "ERROR: Returned size of device " name " does not validate! (expected %d, got %d)\n", (int)sizeof( val ), (int)size );    \
return -1;    \
}                \
log_info( "\tReported device " name " : " type "\n", (cast)val );

#define TEST_DEVICE_PARAM_MEM( device, paramName, val, name, type, div )    \
error = clGetDeviceInfo( device, paramName, sizeof( val ), &val, &size );        \
test_error( error, "Unable to get device " name );                            \
if( size != sizeof( val ) )                \
{                                        \
log_error( "ERROR: Returned size of device " name " does not validate! (expected %d, got %d)\n", (int)sizeof( val ), (int)size );    \
return -1;    \
}                \
log_info( "\tReported device " name " : " type "\n", (int)( val / div ) );

REGISTER_TEST(get_device_info)
{
    int error;
    size_t size;

    cl_uint vendorID;
    TEST_DEVICE_PARAM(device, CL_DEVICE_VENDOR_ID, vendorID, "vendor ID",
                      "0x%08x", int)

    char extensions[ 10240 ];
    error = clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, sizeof(extensions),
                            &extensions, &size);
    test_error( error, "Unable to get device extensions" );
    if( size != strlen( extensions ) + 1 )
    {
        log_error( "ERROR: Returned size of device extensions does not validate! (expected %d, got %d)\n", (int)( strlen( extensions ) + 1 ), (int)size );
        return -1;
    }
    log_info( "\tReported device extensions: %s \n", extensions );

    cl_uint preferred;
    TEST_DEVICE_PARAM(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR, preferred,
                      "preferred vector char width", "%d", int)
    TEST_DEVICE_PARAM(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT, preferred,
                      "preferred vector short width", "%d", int)
    TEST_DEVICE_PARAM(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT, preferred,
                      "preferred vector int width", "%d", int)
    TEST_DEVICE_PARAM(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG, preferred,
                      "preferred vector long width", "%d", int)
    TEST_DEVICE_PARAM(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT, preferred,
                      "preferred vector float width", "%d", int)
    TEST_DEVICE_PARAM(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE,
                      preferred, "preferred vector double width", "%d", int)

    // Note that even if cl_khr_fp64, the preferred width for double can be non-zero.  For example, vendors
    // extensions can support double but may not support cl_khr_fp64, which implies math library support.

    cl_uint baseAddrAlign;
    TEST_DEVICE_PARAM(device, CL_DEVICE_MEM_BASE_ADDR_ALIGN, baseAddrAlign,
                      "base address alignment", "%d bits", int)

    cl_uint maxDataAlign;
    TEST_DEVICE_PARAM(device, CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE, maxDataAlign,
                      "min data type alignment", "%d bytes", int)

    cl_device_mem_cache_type cacheType;
    error = clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_CACHE_TYPE,
                            sizeof(cacheType), &cacheType, &size);
    test_error( error, "Unable to get device global mem cache type" );
    if( size != sizeof( cacheType ) )
    {
        log_error( "ERROR: Returned size of device global mem cache type does not validate! (expected %d, got %d)\n", (int)sizeof( cacheType ), (int)size );
        return -1;
    }
    const char *cacheTypeName = ( cacheType == CL_NONE ) ? "CL_NONE" : ( cacheType == CL_READ_ONLY_CACHE ) ? "CL_READ_ONLY_CACHE" : ( cacheType == CL_READ_WRITE_CACHE ) ? "CL_READ_WRITE_CACHE" : "<unknown>";
    log_info( "\tReported device global mem cache type: %s \n", cacheTypeName );

    cl_uint cachelineSize;
    TEST_DEVICE_PARAM(device, CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE,
                      cachelineSize, "global mem cacheline size", "%d bytes",
                      int)

    cl_ulong cacheSize;
    TEST_DEVICE_PARAM_MEM(device, CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, cacheSize,
                          "global mem cache size", "%d KB", 1024)

    cl_ulong memSize;
    TEST_DEVICE_PARAM_MEM(device, CL_DEVICE_GLOBAL_MEM_SIZE, memSize,
                          "global mem size", "%d MB", (1024 * 1024))

    cl_device_local_mem_type localMemType;
    error = clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_TYPE,
                            sizeof(localMemType), &localMemType, &size);
    test_error( error, "Unable to get device local mem type" );
    if( size != sizeof( cacheType ) )
    {
        log_error( "ERROR: Returned size of device local mem type does not validate! (expected %d, got %d)\n", (int)sizeof( localMemType ), (int)size );
        return -1;
    }
    const char *localMemTypeName = ( localMemType == CL_LOCAL ) ? "CL_LOCAL" : ( cacheType == CL_GLOBAL ) ? "CL_GLOBAL" : "<unknown>";
    log_info( "\tReported device local mem type: %s \n", localMemTypeName );


    cl_bool errSupport;
    TEST_DEVICE_PARAM(device, CL_DEVICE_ERROR_CORRECTION_SUPPORT, errSupport,
                      "error correction support", "%d", int)

    size_t timerResolution;
    TEST_DEVICE_PARAM(device, CL_DEVICE_PROFILING_TIMER_RESOLUTION,
                      timerResolution, "profiling timer resolution",
                      "%ld nanoseconds", long)

    cl_bool endian;
    TEST_DEVICE_PARAM(device, CL_DEVICE_ENDIAN_LITTLE, endian,
                      "little endian flag", "%d", int)

    cl_bool avail;
    TEST_DEVICE_PARAM(device, CL_DEVICE_AVAILABLE, avail, "available flag",
                      "%d", int)

    cl_bool compilerAvail;
    TEST_DEVICE_PARAM(device, CL_DEVICE_COMPILER_AVAILABLE, compilerAvail,
                      "compiler available flag", "%d", int)

    char profile[ 1024 ];
    error = clGetDeviceInfo(device, CL_DEVICE_PROFILE, sizeof(profile),
                            &profile, &size);
    test_error( error, "Unable to get device profile" );
    if( size != strlen( profile ) + 1 )
    {
        log_error( "ERROR: Returned size of device profile does not validate! (expected %d, got %d)\n", (int)( strlen( profile ) + 1 ), (int)size );
        return -1;
    }
    if( strcmp( profile, "FULL_PROFILE" ) != 0 && strcmp( profile, "EMBEDDED_PROFILE" ) != 0 )
    {
        log_error( "ERROR: Returned profile of device not FULL or EMBEDDED as required by OpenCL 1.2! (Returned %s)\n", profile );
        return -1;
    }
    log_info( "\tReported device profile: %s \n", profile );

    if (strcmp(profile, "FULL_PROFILE") == 0 && compilerAvail != CL_TRUE)
    {
        log_error("ERROR: Returned profile of device is FULL , but "
                  "CL_DEVICE_COMPILER_AVAILABLE is not CL_TRUE as required by "
                  "OpenCL 1.2!");
        return -1;
    }

    return 0;
}

REGISTER_TEST(get_device_info_comparability)
{
    int error = CL_SUCCESS;

    // comparability test for CL_DEVICE_PLATFORM
    // 1. find platform related to device without using query
    cl_uint num_platforms = 0;
    error = clGetPlatformIDs(16, nullptr, &num_platforms);
    test_error(error, "clGetPlatformIDs failed");

    std::vector<cl_platform_id> platforms(num_platforms);

    error = clGetPlatformIDs(num_platforms, platforms.data(), &num_platforms);
    test_error(error, "clGetPlatformIDs failed");

    cl_uint num_devices = 0;
    cl_platform_id comp_platform = nullptr;
    for (int p = 0; p < (int)num_platforms && comp_platform == nullptr; p++)
    {
        error = clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_ALL, 0, nullptr,
                               &num_devices);
        if (error != CL_SUCCESS || num_devices == 0) continue;

        std::vector<cl_device_id> devices(num_devices);
        error = clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_ALL, num_devices,
                               devices.data(), nullptr);
        if (error != CL_SUCCESS) continue;

        // find correct device
        for (auto did : devices)
        {
            if (did == device)
            {
                comp_platform = platforms[p];
                break;
            }
        }
    }

    test_error_fail(comp_platform == nullptr,
                    "Test failed to locate platform for comparison!");

    // 2. compare platforms found with and without using query
    cl_platform_id plat = nullptr;
    error =
        clGetDeviceInfo(device, CL_DEVICE_PLATFORM, sizeof(plat), &plat, NULL);
    test_error(error, "clGetDeviceInfo failed");

    test_assert_error(plat == comp_platform,
                      "Unexpected result returned by clGetDeviceInfo for "
                      "CL_DEVICE_PLATFORM query");

    return TEST_PASS;
}

static const char *sample_compile_size[2] = {
    "__kernel void sample_test(__global int *src, __global int *dst)\n"
    "{\n"
    "    int  tid = get_global_id(0);\n"
    "     dst[tid] = src[tid];\n"
    "\n"
    "}\n",
    "__kernel __attribute__((reqd_work_group_size(%d,%d,%d))) void sample_test(__global int *src, __global int *dst)\n"
    "{\n"
    "    int  tid = get_global_id(0);\n"
    "     dst[tid] = src[tid];\n"
    "\n"
    "}\n" };

REGISTER_TEST(kernel_required_group_size)
{
    int error;
    size_t realSize;
    size_t kernel_max_workgroup_size;
    size_t global[] = {64,14,10};
    size_t local[] = {0,0,0};

    cl_uint max_dimensions;

    error = clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,
                            sizeof(max_dimensions), &max_dimensions, NULL);
    test_error(error,  "clGetDeviceInfo failed for CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS");
    log_info("Device reported CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS = %d.\n", (int)max_dimensions);

    {
        clProgramWrapper program;
        clKernelWrapper kernel;

        error = create_single_kernel_helper( context, &program, &kernel, 1, &sample_compile_size[ 0 ], "sample_test" );
        if( error != 0 )
            return error;

        error =
            clGetKernelWorkGroupInfo(kernel, device, CL_KERNEL_WORK_GROUP_SIZE,
                                     sizeof(kernel_max_workgroup_size),
                                     &kernel_max_workgroup_size, NULL);
        test_error( error, "clGetKernelWorkGroupInfo failed for CL_KERNEL_WORK_GROUP_SIZE");
        log_info("The CL_KERNEL_WORK_GROUP_SIZE for the kernel is %d.\n", (int)kernel_max_workgroup_size);

        size_t size[ 3 ];
        error = clGetKernelWorkGroupInfo(kernel, device,
                                         CL_KERNEL_COMPILE_WORK_GROUP_SIZE,
                                         sizeof(size), size, &realSize);
        test_error( error, "Unable to get work group info" );

        if( size[ 0 ] != 0 || size[ 1 ] != 0 || size[ 2 ] != 0 )
        {
            log_error( "ERROR: Nonzero compile work group size returned for nonspecified size! (returned %d,%d,%d)\n", (int)size[0], (int)size[1], (int)size[2] );
            return -1;
        }

        if( realSize != sizeof( size ) )
        {
            log_error( "ERROR: Returned size of compile work group size not valid! (Expected %d, got %d)\n", (int)sizeof( size ), (int)realSize );
            return -1;
        }

        // Determine some local dimensions to use for the test.
        if (max_dimensions == 1) {
            error = get_max_common_work_group_size(context, kernel, global[0], &local[0]);
            test_error( error, "get_max_common_work_group_size failed");
            log_info("For global dimension %d, kernel will require local dimension %d.\n", (int)global[0], (int)local[0]);
        } else if (max_dimensions == 2) {
            error = get_max_common_2D_work_group_size(context, kernel, global, local);
            test_error( error, "get_max_common_2D_work_group_size failed");
            log_info("For global dimension %d x %d, kernel will require local dimension %d x %d.\n", (int)global[0], (int)global[1], (int)local[0], (int)local[1]);
        } else {
            error = get_max_common_3D_work_group_size(context, kernel, global, local);
            test_error( error, "get_max_common_3D_work_group_size failed");
            log_info("For global dimension %d x %d x %d, kernel will require local dimension %d x %d x %d.\n",
                     (int)global[0], (int)global[1], (int)global[2], (int)local[0], (int)local[1], (int)local[2]);
        }
    }


    {
        clProgramWrapper program;
        clKernelWrapper kernel;
        clMemWrapper in, out;
        //char source[1024];
        char *source = (char*)malloc(1024);
        source[0] = '\0';

        sprintf(source, sample_compile_size[1], local[0], local[1], local[2]);

        error = create_single_kernel_helper( context, &program, &kernel, 1, (const char**)&source, "sample_test" );
        if( error != 0 )
            return error;

        size_t size[ 3 ];
        error = clGetKernelWorkGroupInfo(kernel, device,
                                         CL_KERNEL_COMPILE_WORK_GROUP_SIZE,
                                         sizeof(size), size, &realSize);
        test_error( error, "Unable to get work group info" );

        if( size[ 0 ] != local[0] || size[ 1 ] != local[1] || size[ 2 ] != local[2] )
        {
            log_error( "ERROR: Incorrect compile work group size returned for specified size! (returned %d,%d,%d, expected %d,%d,%d)\n",
                      (int)size[0], (int)size[1], (int)size[2], (int)local[0], (int)local[1], (int)local[2]);
            return -1;
        }

        // Verify that the kernel will only execute with that size.
        in = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(cl_int)*global[0], NULL, &error);
        test_error(error, "clCreateBuffer failed");
        out = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(cl_int)*global[0], NULL, &error);
        test_error(error, "clCreateBuffer failed");

        error = clSetKernelArg(kernel, 0, sizeof(in), &in);
        test_error(error, "clSetKernelArg failed");
        error = clSetKernelArg(kernel, 1, sizeof(out), &out);
        test_error(error, "clSetKernelArg failed");

        error = clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global, local, 0, NULL, NULL);
        test_error(error, "clEnqueueNDRangeKernel failed");

        error = clFinish(queue);
        test_error(error, "clFinish failed");

        log_info("kernel_required_group_size may report spurious ERRORS in the conformance log.\n");

        local[0]++;
        error = clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global, local, 0, NULL, NULL);
        if (error != CL_INVALID_WORK_GROUP_SIZE) {
            log_error("Incorrect error returned for executing a kernel with the wrong required local work group size. (used %d,%d,%d, required %d,%d,%d)\n",
                      (int)local[0], (int)local[1], (int)local[2], (int)local[0]-1, (int)local[1], (int)local[2] );
            print_error(error, "Expected: CL_INVALID_WORK_GROUP_SIZE.");
            return -1;
        }

        error = clFinish(queue);
        test_error(error, "clFinish failed");

        if (max_dimensions == 1) {
            free(source);
            return 0;
        }

        local[0]--; local[1]++;
        error = clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global, local, 0, NULL, NULL);
        if (error != CL_INVALID_WORK_GROUP_SIZE) {
            log_error("Incorrect error returned for executing a kernel with the wrong required local work group size. (used %d,%d,%d, required %d,%d,%d)\n",
                      (int)local[0], (int)local[1], (int)local[2], (int)local[0]-1, (int)local[1], (int)local[2]);
            print_error(error, "Expected: CL_INVALID_WORK_GROUP_SIZE.");
            return -1;
        }

        error = clFinish(queue);
        test_error(error, "clFinish failed");

        if (max_dimensions == 2) {
            free(source);
            return 0;
        }

        local[1]--; local[2]++;
        error = clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global, local, 0, NULL, NULL);
        if (error != CL_INVALID_WORK_GROUP_SIZE) {
            log_error("Incorrect error returned for executing a kernel with the wrong required local work group size. (used %d,%d,%d, required %d,%d,%d)\n",
                      (int)local[0], (int)local[1], (int)local[2], (int)local[0]-1, (int)local[1], (int)local[2]);
            print_error(error, "Expected: CL_INVALID_WORK_GROUP_SIZE.");
            return -1;
        }

        error = clFinish(queue);
        test_error(error, "clFinish failed");
        free(source);
    }

    return 0;
}
