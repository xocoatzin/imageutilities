/*
 * Copyright (c) ICG. All rights reserved.
 *
 * Institute for Computer Graphics and Vision
 * Graz University of Technology / Austria
 *
 *
 * This software is distributed WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the above copyright notices for more information.
 *
 *
 * Project     : ImageUtilities
 * Module      : Core
 * Class       : none
 * Language    : C/CUDA
 * Description : CUDA kernels for core functions
 *
 * Author     : Manuel Werlberger
 * EMail      : werlberger@icg.tugraz.at
 *
 */

#ifndef IUCORE_CONVERT_CU
#define IUCORE_CONVERT_CU

#include <cutil_math.h>
#include "coredefs.h"
#include "memorydefs.h"
#include "iutextures.cuh"

namespace iuprivate {


/* ***************************************************************************
 *  CUDA KERNELS
 * ***************************************************************************/

//-----------------------------------------------------------------------------
/** convert kernel 32f_C3 -> 32f_C4 (float3 -> float4)
 */
__global__ void cuConvertC3ToC4Kernel(const float3* src, size_t src_stride, int src_width, int src_height,
                                      float4* dst, size_t dst_stride, int dst_width, int dst_height)
{
  const int x = blockIdx.x*blockDim.x + threadIdx.x;
  const int y = blockIdx.y*blockDim.y + threadIdx.y;
  int src_c = y*src_stride + x;
  int dst_c = y*dst_stride + x;

  if (x<src_width && y<src_height && x<dst_width && y<dst_height)
  {
    float3 val=src[src_c];
    dst[dst_c] =  make_float4(val.x, val.y, val.z, 1.0f);
  }
}

//-----------------------------------------------------------------------------
/** convert kernel 32f_C4 -> 32f_C3 (float4 -> float3)
 */
__global__ void cuConvertC4ToC3Kernel(const float4* src, size_t src_stride, int src_width, int src_height,
                                      float3* dst, size_t dst_stride, int dst_width, int dst_height)
{
  const int x = blockIdx.x*blockDim.x + threadIdx.x;
  const int y = blockIdx.y*blockDim.y + threadIdx.y;
  int src_c = y*src_stride + x;
  int dst_c = y*dst_stride + x;

  if (x<src_width && y<src_height && x<dst_width && y<dst_height)
  {
    float4 val=src[src_c];
    dst[dst_c] = make_float3(val.x, val.y, val.z);
  }
}


//-----------------------------------------------------------------------------
/** convert kernel 8u_C1 -> 32f_C1 (unsigned char -> float)
 */
__global__ void cuConvert8uC1To32fC1Kernel(const unsigned char *src, size_t src_stride, int src_width, int src_height,
                                           float* dst, size_t dst_stride, int dst_width, int dst_height, float mul_constant,
                                           float add_constant)
{
  const int x = blockIdx.x*blockDim.x + threadIdx.x;
  const int y = blockIdx.y*blockDim.y + threadIdx.y;
  int src_c = y*src_stride + x;
  int dst_c = y*dst_stride + x;

  if (x<src_width && y<src_height && x<dst_width && y<dst_height)
  {
    dst[dst_c] = src[src_c] * mul_constant + add_constant;
  }
}

//-----------------------------------------------------------------------------
/** convert kernel 32f_C1 -> 8u_C1 (float -> unsigned char)
 */
__global__ void cuConvert32fC1To8uC1Kernel(const float* src, size_t src_stride, int src_width, int src_height,
                                           unsigned char* dst, size_t dst_stride, int dst_width, int dst_height, float mul_constant,
                                           unsigned char add_constant)
{
  const int x = blockIdx.x*blockDim.x + threadIdx.x;
  const int y = blockIdx.y*blockDim.y + threadIdx.y;
  int src_c = y*src_stride + x;
  int dst_c = y*dst_stride + x;

  if (x<src_width && y<src_height && x<dst_width && y<dst_height)
  {
    dst[dst_c] = src[src_c] * mul_constant + add_constant;
  }
}

//-----------------------------------------------------------------------------
/** convert kernel 32f_C4 -> 8u_C4 (float4 -> unsigned char4)
 */
__global__ void cuConvert32fC4To8uC4Kernel(const float4* src, size_t src_stride, int src_width, int src_height,
                                           uchar4* dst, size_t dst_stride, int dst_width, int dst_height, float mul_constant,
                                           unsigned char add_constant)
{
  const int x = blockIdx.x*blockDim.x + threadIdx.x;
  const int y = blockIdx.y*blockDim.y + threadIdx.y;
  int src_c = y*src_stride + x;
  int dst_c = y*dst_stride + x;

  if (x<src_width && y<src_height && x<dst_width && y<dst_height)
  {
    float4 val = src[src_c];
    uchar4 res;
    res.x = val.x * mul_constant + add_constant;
    res.y = val.y * mul_constant + add_constant;
    res.z = val.z * mul_constant + add_constant;
    res.w = val.w * mul_constant + add_constant;
    dst[dst_c] = res;
  }
}

//-----------------------------------------------------------------------------
/** convert kernel rgb -> hsv
 */
__global__ void cuConvertRGBToHSVKernel(const float4* src, float4* dst, size_t stride,
                                        int width, int height, bool normalize)
{
  const int x = blockIdx.x*blockDim.x + threadIdx.x;
  const int y = blockIdx.y*blockDim.y + threadIdx.y;
  int c = y*stride + x;

  if (x<width && y<height)
  {
    // Read
    float4 in = src[c];
    float R = in.x;
    float G = in.y;
    float B = in.z;

    float Ma = IUMAX(R, IUMAX(G, B));
    float mi = IUMIN(R, IUMIN(G, B));
    float C = Ma-mi;

    // Hue
    float H = 0.0f;
    if (C != 0.0f)
    {
      if (Ma == R)
        H = fmod((G - B)/C, 6.0f);
      if (Ma == G)
        H = (B - R)/C + 2.0f;
      if (Ma == B)
        H = (R - G)/C + 4.0f;
    }

    H *= 60.0f;

    // Value
    float V = Ma;

    // Saturation
    float S = 0.0f;
    if (C != 0.0f)
      S = C/V;

    if (H < 0.0f)
      H += 360.0f;

    // Normalize
    if (normalize)
      H /= 360.0f;

    // Write Back
    dst[c] = make_float4(H, S, V, in.w);
  }
}

//-----------------------------------------------------------------------------
/** convert kernel hsv -> rgb
 */
__global__ void cuConvertHSVToRGBKernel(const float4* src, float4* dst, size_t stride,
                                        int width, int height, bool denormalize)
{
  const int x = blockIdx.x*blockDim.x + threadIdx.x;
  const int y = blockIdx.y*blockDim.y + threadIdx.y;
  int c = y*stride + x;

  if (x<width && y<height)
  {
    // Read
    float4 in = src[c];
    float H = in.x;
    float S = in.y;
    float V = in.z;

    float4 rgb = make_float4(0.0f, 0.0f, 0.0f, 0.0f);

    //    float C = V*S;

    //    // Denormalize
    //    if (denormalize)
    //      H = H*360.0f;

    //    // RGB
    //    H /= 60.0f;
    //    float X = C*(1.0f - abs(fmod(H, 2.0f) - 1.0f));


    //    if (H >= 0.0f)
    //    {
    //      if (H < 1.0f)
    //        rgb = make_float4(C, X, 0.0f, 0.0f);
    //      else if (H < 2.0f)
    //        rgb = make_float4(X, C, 0.0f, 0.0f);
    //      else if (H < 3.0f)
    //        rgb = make_float4(0.0f, C, X, 0.0f);
    //      else if (H < 4.0f)
    //        rgb = make_float4(0.0f, X, C, 0.0f);
    //      else if (H < 5.0f)
    //        rgb = make_float4(X, 0.0f, C, 0.0f);
    //      else if (H <= 6.0f)
    //        rgb = make_float4(C, 0.0f, X, 0.0f);
    //    }

    //    float m = V-C;
    //    rgb += m;

    if (S == 0)
    {
      rgb = make_float4(V, V, V, in.w);
      dst[c] = rgb;
      return;
    }

     H /= 60.0f;
     int i = floor(H);
     float f = H-i;
     float p = V*(1.0f - S);
     float q = V*(1.0f - S*f);
     float t = V*(1.0f - S*(1.0f-f));

     if (i == 0)
       rgb = make_float4(V, t, p, in.w);
     else if (i == 1)
       rgb = make_float4(q, V, p, in.w);
     else if (i == 2)
       rgb = make_float4(p, V, t, in.w);
     else if (i == 3)
       rgb = make_float4(p, q, V, in.w);
     else if (i == 4)
       rgb = make_float4(t, p, V, in.w);
     else if (i == 5)
       rgb = make_float4(V, p, q, in.w);



    // Write Back
    rgb.w = in.w;
    dst[c] = rgb;
  }
}


/* ***************************************************************************
 *  CUDA WRAPPERS
 * ***************************************************************************/

//-----------------------------------------------------------------------------
/** convert kernel 32f_C3 -> 32f_C4 (float3 -> float4)
 */
IuStatus cuConvert(const iu::ImageGpu_32f_C3* src, const IuRect& src_roi,
                   iu::ImageGpu_32f_C4* dst, const IuRect& dst_roi)
{
  // fragmentation
  const unsigned int block_size = 16;
  dim3 dimBlock(block_size, block_size);
  dim3 dimGrid(iu::divUp(dst_roi.width - dst_roi.x, dimBlock.x),
               iu::divUp(dst_roi.height - dst_roi.y, dimBlock.y));

  cuConvertC3ToC4Kernel<<<dimGrid, dimBlock>>>(src->data(src_roi.x, src_roi.y), src->stride(),
                                               src_roi.width, src_roi.height,
                                               dst->data(dst_roi.x, dst_roi.y), dst->stride(),
                                               dst_roi.width, dst_roi.height);

  IU_CHECK_AND_RETURN_CUDA_ERRORS();
}

//-----------------------------------------------------------------------------
/** convert kernel 32f_C4 -> 32f_C3 (float4 -> float3)
 */
IuStatus cuConvert(const iu::ImageGpu_32f_C4* src, const IuRect& src_roi,
                   iu::ImageGpu_32f_C3* dst, const IuRect& dst_roi)
{
  // fragmentation
  const unsigned int block_size = 16;
  dim3 dimBlock(block_size, block_size);
  dim3 dimGrid(iu::divUp(dst_roi.width - dst_roi.x, dimBlock.x),
               iu::divUp(dst_roi.height - dst_roi.y, dimBlock.y));

  cuConvertC4ToC3Kernel<<<dimGrid, dimBlock>>>(src->data(src_roi.x, src_roi.y), src->stride(),
                                               src_roi.width, src_roi.height,
                                               dst->data(dst_roi.x, dst_roi.y), dst->stride(),
                                               dst_roi.width, dst_roi.height);

  IU_CHECK_AND_RETURN_CUDA_ERRORS();
}


//-----------------------------------------------------------------------------
IuStatus cuConvert_8u_32f(const iu::ImageGpu_8u_C1* src, const IuRect& src_roi,
                          iu::ImageGpu_32f_C1* dst, const IuRect& dst_roi, float mul_constant,
                          float add_constant)
{
  // fragmentation
  const unsigned int block_size = 16;
  dim3 dimBlock(block_size, block_size);
  dim3 dimGrid(iu::divUp(dst_roi.width - dst_roi.x, dimBlock.x),
               iu::divUp(dst_roi.height - dst_roi.y, dimBlock.y));

  cuConvert8uC1To32fC1Kernel<<<dimGrid, dimBlock>>>(src->data(src_roi.x, src_roi.y),
                                                    src->stride(), src_roi.width, src_roi.height,
                                                    dst->data(dst_roi.x, dst_roi.y),
                                                    dst->stride(), dst_roi.width, dst_roi.height,
                                                    mul_constant, add_constant);

  IU_CHECK_AND_RETURN_CUDA_ERRORS();
}


//-----------------------------------------------------------------------------
IuStatus cuConvert_32f_8u(const iu::ImageGpu_32f_C1* src, const IuRect& src_roi,
                          iu::ImageGpu_8u_C1* dst, const IuRect& dst_roi, float mul_constant,
                          unsigned char add_constant)
{
  // fragmentation
  const unsigned int block_size = 16;
  dim3 dimBlock(block_size, block_size);
  dim3 dimGrid(iu::divUp(dst_roi.width - dst_roi.x, dimBlock.x),
               iu::divUp(dst_roi.height - dst_roi.y, dimBlock.y));

  cuConvert32fC1To8uC1Kernel<<<dimGrid, dimBlock>>>(src->data(src_roi.x, src_roi.y), src->stride(),
                                                    src_roi.width, src_roi.height,
                                                    dst->data(dst_roi.x, dst_roi.y),
                                                    dst->stride(), dst_roi.width, dst_roi.height,
                                                    mul_constant, add_constant);

  IU_CHECK_AND_RETURN_CUDA_ERRORS();
}

//-----------------------------------------------------------------------------
IuStatus cuConvert_32f_8u(const iu::ImageGpu_32f_C4* src, const IuRect& src_roi,
                          iu::ImageGpu_8u_C4* dst, const IuRect& dst_roi, float mul_constant,
                          unsigned char add_constant)
{
  // fragmentation
  const unsigned int block_size = 16;
  dim3 dimBlock(block_size, block_size);
  dim3 dimGrid(iu::divUp(dst_roi.width - dst_roi.x, dimBlock.x),
               iu::divUp(dst_roi.height - dst_roi.y, dimBlock.y));

  cuConvert32fC4To8uC4Kernel<<<dimGrid, dimBlock>>>(src->data(src_roi.x, src_roi.y),
                                                    src->stride(), src_roi.width, src_roi.height,
                                                    dst->data(dst_roi.x, dst_roi.y),
                                                    dst->stride(), dst_roi.width,
                                                    dst_roi.height, mul_constant, add_constant);

  IU_CHECK_AND_RETURN_CUDA_ERRORS();
}

//-----------------------------------------------------------------------------
IuStatus cuConvert_rgb_to_hsv(const iu::ImageGpu_32f_C4* src, iu::ImageGpu_32f_C4* dst,
                              bool normalize)
{
  // fragmentation
  const unsigned int block_size = 16;
  dim3 dimBlock(block_size, block_size);
  dim3 dimGrid(iu::divUp(src->width(), dimBlock.x),
               iu::divUp(src->height(), dimBlock.y));

  cuConvertRGBToHSVKernel<<<dimGrid, dimBlock>>>(src->data(), dst->data(), src->stride(),
                                                 src->width(), src->height(), normalize);

  cudaThreadSynchronize();
  IU_CHECK_AND_RETURN_CUDA_ERRORS();
}

//-----------------------------------------------------------------------------
IuStatus cuConvert_hsv_to_rgb(const iu::ImageGpu_32f_C4* src, iu::ImageGpu_32f_C4* dst,
                              bool denormalize)
{
  // fragmentation
  const unsigned int block_size = 16;
  dim3 dimBlock(block_size, block_size);
  dim3 dimGrid(iu::divUp(src->width(), dimBlock.x),
               iu::divUp(src->height(), dimBlock.y));

  cuConvertHSVToRGBKernel<<<dimGrid, dimBlock>>>(src->data(), dst->data(), src->stride(),
                                                 src->width(), src->height(), denormalize);

  cudaThreadSynchronize();
  IU_CHECK_AND_RETURN_CUDA_ERRORS();
}



} // namespace iuprivate

#endif // IUCORE_CONVERT_CU
