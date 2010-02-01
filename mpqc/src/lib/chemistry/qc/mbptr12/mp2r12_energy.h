//
// mp2r12_energy.h
//
// Copyright (C) 2003 Edward Valeev
//
// Author: Edward Valeev <evaleev@vt.edu>
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
#pragma interface
#endif

#include <util/ref/ref.h>
#include <chemistry/qc/mbptr12/r12technology.h>
#include <chemistry/qc/mbptr12/r12int_eval.h>
#include <chemistry/qc/mbptr12/spin.h>
#include <chemistry/qc/mbptr12/twobodygrid.h>
#include <chemistry/qc/mbptr12/pairiter.h>
#include <chemistry/qc/mbptr12/mp2r12_energy_util.h>

#ifndef _chemistry_qc_mbptr12_mp2r12energy_h
#define _chemistry_qc_mbptr12_mp2r12energy_h

#define MP2R12ENERGY_CAN_COMPUTE_PAIRFUNCTION 1

namespace sc {
  /**
   * The class R12EnergyIntermediates stores all intermediates
   * for F12 calculations and administrates their computation
   * and / or provision.
   */
  class R12EnergyIntermediates : virtual public SavableState {
    private:
      R12Technology::StandardApproximation stdapprox_;
      Ref<R12IntEval> r12eval_;
      bool V_computed_;
      RefSCMatrix V_[NSpinCases2];
      bool X_computed_;
      RefSymmSCMatrix X_[NSpinCases2];
      bool B_computed_;
      RefSymmSCMatrix B_[NSpinCases2];
      bool A_computed_;
      RefSCMatrix A_[NSpinCases2];
    public:
      typedef enum { V=0, X=1, B=2, A=3 } IntermediateType;
      R12EnergyIntermediates(const Ref<R12IntEval>& r12eval,
                             const R12Technology::StandardApproximation stdapp);
      R12EnergyIntermediates(StateIn &si);
      void save_data_state(StateOut &so);
      ~R12EnergyIntermediates(){}
      Ref<R12IntEval> r12eval() const;
      void set_r12eval(Ref<R12IntEval> &r12eval);
      R12Technology::StandardApproximation stdapprox() const;
      bool V_computed() const;
      bool X_computed() const;
      bool B_computed() const;
      bool A_computed() const;
      void V_computed(const bool computed);
      void X_computed(const bool computed);
      void B_computed(const bool computed);
      void A_computed(const bool computed);
      const RefSCMatrix& get_V(const SpinCase2 &spincase2) const;
      void assign_V(const SpinCase2 &spincase2, const RefSCMatrix& V);
      const RefSymmSCMatrix& get_X(const SpinCase2 &spincase2) const;
      void assign_X(const SpinCase2 &spincase2, const RefSymmSCMatrix& X);
      const RefSymmSCMatrix& get_B(const SpinCase2 &spincase2) const;
      void assign_B(const SpinCase2 &spincase2, const RefSymmSCMatrix& B);
      const RefSCMatrix& get_A(const SpinCase2 &spincase2) const;
      void assign_A(const SpinCase2 &spincase2, const RefSCMatrix& A);
  };

  /** Class MP2R12Energy is the object that computes and maintains MP2-R12 energies */
class MP2R12Energy : virtual public SavableState {
  protected:
    Ref<R12EnergyIntermediates> r12intermediates_;
    Ref<R12IntEval> r12eval_;
    int debug_;
    bool evaluated_;

    // Initialize SCVectors and SCMatrices
    virtual void init_() = 0;
  public:

    MP2R12Energy(StateIn&);
    MP2R12Energy(const Ref<R12EnergyIntermediates>& r12intermediates,
                 int debug);
    ~MP2R12Energy();

    void save_data_state(StateOut&);
    void obsolete();
    void print(std::ostream&o=ExEnv::out0()) const;

    Ref<R12IntEval> r12eval() const;
    const Ref<R12EnergyIntermediates>& r12intermediates() const;
    R12Technology::StandardApproximation stdapprox() const;
    void set_debug(int debug);
    int get_debug() const;

    virtual void print_pair_energies(bool spinadapted, std::ostream&so=ExEnv::out0()) = 0;
    /// total correlation energy (including OBS singles for non-Brillouin references)
    virtual double energy() = 0;
    virtual const RefSCVector& ef12(SpinCase2 S) const {};

    virtual double emp2f12tot(SpinCase2 S) const = 0;
    virtual double ef12tot(SpinCase2 S) const = 0;

#if MP2R12ENERGY_CAN_COMPUTE_PAIRFUNCTION
    /** Computes values of pair function ij on tbgrid */
    virtual void compute_pair_function(unsigned int i, unsigned int j, SpinCase2 spincase2,
                               const Ref<TwoBodyGrid>& tbgrid) {};
#endif

    /// Computes the first-order R12 wave function and MP2-R12 energy
    virtual void compute() = 0;
    /** Returns the matrix of first-order amplitudes of r12-multiplied occupied orbital pairs.
      */
    virtual RefSCMatrix C(SpinCase2 S) = 0;
    /** Returns the matrix of first-order amplitudes of conventional orbital pairs.
     */
    virtual RefSCMatrix T2(SpinCase2 S) = 0;
};

/**
 * The class MP2R12Energy_SpinOrbital is the original implementation of MP2R12Energy
 * It supports only the standard orbital-invariant ansatz and the full set of features
 * of R12Technology.
 */
class MP2R12Energy_SpinOrbital : public MP2R12Energy
{
  private:
    RefSCVector ef12_[NSpinCases2], emp2f12_[NSpinCases2];
    // The coefficients are stored xy by ij, where xy is the geminal-multiplied pair
    RefSCMatrix C_[NSpinCases2];

    double emp2f12tot(SpinCase2 S) const;
    double ef12tot(SpinCase2 S) const;

    // Initialize SCVectors and SCMatrices
    void init_();

    /** Computes values of all 2-body products from
        space1 and space2 if electron 1 is at r1 and
        electron 2 is at r2. equiv specifies whether electrons
        are equivalent (same spin) or not */
    RefSCVector compute_2body_values_(bool equiv, const Ref<OrbitalSpace>& space1, const Ref<OrbitalSpace>& space2,
                                      const SCVector3& r1, const SCVector3& r2) const;
  public:
    MP2R12Energy_SpinOrbital(StateIn&);
    MP2R12Energy_SpinOrbital(Ref<R12EnergyIntermediates> &r12intermediates, int debug);
    ~MP2R12Energy_SpinOrbital();

    void save_data_state(StateOut&);
    void compute();

    // Print pair energies nicely to so
    void print_pair_energies(bool spinadapted, std::ostream&so=ExEnv::out0());

#if MP2R12ENERGY_CAN_COMPUTE_PAIRFUNCTION
  /** Computes values of pair function ij on tbgrid */
  void compute_pair_function(unsigned int i, unsigned int j, SpinCase2 spincase2,
                             const Ref<TwoBodyGrid>& tbgrid);
#endif
  /// Returns the vector of second-order pair energies of spin case S
  const RefSCVector& emp2f12(SpinCase2 S) const;
  /// Returns the vector of F12 corrections to second-order pair energies of spin case S
  const RefSCVector& ef12(SpinCase2 S) const;
  /// Returns total MP2-F12 correlation energy
  double energy();

  /** Returns the matrix of first-order amplitudes of r12-multiplied occupied orbital pairs.
  */
  RefSCMatrix C(SpinCase2 S);
  /** Returns the matrix of first-order amplitudes of conventional orbital pairs.
  */
  RefSCMatrix T2(SpinCase2 S);
};

/**
 * The class MP2R12Energy_SpinOrbital_new is a new version of MP2R12Energy_SpinOrbital
 * and computes diagonal and non diagonal MP2 F12 energies preserving the Spin symmetry
 * of the wavefunction. It also implements fixed-amplitude
 * version of MP2-R12 where the coefficients are determined according to the singlet and
 * triplet electron pair coalescence conditions. For non-diagonal MP2 F12 calculations
 * the old version MP2R12Energy_SpinOrbital should be preferred, since it invokes many
 * security checks still not implemented in this version of MP2R12Energy_SpinOrbital_new.
 */
class MP2R12Energy_SpinOrbital_new : public MP2R12Energy
{
  private:
    RefSCVector ef12_[NSpinCases2], emp2f12_[NSpinCases2];
    // The coefficients are stored xy by ij, where xy is the geminal-multiplied pair
    RefSCMatrix C_[NSpinCases2];

    // Initialize SCVectors and SCMatrices
    void init_();

    /** Computes values of all 2-body products from
        space1 and space2 if electron 1 is at r1 and
        electron 2 is at r2. equiv specifies whether electrons
        are equivalent (same spin) or not */
    RefSCVector compute_2body_values_(bool equiv, const Ref<OrbitalSpace>& space1, const Ref<OrbitalSpace>& space2,
                                      const SCVector3& r1, const SCVector3& r2) const;

    RefSymmSCMatrix compute_B_non_pairspecific(const RefSymmSCMatrix &B,
                                               const RefSymmSCMatrix &X,
                                               const RefSCMatrix &V,
                                               const RefSCMatrix &A,
                                               const SpinCase2 &spincase2);
    RefSymmSCMatrix compute_B_pairspecific(const SpinMOPairIter &ij_iter,
                                           const RefSymmSCMatrix &B,
                                           const RefSymmSCMatrix &X,
                                           const RefSCMatrix &V,
                                           const RefSCMatrix &A,
                                           const SpinCase2 &spincase2);
    void determine_C_non_pairspecific(const RefSymmSCMatrix &B_ij,
                                      const RefSCMatrix &V,
                                      const SpinCase2 &spincase2,
                                      const Ref<MP2R12EnergyUtil_Diag> &util);
    void determine_C_pairspecific(const int ij,
                                  const RefSymmSCMatrix &B_ij,
                                  const RefSCMatrix &V,
                                  const SpinCase2 &spincase2);
    void determine_C_fixed_non_pairspecific(const SpinCase2 &spincase2);
    void determine_ef12_hylleraas(const RefSymmSCMatrix &B_ij,
                                  const RefSCMatrix &V,
                                  const SpinCase2 &spincase2,
                                  const Ref<MP2R12EnergyUtil_Diag> &util);
    void compute_MP2R12_nondiag();
    void compute_MP2R12_diag_fullopt();
    void compute_MP2R12_diag_nonfullopt();

    // shortcuts
    bool diag() const;
    bool fixedcoeff() const;

  public:
    MP2R12Energy_SpinOrbital_new(StateIn&);
    MP2R12Energy_SpinOrbital_new(Ref<R12EnergyIntermediates> &r12intermediates,
                                 int debug);
    ~MP2R12Energy_SpinOrbital_new();

    void save_data_state(StateOut&);
    void compute();

    // Print pair energies nicely to so
    void print_pair_energies(bool spinadapted, std::ostream&so=ExEnv::out0());

#if MP2R12ENERGY_CAN_COMPUTE_PAIRFUNCTION
  /** Computes values of pair function ij on tbgrid */
  void compute_pair_function(unsigned int i, unsigned int j, SpinCase2 spincase2,
                             const Ref<TwoBodyGrid>& tbgrid);
#endif
  /// Returns the vector of second-order pair energies of spin case S
  const RefSCVector& emp2f12(SpinCase2 S) const;
  /// Returns the vector of F12 corrections to second-order pair energies of spin case S
  const RefSCVector& ef12(SpinCase2 S) const;
  /// Returns total MP2-F12 correlation energy
  double energy();

  double emp2f12tot(SpinCase2 S) const;
  double ef12tot(SpinCase2 S) const;

  /** Returns the matrix of first-order amplitudes of r12-multiplied occupied orbital pairs.
  */
  RefSCMatrix C(SpinCase2 S);
  /** Returns the matrix of first-order amplitudes of conventional orbital pairs.
  */
  RefSCMatrix T2(SpinCase2 S);
};

Ref<MP2R12Energy> construct_MP2R12Energy(Ref<R12EnergyIntermediates> &r12intermediates,
                                         int debug,
                                         bool use_new_version);

}

#endif

// Local Variables:
// mode: c++
// c-file-style: "CLJ"
// End:


