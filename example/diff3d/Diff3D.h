//
//  diff3d.h
//  diff3d
//
//  Created by keno on 2016/06/26.
//  Copyright © 2016年 keno. All rights reserved.
//


#ifndef Diff3D_h
#define Diff3D_h

#include <mpi.h>
#include <math.h>
#include <stdio.h>
#include <CB_Define.h>
#include "Ffunc.h"

// Physical parameters
typedef struct {
  REAL_TYPE dh;    ///< Mesh width
  REAL_TYPE dt;    ///< Time increment
  REAL_TYPE alpha; ///< coefficient
  REAL_TYPE org[3];///< origin of subdomain
} Phys_Param;


// Cotrol parameters
typedef struct {
  int laststep; ///< Number of steps to calculate
  int fileout;  ///< Interval to output to a file
  int blocking; ///< 0-blocking, 1-nonblocking
} Cntl_Param;


// Linear Solver, do not use zero. Zero indicates invalid.
enum Linear_Solver {
  Jacobi=1,
  SOR,
  SOR2SMA
};

#endif /* Diff3D_h */