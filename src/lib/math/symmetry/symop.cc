//
// symop.cc
//
// Copyright (C) 1996 Limit Point Systems, Inc.
//
// Author: Edward Seidl <seidl@janed.com>
// Maintainer: LPS
//
// This file is part of the SC Toolkit.
//
// The SC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The SC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the SC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#include <util/misc/math.h>

#include <math/symmetry/pointgrp.h>
#include <util/misc/formio.h>

using namespace std;
using namespace sc;

////////////////////////////////////////////////////////////////////////

SymmetryOperation::SymmetryOperation()
{
  zero();
}

SymmetryOperation::SymmetryOperation(const SymmetryOperation &so)
{
  for (int i=0; i<3; i++) {
    for (int j=0; j<3; j++) {
      d[i][j] = so.d[i][j];
    }
  }
}

SymmetryOperation::~SymmetryOperation()
{
}

SymmetryOperation
SymmetryOperation::operate(const SymmetryOperation& r) const
{
  SymmetryOperation ret;
  for (int i=0; i < 3; i++)
    for (int j=0; j < 3; j++) {
      double t=0;
      for (int k=0; k < 3; k++)
        t += r.d[i][k]*d[k][j];
      ret.d[i][j] = t;
    }
  return ret;
}

SymmetryOperation
SymmetryOperation::transform(const SymmetryOperation& r) const
{
  int i,j,k;
  SymmetryOperation ret,foo;
  
  // foo = r * d
  for (i=0; i < 3; i++) {
    for (j=0; j < 3; j++) {
      double t=0;
      for (k=0; k < 3; k++)
        t += r.d[i][k] * d[k][j];
      foo.d[i][j] = t;
    }
  }

  // ret = (r*d)*r~ = foo*r~
  for (i=0; i < 3; i++) {
    for (j=0; j < 3; j++) {
      double t=0;
      for (k=0; k < 3; k++)
        t += foo.d[i][k]*r.d[j][k];
      ret.d[i][j]=t;
    }
  }

  return ret;
}

// Clockwise rotation by 2pi/n degrees
void
SymmetryOperation::rotation(int n)
{
  double theta = (n) ? 2.0*M_PI/n : 2.0*M_PI;
  rotation(theta);
}

// Clockwise rotation by theta degrees
void
SymmetryOperation::rotation(double theta)
{
  zero();

  double ctheta = cos(theta);
  double stheta = sin(theta);

  d[0][0] = ctheta;
  d[0][1] = stheta;
  d[1][0] = -stheta;
  d[1][1] = ctheta;
  d[2][2] = 1.0;
}

void
SymmetryOperation::transpose()
{
  for (int i=1; i<3; i++) {
    for (int j=0; j<i; j++) {
      double tmp = d[i][j];
      d[i][j] = d[j][i];
      d[j][i] = tmp;
    }
  }
}

void
SymmetryOperation::print(ostream& os) const
{
  os << indent << "        1          2          3\n"
     << indent << "  1  "
     << scprintf("%10.7f ", d[0][0])
     << scprintf("%10.7f ", d[0][1])
     << scprintf("%10.7f ", d[0][2]) << endl
     << indent << "  2  "
     << scprintf("%10.7f ", d[1][0])
     << scprintf("%10.7f ", d[1][1])
     << scprintf("%10.7f ", d[1][2]) << endl
     << indent << "  3  "
     << scprintf("%10.7f ", d[2][0])
     << scprintf("%10.7f ", d[2][1])
     << scprintf("%10.7f ", d[2][2]) << endl << endl;
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "ETS"
// End:
