/* Link-time stub. HiveOS loads the real libOpenCL.so.1 at runtime. */
typedef int cl_int;
typedef unsigned cl_uint;
typedef void* cl_ptr;
cl_int clGetPlatformIDs(void) { return 0; }
cl_int clGetDeviceIDs(void) { return 0; }
cl_int clGetDeviceInfo(void) { return 0; }
cl_ptr clCreateContext(void) { return 0; }
cl_ptr clCreateCommandQueueWithProperties(void) { return 0; }
cl_ptr clCreateCommandQueue(void) { return 0; }
cl_ptr clCreateProgramWithSource(void) { return 0; }
cl_int clBuildProgram(void) { return 0; }
cl_int clGetProgramBuildInfo(void) { return 0; }
cl_ptr clCreateKernel(void) { return 0; }
cl_ptr clCreateBuffer(void) { return 0; }
cl_int clSetKernelArg(void) { return 0; }
cl_int clEnqueueWriteBuffer(void) { return 0; }
cl_int clEnqueueReadBuffer(void) { return 0; }
cl_int clEnqueueNDRangeKernel(void) { return 0; }
cl_int clFlush(void) { return 0; }
cl_int clFinish(void) { return 0; }
cl_int clGetEventInfo(void) { return 0; }
cl_int clWaitForEvents(void) { return 0; }
cl_int clReleaseEvent(void) { return 0; }
cl_int clReleaseMemObject(void) { return 0; }
cl_int clReleaseKernel(void) { return 0; }
cl_int clReleaseProgram(void) { return 0; }
cl_int clReleaseCommandQueue(void) { return 0; }
cl_int clReleaseContext(void) { return 0; }
