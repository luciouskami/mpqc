//
// fjt.cc
//
// Copyright (C) 2001 Edward Valeev
//
// Author: Edward Valeev <edward.valeev@chemistry.gatech.edu>
// Maintainer: EV
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

#ifdef __GNUG__
#pragma implementation
#endif

#include <cmath>
#include <util/misc/math.h>
#include <iostream>
#include <chemistry/qc/basis/fjt.h>

using namespace sc;
using namespace std;

#define SOFT_ZERO 1e-6

/*------------------------------------------------------
  Initialize Taylor_Fm_Eval object (computes incomplete
  gamma function via Taylor interpolation)
 ------------------------------------------------------*/
Taylor_Fjt::Taylor_Fjt(unsigned int mmax, double accuracy) :
    cutoff_(accuracy), interp_order_(TAYLOR_INTERPOLATION_ORDER),
    ExpMath_(interp_order_+1,2*(mmax + interp_order_ - 1))
{
   
  /*---------------------------------------
    We are doing Taylor interpolation with
    n=TAYLOR_ORDER terms here:
    error <= delT^n/(n+1)!
   ---------------------------------------*/
  delT_ = 2.0*std::pow(cutoff_*ExpMath_.fac[interp_order_+1],
		       1.0/interp_order_);
  max_m_ = mmax + interp_order_ - 1;

  T_crit_ = new double[max_m_ + 1];   /*--- m=0 is included! ---*/
  max_T_ = 0;
  /*--- Figure out T_crit for each m and put into the T_crit ---*/
  for(int m=max_m_; m>=0; --m) {
    /*------------------------------------------
      Newton-Raphson method to solve
      T^{m-0.5}*exp(-T) = epsilon*Gamma(m+0.5)
      The solution is the max T for which to do
      the interpolation
     ------------------------------------------*/
    double T = -log(cutoff_);
    const double egamma = cutoff_ * std::sqrt(M_PI) * ExpMath_.df[2*m]/std::pow(2.0,m);
    double T_new = T;
    double func;
    do {
      T = T_new;
      func = std::pow(T,m-0.5) * std::exp(-T) - egamma;
      const double dfuncdT = ((m-0.5) * std::pow(T,m-1.5) - std::pow(T,m-0.5)) * std::exp(-T);
      if (dfuncdT >= 0.0) {
	T_new *= 2.5;
	continue;
      }
      T_new = T - func/dfuncdT;
      if ( T_new <= 0.0 ) {
	T_new = T / 2.0;
      }
    } while (std::fabs(func/egamma) >= SOFT_ZERO);
    T_crit_[m] = T_new;
    const int T_idx = std::floor(T_new/delT_);
    max_T_ = std::max(max_T_,T_idx);
  }

  // allocate the grid (see the comments below)
  {
      const int nrow = max_T_+1;
      const int ncol = max_m_+1;
      grid_ = new double*[nrow];
      grid_[0] = new double[nrow*ncol];
      for(int r=1; r<nrow; ++r)
	  grid_[r] = grid_[r-1] + ncol;
  }

  /*-------------------------------------------------------
    Tabulate the gamma function from t=delT to T_crit[m]:
    1) include T=0 though the table is empty for T=0 since
       Fm(0) is simple to compute
    2) modified MacLaurin series converges fastest for
       the largest m -> use it to compute Fmmax(T)
       see JPC 94, 5564 (1990).
    3) then either use the series to compute the rest
       of the row or maybe use downward recursion
   -------------------------------------------------------*/
  /*--- do the mmax first ---*/
  for(int m=0; m<=max_m_; ++m) {
      for(int T_idx = max_T_;
	  T_idx >= 0;
	  --T_idx) {
	  const double T = T_idx * delT_;
	  double denom = (m+0.5);
	  const double T_o_denom = T/denom;
	  double term = 0.5*std::exp(-T)/denom;
	  double sum = term;
	  double rel_error;
	  do {
	      denom += 1.0;
	      term *= T_o_denom;
	      sum += term;
	      rel_error = term/sum;
	  } while (rel_error >= cutoff_);

	  grid_[T_idx][m] = sum;
      }
  }
}

