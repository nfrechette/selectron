// OpenCL Selectron prototype
//
// Patrick Walton <pcwalton@mozilla.com>
//
// Copyright (c) 2014 Mozilla Corporation

#include "selectron.h"

#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <time.h>
#include <string.h>

#ifndef __APPLE__
#include <windows.h>
#include <CL/opencl.h>
#else
#include <mach/mach_time.h>
#include <OpenCL/OpenCL.h>
#endif

#if HAVE_OPENCL_SVM
#else
#define NO_SVM
#endif

// Hack to allow stringification of macro expansion

#define XSTRINGIFY(s)   STRINGIFY(s)
#define STRINGIFY(s)    #s

#ifndef __APPLE__
uint64_t mach_absolute_time() {
    static LARGE_INTEGER freq = { 0, 0 };
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);

    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return time.QuadPart * 1000000000 / freq.QuadPart;
}
#endif

const char *selector_matching_kernel_source = "\n"
    "#define cl_int int\n"
    "#define cl_ulong ulong\n"
    XSTRINGIFY(STRUCT_CSS_PROPERTY) ";\n"
    XSTRINGIFY(STRUCT_CSS_RULE) ";\n"
    XSTRINGIFY(STRUCT_CSS_CUCKOO_HASH) ";\n"
    XSTRINGIFY(STRUCT_CSS_MATCHED_PROPERTY) ";\n"
    XSTRINGIFY(STRUCT_CSS_STYLESHEET_SOURCE) ";\n"
    XSTRINGIFY(STRUCT_CSS_STYLESHEET) ";\n"
    XSTRINGIFY(STRUCT_DOM_NODE) ";\n"
    "static unsigned int css_rule_hash(unsigned int key, unsigned int seed) {\n"
    "   " XSTRINGIFY(CSS_RULE_HASH(key, seed)) ";\n"
    "}\n"
    "\n"
    "static __global const struct css_rule *css_cuckoo_hash_find(__global const struct css_cuckoo_hash *hash,\n"
    "                                               int key,\n"
    "                                               int left_index,\n"
    "                                               int right_index) {\n"
    "   " XSTRINGIFY(CSS_CUCKOO_HASH_FIND(hash, key, left_index, right_index)) ";\n"
    "}\n"
    "\n"
    "static void sort_selectors(__local struct css_matched_property *matched_properties, int length) {\n"
    "   " XSTRINGIFY(SORT_SELECTORS(matched_properties, length)) ";\n"
    "}\n"
    "\n"
    "__kernel void match_selectors(__global struct dom_node *first, \n"
    "                              __global struct css_stylesheet *stylesheet, \n"
    "                              __global struct css_property *properties, \n"
    "                              __global int *classes) {\n"
    "   int index = get_global_id(0);\n"
    "   " XSTRINGIFY(MATCH_SELECTORS(first,
                                     stylesheet,
                                     properties,
                                     classes,
                                     index,
                                     css_cuckoo_hash_find,
                                     css_rule_hash,
                                     sort_selectors,
                                     __global)) ";\n"
    "}\n";

#define CHECK_CL(call) \
    do { \
        int _err = call; \
        if (_err != CL_SUCCESS) { \
            fprintf(stderr, \
                    "OpenCL call failed (%s, %d): %d, %s\n", \
                    __FILE__, \
                    __LINE__, \
                    _err, \
                    #call); \
            abort(); \
        } \
    } while(0)

void abort_unless(int error) {
    if (!error) {
        fprintf(stderr, "OpenCL error");
    }
}

void abort_if_null(void *ptr, const char *msg = "") {
    if (!ptr) {
        fprintf(stderr, "OpenCL error: %s\n", msg);
    }
}

#define FIND_EXTENSION(name, platform) \
    static name##_fn name = NULL; \
    do { \
        if (!name) { \
            name = (name##_fn)clGetExtensionFunctionAddressForPlatform(platform, #name); \
            abort_if_null(name, "couldn't find extension " #name); \
        } \
    } while(0)

#define MALLOC(context, commands, err, mode, name, perm, type) \
    do { \
        ctx.device_##name = clCreateBuffer(context, perm, inctx->size_##name, NULL, NULL); \
        abort_if_null(ctx.device_##name, "create buffer failed " #name); \
        if ((mode) == MODE_MAPPED) { \
        } else { \
            ctx.host_##name = (type *)malloc(inctx->size_##name); \
        } \
        /*fprintf(stderr, "mapped " #name " to %p\n", ctx.host_##name);*/ \
    } while(0)

void dump_dom(struct dom_node *node, cl_int *classes, const char* path) {
    FILE *f = fopen(path, "w");

    for (int i = 0; i < 20; i++) {
        fprintf(f, "%d (id %d; tag %d; classes", i, node[i].id, node[i].tag_name);
        for (int j = 0; j < node[i].class_count; j++) {
            fprintf(f, "%s%d", j == 0 ? " " : ", ", (int)classes[node[i].first_class + j]);
        }
        fprintf(f, ") -> ");
        for (int j = 0; j < MAX_STYLE_PROPERTIES; j++) {
            if (node[i].style[j] != 0)
                fprintf(f, "%d=%d ", j, node[i].style[j]);
        }
        fprintf(f, "\n");
    }

    fclose(f); f = NULL;
}

struct input_context
{
    struct css_stylesheet *stylesheet;
    struct css_property *properties;
    struct dom_node *dom;
    cl_int *classes;

    int size_dom;
    int size_stylesheet;
    int size_properties;
    int size_classes;
};

struct kernel_context
{
    struct css_stylesheet *host_stylesheet;
    struct css_property *host_properties;
    struct dom_node *host_dom;
    cl_int *host_classes;

    cl_mem device_dom;
    cl_mem device_stylesheet;
    cl_mem device_properties;
    cl_mem device_classes;
};

void init_mem(cl_command_queue commands, const struct input_context* inctx, struct kernel_context* ctx, const char* device_name, int mode, cl_device_type device_type)
{
    uint64_t start = mach_absolute_time();

    if (mode == MODE_MAPPED) {
        cl_int err;
        ctx->host_stylesheet = (struct css_stylesheet *)clEnqueueMapBuffer(commands,
                                                 ctx->device_stylesheet,
                                                 CL_TRUE,
                                                 CL_MAP_READ | CL_MAP_WRITE,
                                                 0,
                                                 inctx->size_stylesheet,
                                                 0,
                                                 NULL,
                                                 NULL,
                                                 &err);
        CHECK_CL(err);

        ctx->host_properties = (struct css_property *)clEnqueueMapBuffer(commands,
                                                 ctx->device_properties,
                                                 CL_TRUE,
                                                 CL_MAP_READ | CL_MAP_WRITE,
                                                 0,
                                                 inctx->size_properties,
                                                 0,
                                                 NULL,
                                                 NULL,
                                                 &err);
        CHECK_CL(err);

        ctx->host_dom = (struct dom_node *)clEnqueueMapBuffer(commands,
                                                 ctx->device_dom,
                                                 CL_TRUE,
                                                 CL_MAP_READ | CL_MAP_WRITE,
                                                 0,
                                                 inctx->size_dom,
                                                 0,
                                                 NULL,
                                                 NULL,
                                                 &err);
        CHECK_CL(err);

        ctx->host_classes = (int *)clEnqueueMapBuffer(commands,
                                                 ctx->device_classes,
                                                 CL_TRUE,
                                                 CL_MAP_READ | CL_MAP_WRITE,
                                                 0,
                                                 inctx->size_classes,
                                                 0,
                                                 NULL,
                                                 NULL,
                                                 &err);
        CHECK_CL(err);
    }

    memcpy(ctx->host_stylesheet, inctx->stylesheet, inctx->size_stylesheet);
    memcpy(ctx->host_properties, inctx->properties, inctx->size_properties);
    memcpy(ctx->host_dom, inctx->dom, inctx->size_dom);
    memcpy(ctx->host_classes, inctx->classes, inctx->size_classes);

    // Unmap or copy buffers if necessary.
    switch (mode) {
    case MODE_MAPPED:
        CHECK_CL(clEnqueueUnmapMemObject(commands,
                                         ctx->device_stylesheet,
                                         ctx->host_stylesheet,
                                         0,
                                         NULL,
                                         NULL));
        ctx->host_stylesheet = NULL;

        CHECK_CL(clEnqueueUnmapMemObject(commands,
                                         ctx->device_properties,
                                         ctx->host_properties,
                                         0,
                                         NULL,
                                         NULL));
        ctx->host_properties = NULL;

        CHECK_CL(clEnqueueUnmapMemObject(commands,
                                         ctx->device_dom,
                                         ctx->host_dom,
                                         0,
                                         NULL,
                                         NULL));
        ctx->host_dom = NULL;

        CHECK_CL(clEnqueueUnmapMemObject(commands,
                                         ctx->device_classes,
                                         ctx->host_classes,
                                         0,
                                         NULL,
                                         NULL));
        ctx->host_classes = NULL;
        break;
    case MODE_COPYING:
        CHECK_CL(clEnqueueWriteBuffer(commands,
                                      ctx->device_stylesheet,
                                      CL_TRUE,
                                      0,
                                      inctx->size_stylesheet,
                                      ctx->host_stylesheet,
                                      0,
                                      NULL,
                                      NULL));
        CHECK_CL(clEnqueueWriteBuffer(commands,
                                      ctx->device_properties,
                                      CL_TRUE,
                                      0,
                                      inctx->size_properties,
                                      ctx->host_properties,
                                      0,
                                      NULL,
                                      NULL));
        CHECK_CL(clEnqueueWriteBuffer(commands,
                                      ctx->device_dom,
                                      CL_TRUE,
                                      0,
                                      inctx->size_dom,
                                      ctx->host_dom,
                                      0,
                                      NULL,
                                      NULL));
        CHECK_CL(clEnqueueWriteBuffer(commands,
                                      ctx->device_classes,
                                      CL_TRUE,
                                      0,
                                      inctx->size_classes,
                                      ctx->host_classes,
                                      0,
                                      NULL,
                                      NULL));
    }

    clFinish(commands);

    double elapsed = (double)(mach_absolute_time() - start) / 1000000.0;
    report_timing(device_name, "buffer copying", elapsed, false, mode);
}

void print_device_info(cl_device_id device_id)
{
    size_t max_workgroup_size;
    CHECK_CL(clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &max_workgroup_size, NULL));
    fprintf(stderr, "max workgroup size: %d\n", (int)max_workgroup_size);

    cl_uint num_compute_units;
    CHECK_CL(clGetDeviceInfo(device_id, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &num_compute_units, NULL));
    fprintf(stderr, "num compute units: %d\n", (int)num_compute_units);

    cl_ulong local_mem_size;
    CHECK_CL(clGetDeviceInfo(device_id, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &local_mem_size, NULL));
    fprintf(stderr, "local mem size: %d\n", (int)local_mem_size);

    cl_ulong global_mem_size;
    CHECK_CL(clGetDeviceInfo(device_id, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong), &global_mem_size, NULL));
    fprintf(stderr, "global mem size: %d\n", (int)global_mem_size);

    cl_bool is_mem_unified;
    CHECK_CL(clGetDeviceInfo(device_id, CL_DEVICE_HOST_UNIFIED_MEMORY, sizeof(cl_bool), &is_mem_unified, NULL));
    fprintf(stderr, "is memory unified: %s\n", is_mem_unified ? "true" : "false");

    cl_uint cache_line_size;
    CHECK_CL(clGetDeviceInfo(device_id, CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE, sizeof(cl_uint), &cache_line_size, NULL));
    fprintf(stderr, "cache line size: %d\n", (int)cache_line_size);

    cl_ulong global_cache_size;
    CHECK_CL(clGetDeviceInfo(device_id, CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, sizeof(cl_ulong), &global_cache_size, NULL));
    fprintf(stderr, "global cache size: %d\n", (int)global_cache_size);

    cl_uint address_space_size;
    CHECK_CL(clGetDeviceInfo(device_id, CL_DEVICE_ADDRESS_BITS, sizeof(cl_uint), &address_space_size, NULL));
    fprintf(stderr, "address space size: %d\n", (int)address_space_size);
}

void go(cl_platform_id platform, cl_device_type device_type, int mode, const struct input_context* inctx) {
#ifndef NO_SVM
    FIND_EXTENSION(clSVMAllocAMD, platform);
    FIND_EXTENSION(clSVMFreeAMD, platform);
    FIND_EXTENSION(clSetKernelArgSVMPointerAMD, platform);
#endif

    // Perform OpenCL initialization.
    cl_device_id device_id;
    CHECK_CL(clGetDeviceIDs(platform, device_type, 1, &device_id, NULL));
    size_t device_name_size;
    CHECK_CL(clGetDeviceInfo(device_id, CL_DEVICE_NAME, 0, NULL, &device_name_size));
    char *device_name = (char *)malloc(device_name_size);
    CHECK_CL(clGetDeviceInfo(device_id, CL_DEVICE_NAME, device_name_size, device_name, NULL));
    fprintf(stderr, "device found: %s\n", device_name);

    print_device_info(device_id);

    cl_context_properties props[6] = {
        CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
#ifndef NO_SVM
        CL_HSA_ENABLED_AMD, (cl_context_properties)1,
#endif
        0, 0
    };

    cl_int err;
    cl_context context = clCreateContextFromType(props, device_type, NULL, NULL, &err);
    CHECK_CL(err);
    abort_if_null(context, "create context failed");

    cl_command_queue commands = clCreateCommandQueue(context, device_id, 0, &err);
    abort_if_null(commands, "create command queue failed");

    cl_program program = clCreateProgramWithSource(context,
                                                   1,
                                                   (const char **)&selector_matching_kernel_source,
                                                   NULL,
                                                   &err);
    CHECK_CL(err);

    err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        cl_build_status build_status;
        CHECK_CL(clGetProgramBuildInfo(program,
                                       device_id,
                                       CL_PROGRAM_BUILD_STATUS,
                                       sizeof(cl_build_status),
                                       &build_status,
                                       NULL));
        fprintf(stderr, "Build status: %d\n", (int)build_status);

        size_t log_size;
        CHECK_CL(clGetProgramBuildInfo(program,
                                       device_id,
                                       CL_PROGRAM_BUILD_LOG,
                                       0,
                                       NULL,
                                       &log_size));

        char *log = (char *)malloc(log_size);
        CHECK_CL(clGetProgramBuildInfo(program,
                                       device_id,
                                       CL_PROGRAM_BUILD_LOG,
                                       log_size,
                                       log,
                                       NULL));
        fprintf(stderr, "Compilation error: %s\n", log);
        exit(1);
    }

#if 0
    size_t binary_sizes_sizes;
    CHECK_CL(clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, 0, NULL, &binary_sizes_sizes));
    size_t *binary_sizes = (size_t *)malloc(sizeof(size_t) * binary_sizes_sizes);
    CHECK_CL(clGetProgramInfo(program,
                              CL_PROGRAM_BINARY_SIZES,
                              binary_sizes_sizes,
                              binary_sizes,
                              NULL));
    char **binaries = (char **)malloc(binary_sizes_sizes / sizeof(size_t));
    for (int i = 0; i < binary_sizes_sizes / sizeof(size_t); i++)
        binaries[i] = (char *)malloc(binary_sizes[i]);
    CHECK_CL(clGetProgramInfo(program, CL_PROGRAM_BINARIES, binary_sizes_sizes, binaries, NULL));
    for (int i = 0; i < binary_sizes_sizes / sizeof(size_t); i++) {
        char *path = (char *)malloc(32);
        sprintf(path, "prg%c%02d.plist", (device_type == CL_DEVICE_TYPE_CPU) ? 'c' : 'g', i);
        FILE *f = fopen(path, "w");
        fwrite(binaries[i], binary_sizes[i], 1, f);
        fclose(f);
        free(path);
    }
#endif

    cl_kernel kernel = clCreateKernel(program, "match_selectors", &err);
    CHECK_CL(err);

    struct kernel_context ctx = {0};

    if (mode != MODE_SVM) {
        MALLOC(context,
               commands,
               err,
               mode,
               dom,
               CL_MEM_READ_WRITE,
               struct dom_node);
        MALLOC(context,
               commands,
               err,
               mode,
               stylesheet,
               CL_MEM_READ_ONLY,
               struct css_stylesheet);
        MALLOC(context,
               commands,
               err,
               mode,
               properties,
               CL_MEM_READ_ONLY,
               struct css_property);
        MALLOC(context,
               commands,
               err,
               mode,
               classes,
               CL_MEM_READ_ONLY,
               cl_int);
    } else {
#ifndef NO_SVM
        // Allocate the rule tree.
        ctx.host_stylesheet = (struct css_stylesheet *)clSVMAllocAMD(context,
                                                                 0,
                                                                 sizeof(struct css_stylesheet),
                                                                 16);
        abort_if_null(host_stylesheet, "failed to allocate host stylesheet");
        ctx.host_properties = (struct css_property *)clSVMAllocAMD(
            context,
            0,
            sizeof(struct css_property) * PROPERTY_COUNT,
            16);
        abort_if_null(host_properties, "failed to allocate host properties");

        // Allocate the DOM tree.
        ctx.host_dom = (struct dom_node *)clSVMAllocAMD(context,
                                                    0,
                                                    sizeof(struct dom_node) * NODE_COUNT,
                                                    16);
        abort_if_null(host_dom, "failed to allocate host DOM");

        // Allocate the classes.
        ctx.host_classes = (struct dom_node *)clSVMAllocAMD(context,
                                                        0,
                                                        sizeof(int) * CLASS_COUNT,
                                                        16);
        abort_if_null(host_dom, "failed to allocate host classes");
#endif
    }

    uint64_t start = 0;

    // Figure out the allowable size.
    size_t local_workgroup_size;
    CHECK_CL(clGetKernelWorkGroupInfo(kernel,
                                      device_id,
                                      CL_KERNEL_WORK_GROUP_SIZE,
                                      sizeof(local_workgroup_size),
                                      &local_workgroup_size,
                                      NULL));
    fprintf(stderr, "kernel local workgroup size=%d\n", (int)local_workgroup_size);

    cl_ulong kernel_local_mem_size;
    CHECK_CL(clGetKernelWorkGroupInfo(kernel,
                                      device_id,
                                      CL_KERNEL_LOCAL_MEM_SIZE,
                                      sizeof(kernel_local_mem_size),
                                      &kernel_local_mem_size,
                                      NULL));
    fprintf(stderr, "kernel local mem size=%d\n", (int)kernel_local_mem_size);

    // Set the arguments to the kernel.
    if (mode != MODE_SVM) {
        CHECK_CL(clSetKernelArg(kernel, 0, sizeof(cl_mem), &ctx.device_dom));
        CHECK_CL(clSetKernelArg(kernel, 1, sizeof(cl_mem), &ctx.device_stylesheet));
        CHECK_CL(clSetKernelArg(kernel, 2, sizeof(cl_mem), &ctx.device_properties));
        CHECK_CL(clSetKernelArg(kernel, 3, sizeof(cl_mem), &ctx.device_classes));
    } else {
#ifndef NO_SVM
        CHECK_CL(clSetKernelArgSVMPointerAMD(kernel, 0, ctx.host_dom));
        CHECK_CL(clSetKernelArgSVMPointerAMD(kernel, 1, ctx.host_stylesheet));
        CHECK_CL(clSetKernelArgSVMPointerAMD(kernel, 2, ctx.host_properties));
        CHECK_CL(clSetKernelArgSVMPointerAMD(kernel, 3, ctx.host_classes));
#endif
    }

    for (int i = 0; i < 3; ++i)
    {
        // Create the stylesheet and the DOM.
        init_mem(commands, inctx, &ctx, device_name, mode, device_type);

        start = mach_absolute_time();
        size_t global_work_size = NODE_COUNT;
        local_workgroup_size = local_workgroup_size > 320 ? 320 : local_workgroup_size;
        CHECK_CL(clEnqueueNDRangeKernel(commands,
                                        kernel,
                                        1,
                                        NULL,
                                        &global_work_size,
                                        &local_workgroup_size,
                                        0,
                                        NULL,
                                        NULL));
        clFinish(commands);

        // Report timing.
        double elapsed = (double)(mach_absolute_time() - start) / 1000000.0;
        report_timing(device_name, "kernel execution", elapsed, false, mode);
    }

    if (mode != MODE_SVM) {
        // Retrieve the DOM.
        struct dom_node *device_dom_mirror = (struct dom_node *)clEnqueueMapBuffer(commands, ctx.device_dom, CL_TRUE, CL_MAP_READ, 0, inctx->size_dom, 0, NULL, NULL, &err);
        CHECK_CL(err);

        cl_int *device_classes_mirror = (cl_int *)clEnqueueMapBuffer(commands, ctx.device_classes, CL_TRUE, CL_MAP_READ, 0, inctx->size_classes, 0, NULL, NULL, &err);
        CHECK_CL(err);

        //check_dom(device_dom_mirror, device_classes_mirror);

        const char* dom_output_path = device_type == CL_DEVICE_TYPE_GPU ? "GPU_out.dat" : "CPU_out.dat";
        dump_dom(device_dom_mirror, device_classes_mirror, dom_output_path);

        CHECK_CL(clEnqueueUnmapMemObject(commands,
                                         ctx.device_dom,
                                         device_dom_mirror,
                                         0,
                                         NULL,
                                         NULL));
        CHECK_CL(clEnqueueUnmapMemObject(commands,
                                         ctx.device_classes,
                                         device_classes_mirror,
                                         0,
                                         NULL,
                                         NULL));

        clReleaseMemObject(ctx.device_dom);
        clReleaseMemObject(ctx.device_stylesheet);
        clReleaseMemObject(ctx.device_properties);
        clReleaseMemObject(ctx.device_classes);
    } else {
        check_dom(ctx.host_dom, ctx.host_classes);
    }

    clReleaseProgram(program);
    clReleaseKernel(kernel);
    clReleaseCommandQueue(commands);
    clReleaseContext(context);
}

#define PRINT_PLATFORM_INFO(platform, name) \
    do { \
        size_t size; \
        CHECK_CL(clGetPlatformInfo(platform, CL_PLATFORM_##name, 0, NULL, &size)); \
        char *result = (char *)malloc(size); \
        CHECK_CL(clGetPlatformInfo(platform, CL_PLATFORM_##name, size, result, NULL)); \
        fprintf(stderr, "%s: %s\n", #name, result); \
        free(result); \
    } while(0)

int main() {
    cl_platform_id platforms[10];
    cl_uint num_platforms;
    CHECK_CL(clGetPlatformIDs(10, platforms, &num_platforms));
    fprintf(stderr,
            "%d platform(s) available: first ID %p\n",
            (int)num_platforms,
            platforms[0]);
    cl_platform_id platform = platforms[0];

    PRINT_PLATFORM_INFO(platform, PROFILE);
    PRINT_PLATFORM_INFO(platform, VERSION);
    PRINT_PLATFORM_INFO(platform, NAME);
    PRINT_PLATFORM_INFO(platform, VENDOR);
    PRINT_PLATFORM_INFO(platform, EXTENSIONS);

    fprintf(stderr, "Size of a DOM node: %d\n", (int)sizeof(struct dom_node));

    //time_t seed = time(NULL);
    //srand(seed);
    srand(42);

    struct input_context inctx = {0};

    inctx.size_dom = sizeof(struct dom_node) * NODE_COUNT;
    inctx.size_stylesheet = sizeof(struct css_stylesheet) * 1;
    inctx.size_properties = sizeof(struct css_property) * PROPERTY_COUNT;
    inctx.size_classes = sizeof(cl_int) * CLASS_COUNT;

    fprintf(stderr, "DOM size: %d\n", inctx.size_dom);
    fprintf(stderr, "stylesheet size: %d\n", inctx.size_stylesheet);
    fprintf(stderr, "properties size: %d\n", inctx.size_properties);
    fprintf(stderr, "classes size: %d\n", inctx.size_classes);

    inctx.dom = (struct dom_node*)malloc(inctx.size_dom);
    inctx.stylesheet = (struct css_stylesheet*)malloc(inctx.size_stylesheet);
    inctx.properties = (struct css_property*)malloc(inctx.size_properties);
    inctx.classes = (cl_int*)malloc(inctx.size_classes);

    memset(inctx.dom, 0, inctx.size_dom);
    memset(inctx.stylesheet, 0, inctx.size_stylesheet);
    memset(inctx.properties, 0, inctx.size_properties);
    memset(inctx.classes, 0, inctx.size_classes);

    // Create the stylesheet and the DOM.
    uint64_t start = mach_absolute_time();
    int property_index = 0;
    create_stylesheet(inctx.stylesheet, inctx.properties, &property_index);
    int class_count = 0, global_count = 0;
    create_dom(inctx.dom, inctx.classes, NULL, &class_count, &global_count, 0);

    double elapsed = (double)(mach_absolute_time() - start) / 1000000.0;
    report_timing("???", "stylesheet/DOM creation", elapsed, false, MODE_MAPPED);

    dump_dom(inctx.dom, inctx.classes, "DOM_in.dat");

    go(platform, CL_DEVICE_TYPE_GPU, MODE_MAPPED, &inctx);
#ifndef NO_SVM
    //go(platform, CL_DEVICE_TYPE_GPU, MODE_SVM, &inctx);
#endif
    go(platform, CL_DEVICE_TYPE_CPU, MODE_MAPPED, &inctx);
    return 0;
}

