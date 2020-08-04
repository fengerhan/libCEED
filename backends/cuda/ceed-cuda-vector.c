// Copyright (c) 2017-2018, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory. LLNL-CODE-734707.
// All Rights reserved. See files LICENSE and NOTICE for details.
//
// This file is part of CEED, a collection of benchmarks, miniapps, software
// libraries and APIs for efficient high-order finite element and spectral
// element discretizations for exascale applications. For more information and
// source code availability see http://github.com/ceed.
//
// The CEED research is supported by the Exascale Computing Project 17-SC-20-SC,
// a collaborative effort of two U.S. Department of Energy organizations (Office
// of Science and the National Nuclear Security Administration) responsible for
// the planning and preparation of a capable exascale ecosystem, including
// software, applications, hardware, advanced system engineering and early
// testbed platforms, in support of the nation's exascale computing imperative.

#include "ceed-cuda.h"
#include <string.h>

//------------------------------------------------------------------------------
// * Bytes used
//------------------------------------------------------------------------------
static inline size_t bytes(const CeedVector vec) {
  int ierr;
  CeedInt length;
  ierr = CeedVectorGetLength(vec, &length); CeedChk(ierr);
  return length * sizeof(CeedScalar);
}

//------------------------------------------------------------------------------
// Sync host to device
//------------------------------------------------------------------------------
static inline int CeedVectorSyncH2D_Cuda(const CeedVector vec) {
  int ierr;
  Ceed ceed;
  ierr = CeedVectorGetCeed(vec, &ceed); CeedChk(ierr);
  CeedVector_Cuda *data;
  ierr = CeedVectorGetData(vec, (void *)&data); CeedChk(ierr);

  ierr = cudaMemcpy(data->d_array, data->h_array, bytes(vec),
                    cudaMemcpyHostToDevice); CeedChk_Cu(ceed, ierr);
  return 0;
}

//------------------------------------------------------------------------------
// Sync device to host
//------------------------------------------------------------------------------
static inline int CeedVectorSyncD2H_Cuda(const CeedVector vec) {
  int ierr;
  Ceed ceed;
  ierr = CeedVectorGetCeed(vec, &ceed); CeedChk(ierr);
  CeedVector_Cuda *data;
  ierr = CeedVectorGetData(vec, (void *)&data); CeedChk(ierr);

  ierr = cudaMemcpy(data->h_array, data->d_array, bytes(vec),
                    cudaMemcpyDeviceToHost); CeedChk_Cu(ceed, ierr);
  return 0;
}

//------------------------------------------------------------------------------
// Set array from host
//------------------------------------------------------------------------------
static int CeedVectorSetArrayHost_Cuda(const CeedVector vec,
                                       const CeedCopyMode cmode,
                                       CeedScalar *array) {
  int ierr;
  CeedVector_Cuda *data;
  ierr = CeedVectorGetData(vec, (void *)&data); CeedChk(ierr);

  switch (cmode) {
  case CEED_COPY_VALUES: {
    CeedInt length;
    if(!data->h_array) {
      ierr = CeedVectorGetLength(vec, &length); CeedChk(ierr);
      ierr = CeedMalloc(length, &data->h_array_allocated); CeedChk(ierr);
      data->h_array = data->h_array_allocated;
    }
    if (array)
      memcpy(data->h_array, array, bytes(vec));
  } break;
  case CEED_OWN_POINTER:
    ierr = CeedFree(&data->h_array_allocated); CeedChk(ierr);
    data->h_array_allocated = array;
    data->h_array = array;
    break;
  case CEED_USE_POINTER:
    ierr = CeedFree(&data->h_array_allocated); CeedChk(ierr);
    data->h_array = array;
    break;
  }
  data->memState = CEED_CUDA_HOST_SYNC;
  return 0;
}

//------------------------------------------------------------------------------
// Set array from device
//------------------------------------------------------------------------------
static int CeedVectorSetArrayDevice_Cuda(const CeedVector vec,
    const CeedCopyMode cmode, CeedScalar *array) {
  int ierr;
  Ceed ceed;
  ierr = CeedVectorGetCeed(vec, &ceed); CeedChk(ierr);
  CeedVector_Cuda *data;
  ierr = CeedVectorGetData(vec, (void *)&data); CeedChk(ierr);

  switch (cmode) {
  case CEED_COPY_VALUES:
    if (!data->d_array) {
      ierr = cudaMalloc((void **)&data->d_array_allocated, bytes(vec));
      CeedChk_Cu(ceed, ierr);
      data->d_array = data->d_array_allocated;
    }
    if (array) {
      ierr = cudaMemcpy(data->d_array, array, bytes(vec),
                        cudaMemcpyDeviceToDevice); CeedChk_Cu(ceed, ierr);
    }
    break;
  case CEED_OWN_POINTER:
    ierr = cudaFree(data->d_array_allocated); CeedChk_Cu(ceed, ierr);
    data->d_array_allocated = array;
    data->d_array = array;
    break;
  case CEED_USE_POINTER:
    ierr = cudaFree(data->d_array_allocated); CeedChk_Cu(ceed, ierr);
    data->d_array_allocated = NULL;
    data->d_array = array;
    break;
  }
  data->memState = CEED_CUDA_DEVICE_SYNC;
  return 0;
}

//------------------------------------------------------------------------------
// Set the array used by a vector,
//   freeing any previously allocated array if applicable
//------------------------------------------------------------------------------
static int CeedVectorSetArray_Cuda(const CeedVector vec,
                                   const CeedMemType mtype,
                                   const CeedCopyMode cmode,
                                   CeedScalar *array) {
  int ierr;
  Ceed ceed;
  ierr = CeedVectorGetCeed(vec, &ceed); CeedChk(ierr);
  CeedVector_Cuda *data;
  ierr = CeedVectorGetData(vec, (void *)&data); CeedChk(ierr);

  switch (mtype) {
  case CEED_MEM_HOST:
    return CeedVectorSetArrayHost_Cuda(vec, cmode, array);
  case CEED_MEM_DEVICE:
    return CeedVectorSetArrayDevice_Cuda(vec, cmode, array);
  }
  return 1;
}

//------------------------------------------------------------------------------
// Vector Take Array
//------------------------------------------------------------------------------
static int CeedVectorTakeArray_Cuda(CeedVector vec, CeedMemType mtype,
                                    CeedScalar **array) {
  int ierr;
  CeedVector_Cuda *impl;
  ierr = CeedVectorGetData(vec, (void *)&impl); CeedChk(ierr);

  switch(mtype) {
  case CEED_MEM_HOST:
    if (impl->memState == CEED_CUDA_DEVICE_SYNC) {
      ierr = CeedVectorSyncD2H_Cuda(vec); CeedChk(ierr);
    }
    (*array) = impl->h_array;
    impl->h_array = NULL;
    impl->h_array_allocated = NULL;
    impl->memState = CEED_CUDA_HOST_SYNC;
    break;
  case CEED_MEM_DEVICE:
    if (impl->memState == CEED_CUDA_HOST_SYNC) {
      ierr = CeedVectorSyncH2D_Cuda(vec); CeedChk(ierr);
    }
    (*array) = impl->d_array;
    impl->d_array = NULL;
    impl->d_array_allocated = NULL;
    impl->memState = CEED_CUDA_DEVICE_SYNC;
    break;
  }

  return 0;
}

//------------------------------------------------------------------------------
// Set host array to value
//------------------------------------------------------------------------------
static int CeedHostSetValue_Cuda(CeedScalar *h_array, CeedInt length,
                                 CeedScalar val) {
  for (int i = 0; i < length; i++)
    h_array[i] = val;
  return 0;
}

//------------------------------------------------------------------------------
// Set device array to value (impl in .cu file)
//------------------------------------------------------------------------------
int CeedDeviceSetValue_Cuda(CeedScalar *d_array, CeedInt length,
                            CeedScalar val);

//------------------------------------------------------------------------------
// Set a vector to a value,
//------------------------------------------------------------------------------
static int CeedVectorSetValue_Cuda(CeedVector vec, CeedScalar val) {
  int ierr;
  Ceed ceed;
  ierr = CeedVectorGetCeed(vec, &ceed); CeedChk(ierr);
  CeedVector_Cuda *data;
  ierr = CeedVectorGetData(vec, (void *)&data); CeedChk(ierr);
  CeedInt length;
  ierr = CeedVectorGetLength(vec, &length); CeedChk(ierr);

  // Set value for synced device/host array
  switch(data->memState) {
  case CEED_CUDA_HOST_SYNC:
    ierr = CeedHostSetValue_Cuda(data->h_array, length, val); CeedChk(ierr);
    break;
  case CEED_CUDA_NONE_SYNC:
    /*
      Handles the case where SetValue is used without SetArray.
      Default allocation then happens on the GPU.
    */
    if (data->d_array == NULL) {
      ierr = cudaMalloc((void **)&data->d_array_allocated, bytes(vec));
      CeedChk_Cu(ceed, ierr);
      data->d_array = data->d_array_allocated;
    }
    data->memState = CEED_CUDA_DEVICE_SYNC;
    ierr = CeedDeviceSetValue_Cuda(data->d_array, length, val); CeedChk(ierr);
    break;
  case CEED_CUDA_DEVICE_SYNC:
    ierr = CeedDeviceSetValue_Cuda(data->d_array, length, val); CeedChk(ierr);
    break;
  case CEED_CUDA_BOTH_SYNC:
    ierr = CeedHostSetValue_Cuda(data->h_array, length, val); CeedChk(ierr);
    ierr = CeedDeviceSetValue_Cuda(data->d_array, length, val); CeedChk(ierr);
    break;
  }
  return 0;
}

//------------------------------------------------------------------------------
// Get read-only access to a vector via the specified mtype memory type
//   on which to access the array. If the backend uses a different memory type,
//   this will perform a copy (possibly cached).
//------------------------------------------------------------------------------
static int CeedVectorGetArrayRead_Cuda(const CeedVector vec,
                                       const CeedMemType mtype,
                                       const CeedScalar **array) {
  int ierr;
  Ceed ceed;
  ierr = CeedVectorGetCeed(vec, &ceed); CeedChk(ierr);
  CeedVector_Cuda *data;
  ierr = CeedVectorGetData(vec, (void *)&data); CeedChk(ierr);

  // Sync array to requested memtype and update pointer
  switch (mtype) {
  case CEED_MEM_HOST:
    if(data->h_array==NULL) {
      CeedInt length;
      ierr = CeedVectorGetLength(vec, &length); CeedChk(ierr);
      ierr = CeedMalloc(length, &data->h_array_allocated);
      CeedChk(ierr);
      data->h_array = data->h_array_allocated;
    }
    if(data->memState==CEED_CUDA_DEVICE_SYNC) {
      ierr = CeedVectorSyncD2H_Cuda(vec);
      CeedChk(ierr);
      data->memState = CEED_CUDA_BOTH_SYNC;
    }
    *array = data->h_array;
    break;
  case CEED_MEM_DEVICE:
    if (data->d_array==NULL) {
      ierr = cudaMalloc((void **)&data->d_array_allocated, bytes(vec));
      CeedChk_Cu(ceed, ierr);
      data->d_array = data->d_array_allocated;
    }
    if (data->memState==CEED_CUDA_HOST_SYNC) {
      ierr = CeedVectorSyncH2D_Cuda(vec);
      CeedChk(ierr);
      data->memState = CEED_CUDA_BOTH_SYNC;
    }
    *array = data->d_array;
    break;
  }
  return 0;
}

//------------------------------------------------------------------------------
// Get array
//------------------------------------------------------------------------------
static int CeedVectorGetArray_Cuda(const CeedVector vec,
                                   const CeedMemType mtype,
                                   CeedScalar **array) {
  int ierr;
  Ceed ceed;
  ierr = CeedVectorGetCeed(vec, &ceed); CeedChk(ierr);
  CeedVector_Cuda *data;
  ierr = CeedVectorGetData(vec, (void *)&data); CeedChk(ierr);

  // Sync array to requested memtype and update pointer
  switch (mtype) {
  case CEED_MEM_HOST:
    if(data->h_array==NULL) {
      CeedInt length;
      ierr = CeedVectorGetLength(vec, &length); CeedChk(ierr);
      ierr = CeedMalloc(length, &data->h_array_allocated);
      CeedChk(ierr);
      data->h_array = data->h_array_allocated;
    }
    if(data->memState==CEED_CUDA_DEVICE_SYNC) {
      ierr = CeedVectorSyncD2H_Cuda(vec); CeedChk(ierr);
    }
    data->memState = CEED_CUDA_HOST_SYNC;
    *array = data->h_array;
    break;
  case CEED_MEM_DEVICE:
    if (data->d_array==NULL) {
      ierr = cudaMalloc((void **)&data->d_array_allocated, bytes(vec));
      CeedChk_Cu(ceed, ierr);
      data->d_array = data->d_array_allocated;
    }
    if (data->memState==CEED_CUDA_HOST_SYNC) {
      ierr = CeedVectorSyncH2D_Cuda(vec); CeedChk(ierr);
    }
    data->memState = CEED_CUDA_DEVICE_SYNC;
    *array = data->d_array;
    break;
  }
  return 0;
}

//------------------------------------------------------------------------------
// Restore an array obtained using CeedVectorGetArrayRead()
//------------------------------------------------------------------------------
static int CeedVectorRestoreArrayRead_Cuda(const CeedVector vec) {
  return 0;
}

//------------------------------------------------------------------------------
// Restore an array obtained using CeedVectorGetArray()
//------------------------------------------------------------------------------
static int CeedVectorRestoreArray_Cuda(const CeedVector vec) {
  return 0;
}

//------------------------------------------------------------------------------
// Get the norm of a CeedVector
//------------------------------------------------------------------------------
static int CeedVectorNorm_Cuda(CeedVector vec, CeedNormType type,
                               CeedScalar *norm) {
  int ierr;
  Ceed ceed;
  ierr = CeedVectorGetCeed(vec, &ceed); CeedChk(ierr);
  CeedVector_Cuda *data;
  ierr = CeedVectorGetData(vec, (void *)&data); CeedChk(ierr);
  CeedInt length;
  ierr = CeedVectorGetLength(vec, &length); CeedChk(ierr);
  cublasHandle_t handle;
  ierr = CeedCudaGetCublasHandle(ceed, &handle); CeedChk(ierr);

  // Compute norm
  const CeedScalar *d_array;
  ierr = CeedVectorGetArrayRead(vec, CEED_MEM_DEVICE, &d_array); CeedChk(ierr);
  switch (type) {
  case CEED_NORM_1: {
    ierr = cublasDasum(handle, length, d_array, 1, norm);
    CeedChk_Cublas(ceed, ierr);
    break;
  }
  case CEED_NORM_2: {
    ierr = cublasDnrm2(handle, length, d_array, 1, norm);
    CeedChk_Cublas(ceed, ierr);
    break;
  }
  case CEED_NORM_MAX: {
    CeedInt indx;
    ierr = cublasIdamax(handle, length, d_array, 1, &indx);
    CeedChk_Cublas(ceed, ierr);
    CeedScalar normNoAbs;
    ierr = cudaMemcpy(&normNoAbs, data->d_array+indx-1, sizeof(CeedScalar),
                      cudaMemcpyDeviceToHost); CeedChk_Cu(ceed, ierr);
    *norm = fabs(normNoAbs);
    break;
  }
  }
  ierr = CeedVectorRestoreArrayRead(vec, &d_array); CeedChk(ierr);

  return 0;
}

//------------------------------------------------------------------------------
// Take reciprocal of a vector on host
//------------------------------------------------------------------------------
static int CeedHostReciprocal_Cuda(CeedScalar *h_array, CeedInt length) {
  for (int i = 0; i < length; i++)
    if (fabs(h_array[i]) > CEED_EPSILON)
      h_array[i] = 1./h_array[i];
  return 0;
}

//------------------------------------------------------------------------------
// Take reciprocal of a vector on device (impl in .cu file)
//------------------------------------------------------------------------------
int CeedDeviceReciprocal_Cuda(CeedScalar *d_array, CeedInt length);

//------------------------------------------------------------------------------
// Take reciprocal of a vector
//------------------------------------------------------------------------------
static int CeedVectorReciprocal_Cuda(CeedVector vec) {
  int ierr;
  Ceed ceed;
  ierr = CeedVectorGetCeed(vec, &ceed); CeedChk(ierr);
  CeedVector_Cuda *data;
  ierr = CeedVectorGetData(vec, (void *)&data); CeedChk(ierr);
  CeedInt length;
  ierr = CeedVectorGetLength(vec, &length); CeedChk(ierr);

  // Set value for synced device/host array
  switch(data->memState) {
  case CEED_CUDA_HOST_SYNC:
    ierr = CeedHostReciprocal_Cuda(data->h_array, length); CeedChk(ierr);
    break;
  case CEED_CUDA_DEVICE_SYNC:
    ierr = CeedDeviceReciprocal_Cuda(data->d_array, length); CeedChk(ierr);
    break;
  case CEED_CUDA_BOTH_SYNC:
    ierr = CeedDeviceReciprocal_Cuda(data->d_array, length); CeedChk(ierr);
    ierr = CeedVectorSyncArray(vec, CEED_MEM_HOST); CeedChk(ierr);
    break;
  // LCOV_EXCL_START
  case CEED_CUDA_NONE_SYNC:
    break; // Not possible, but included for completness
    // LCOV_EXCL_STOP
  }
  return 0;
}

//------------------------------------------------------------------------------
// Destroy the vector
//------------------------------------------------------------------------------
static int CeedVectorDestroy_Cuda(const CeedVector vec) {
  int ierr;
  Ceed ceed;
  ierr = CeedVectorGetCeed(vec, &ceed); CeedChk(ierr);
  CeedVector_Cuda *data;
  ierr = CeedVectorGetData(vec, (void *)&data); CeedChk(ierr);

  ierr = cudaFree(data->d_array_allocated); CeedChk_Cu(ceed, ierr);
  ierr = CeedFree(&data->h_array_allocated); CeedChk(ierr);
  ierr = CeedFree(&data); CeedChk(ierr);
  return 0;
}

//------------------------------------------------------------------------------
// Create a vector of the specified length (does not allocate memory)
//------------------------------------------------------------------------------
int CeedVectorCreate_Cuda(CeedInt n, CeedVector vec) {
  CeedVector_Cuda *data;
  int ierr;
  Ceed ceed;
  ierr = CeedVectorGetCeed(vec, &ceed); CeedChk(ierr);

  ierr = CeedSetBackendFunction(ceed, "Vector", vec, "SetArray",
                                CeedVectorSetArray_Cuda); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Vector", vec, "TakeArray",
                                CeedVectorTakeArray_Cuda); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Vector", vec, "SetValue",
                                CeedVectorSetValue_Cuda); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Vector", vec, "GetArray",
                                CeedVectorGetArray_Cuda); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Vector", vec, "GetArrayRead",
                                CeedVectorGetArrayRead_Cuda); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Vector", vec, "RestoreArray",
                                CeedVectorRestoreArray_Cuda); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Vector", vec, "RestoreArrayRead",
                                CeedVectorRestoreArrayRead_Cuda); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Vector", vec, "Norm",
                                CeedVectorNorm_Cuda); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Vector", vec, "Reciprocal",
                                CeedVectorReciprocal_Cuda); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Vector", vec, "Destroy",
                                CeedVectorDestroy_Cuda); CeedChk(ierr);

  ierr = CeedCalloc(1, &data); CeedChk(ierr);
  ierr = CeedVectorSetData(vec, (void *)&data); CeedChk(ierr);
  data->memState = CEED_CUDA_NONE_SYNC;
  return 0;
}