Taylor_Fjt::~Taylor_Fjt()
{
  delete[] T_crit_;
  T_crit_ = 0;
  delete[] grid_[0];
  delete[] grid_;
  grid_ = NULL;
}

/* Using the tabulated incomplete gamma function in gtable, compute
 * the incomplete gamma function for a particular wval for all 0<=j<=J.
 * The result is placed in the global intermediate int_fjttable.
 */
double *
Taylor_Fjt::values(int l, double T)
{
#include "static.h"

  const double T_crit = T_crit_[l];
  const double two_T = 2.0*T;

  /*------------------------
    First compute Fl(T) ...
   ------------------------*/
  if (T > T_crit) {
    /*--- Asymptotic formula ---*/
    F_[l] = ExpMath_.df[2*l] * std::sqrt(M_PI/2)/std::pow(two_T,l+0.5);
  }
  else {
    /*--- Taylor interpolation ---*/
      const int T_ind = (int)std::floor(0.5+T/delT_);
      const double h = T_ind * delT_ - T;
      const double* F_row = grid_[T_ind] + l;
      F_[l] =          F_row[0]
#if TAYLOR_INTERPOLATION_ORDER > 0
	    +       h*(F_row[1]
#endif
#if TAYLOR_INTERPOLATION_ORDER > 1
	    +oon[2]*h*(F_row[2]
#endif
#if TAYLOR_INTERPOLATION_ORDER > 2
	    +oon[3]*h*(F_row[3]
#endif
#if TAYLOR_INTERPOLATION_ORDER > 3
	    +oon[4]*h*(F_row[4]
#endif
#if TAYLOR_INTERPOLATION_ORDER > 4
	    +oon[5]*h*(F_row[5]
#endif
#if TAYLOR_INTERPOLATION_ORDER > 5
	    +oon[6]*h*(F_row[6]
#endif
#if TAYLOR_INTERPOLATION_ORDER > 6
	    +oon[7]*h*(F_row[7]
#endif
#if TAYLOR_INTERPOLATION_ORDER > 7
	    +oon[8]*h*(F_row[8])
#endif
#if TAYLOR_INTERPOLATION_ORDER > 6
		 )
#endif
#if TAYLOR_INTERPOLATION_ORDER > 5
		 )
#endif
#if TAYLOR_INTERPOLATION_ORDER > 4
		 )
#endif
#if TAYLOR_INTERPOLATION_ORDER > 3
		 )
#endif
#if TAYLOR_INTERPOLATION_ORDER > 2
		 )
#endif
#if TAYLOR_INTERPOLATION_ORDER > 1
		 )
#endif
#if TAYLOR_INTERPOLATION_ORDER > 0
		 )
#endif
	  ;
  }

  /*------------------------------------
    And then do downward recursion in m
   ------------------------------------*/
  if (l > 0) {
    double F_mp1 = F_[l];
    const double exp_mT = std::exp(-T);
    for(int m=l-1; m>=0; --m) {
      const double F_m = (exp_mT + two_T*F_mp1)*oo2np1[m];
      F_[m] = F_m;
      F_mp1 = F_m;
    }
  }

  return F_;
}

Taylor_Fjt::ExpensiveMath::ExpensiveMath(int ifac, int idf)
{
  if (ifac >= 0) {
      fac = new double[ifac+1];
      fac[0] = 1.0;
      for(int i=1; i<=ifac; i++) {
	  fac[i] = i*fac[i-1];
      }
  }

  if (idf >= 0) {
      df = new double[idf+1];
      /* df[i] gives (i-1)!!, so that (-1)!! is defined... */
      df[0] = 1.0;
      if (idf >= 1) df[1] = 1.0;
      if (idf >= 2) df[2] = 1.0;
      for(int i=3; i<=idf; i++){
	  df[i] = (i-1)*df[i-2];
      }
  }

}

Taylor_Fjt::ExpensiveMath::~ExpensiveMath()
{
  delete[] fac;
  delete[] df;
}


/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ-CONDENSED"
// End: