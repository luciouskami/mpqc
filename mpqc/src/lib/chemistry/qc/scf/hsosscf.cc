
#ifdef __GNUC__
#pragma implementation
#pragma implementation "hsoscont.h"
#endif

#include <iostream.h>
#include <math.h>

#include <util/misc/timer.h>

#include <math/scmat/block.h>
#include <math/scmat/blocked.h>
#include <math/scmat/local.h>
#include <math/scmat/repl.h>
#include <math/scmat/dist.h>

#include <math/optimize/diis.h>
#include <math/optimize/scextrapmat.h>

#include <chemistry/qc/basis/petite.h>

#include <chemistry/qc/scf/scflocal.h>
#include <chemistry/qc/scf/hsosscf.h>
#include <chemistry/qc/scf/hsoscont.h>
#include <chemistry/qc/scf/lgbuild.h>
#include <chemistry/qc/scf/ltbgrad.h>

///////////////////////////////////////////////////////////////////////////

#ifdef __GNUC__
template class GBuild<LocalHSOSContribution>;
template class LocalGBuild<LocalHSOSContribution>;

template class TBGrad<LocalHSOSGradContribution>;
template class LocalTBGrad<LocalHSOSGradContribution>;
#endif

///////////////////////////////////////////////////////////////////////////
// HSOSSCF

#define CLASSNAME HSOSSCF
#define HAVE_STATEIN_CTOR
#define HAVE_KEYVAL_CTOR
#define PARENTS public SCF
#include <util/class/classi.h>
void *
HSOSSCF::_castdown(const ClassDesc*cd)
{
  void* casts[1];
  casts[0] = SCF::_castdown(cd);
  return do_castdowns(casts,cd);
}

HSOSSCF::HSOSSCF(StateIn& s) :
  SCF(s)
{
  s.get(user_occupations_);
  s.get(tndocc_);
  s.get(tnsocc_);
  s.get(nirrep_);
  s.get(ndocc_);
  s.get(nsocc_);
}

HSOSSCF::HSOSSCF(const RefKeyVal& keyval) :
  SCF(keyval)
{
  // calculate the total nuclear charge
  int Znuc=0;
  PointBag_double *z = molecule()->charges();
  
  for (Pix i=z->first(); i; z->next(i)) Znuc += (int) z->get(i);

  // check to see if this is to be a charged molecule
  int charge = keyval->intvalue("total_charge");
  int nelectrons = Znuc-charge;

  // first let's try to figure out how many open shells there are
  if (keyval->exists("nsocc")) {
    tnsocc_ = keyval->intvalue("nsocc");
  } else if (keyval->exists("multiplicity")) {
    tnsocc_ = keyval->intvalue("multiplicity")-1;
  } else {
    // if there's an odd number of electrons, then do a doublet, otherwise
    // do a triplet
    if (nelectrons%2)
      tnsocc_=1;
    else
      tnsocc_=2;
  }
  
  // now do the same for the number of doubly occupied shells
  if (keyval->exists("ndocc")) {
    tndocc_ = keyval->intvalue("ndocc");
  } else {
    tndocc_ = (nelectrons-tnsocc_)/2;
    if ((nelectrons-tnsocc_)%2) {
      fprintf(stderr,
              "\n  HSOSSCF::init: Warning, there's a leftover electron.\n");
      fprintf(stderr,"    total_charge = %d\n",charge);
      fprintf(stderr,"    total nuclear charge = %d\n", Znuc);
      fprintf(stderr,"    ndocc_ = %d\n", tndocc_);
      fprintf(stderr,"    nsocc_ = %d\n", tnsocc_);
    }
  }

  printf("\n  HSOSSCF::init: total charge = %d\n\n", Znuc-2*tndocc_-tnsocc_);

  nirrep_ = molecule()->point_group().char_table().ncomp();

  if (keyval->exists("docc") && keyval->exists("socc")) {
    ndocc_ = new int[nirrep_];
    nsocc_ = new int[nirrep_];
    user_occupations_=1;
    for (int i=0; i < nirrep_; i++) {
      ndocc_[i] = keyval->intvalue("docc",i);
      nsocc_[i] = keyval->intvalue("socc",i);
    }
  } else {
    ndocc_=0;
    nsocc_=0;
    user_occupations_=0;
    set_occupations(0);
  }

  printf("  docc = [");
  for (int i=0; i < nirrep_; i++)
    printf(" %d",ndocc_[i]);
  printf(" ]\n");

  printf("  socc = [");
  for (int i=0; i < nirrep_; i++)
    printf(" %d",nsocc_[i]);
  printf(" ]\n");

  // check to see if this was done in SCF(keyval)
  if (!keyval->exists("maxiter"))
    maxiter_ = 100;
}

HSOSSCF::~HSOSSCF()
{
  if (ndocc_) {
    delete[] ndocc_;
    ndocc_=0;
  }
  if (nsocc_) {
    delete[] nsocc_;
    nsocc_=0;
  }
}

void
HSOSSCF::save_data_state(StateOut& s)
{
  SCF::save_data_state(s);
  s.put(user_occupations_);
  s.put(tndocc_);
  s.put(tnsocc_);
  s.put(nirrep_);
  s.put(ndocc_,nirrep_);
  s.put(nsocc_,nirrep_);
}

double
HSOSSCF::occupation(int ir, int i)
{
  if (i < ndocc_[ir]) return 2.0;
  else if (i < ndocc_[ir] + nsocc_[ir]) return 1.0;
  return 0.0;
}

int
HSOSSCF::value_implemented()
{
  return 1;
}

int
HSOSSCF::gradient_implemented()
{
  return 1;
}

int
HSOSSCF::hessian_implemented()
{
  return 0;
}

void
HSOSSCF::print(ostream&o)
{
  SCF::print(o);
}

//////////////////////////////////////////////////////////////////////////////

void
HSOSSCF::set_occupations(const RefDiagSCMatrix& ev)
{
  if (user_occupations_)
    return;
  
  int i,j;
  
  RefDiagSCMatrix evals;
  
  if (ev.null())
    evals = core_hamiltonian().eigvals();
  else
    evals = ev;

  // first convert evals to something we can deal with easily
  BlockedDiagSCMatrix *evalsb = BlockedDiagSCMatrix::require_castdown(evals,
                                                 "HSOSSCF::set_occupations");
  
  RefPetiteList pl = integral()->petite_list(basis());
  
  double **vals = new double*[nirrep_];
  for (i=0; i < nirrep_; i++) {
    int nf=pl->nfunction(i);
    if (nf) {
      vals[i] = new double[nf];
      evalsb->block(i)->convert(vals[i]);
    } else {
      vals[i] = 0;
    }
  }

  // now loop to find the tndocc_ lowest eigenvalues and populate those
  // MO's
  int *newdocc = new int[nirrep_];
  memset(newdocc,0,sizeof(int)*nirrep_);

  for (i=0; i < tndocc_; i++) {
    // find lowest eigenvalue
    int lir,ln;
    double lowest=999999999;

    for (int ir=0; ir < nirrep_; ir++) {
      int nf=pl->nfunction(ir);
      if (!nf)
        continue;
      for (j=0; j < nf; j++) {
        if (vals[ir][j] < lowest) {
          lowest=vals[ir][j];
          lir=ir;
          ln=j;
        }
      }
    }
    vals[lir][ln]=999999999;
    newdocc[lir]++;
  }

  int *newsocc = new int[nirrep_];
  memset(newsocc,0,sizeof(int)*nirrep_);

  for (i=0; i < tnsocc_; i++) {
    // find lowest eigenvalue
    int lir,ln;
    double lowest=999999999;

    for (int ir=0; ir < nirrep_; ir++) {
      int nf=pl->nfunction(ir);
      if (!nf)
        continue;
      for (j=0; j < nf; j++) {
        if (vals[ir][j] < lowest) {
          lowest=vals[ir][j];
          lir=ir;
          ln=j;
        }
      }
    }
    vals[lir][ln]=999999999;
    newsocc[lir]++;
  }

  // get rid of vals
  for (i=0; i < nirrep_; i++)
    if (vals[i])
      delete[] vals[i];
  delete[] vals;

  if (!ndocc_) {
    ndocc_=newdocc;
    nsocc_=newsocc;
  } else {
    // test to see if newocc is different from ndocc_
    for (i=0; i < nirrep_; i++) {
      if (ndocc_[i] != newdocc[i]) {
        fprintf(stderr,"  HSOSSCF::set_occupations:  WARNING!!!!\n");
        fprintf(stderr,"    occupations for irrep %d have changed\n",i+1);
        fprintf(stderr,"    ndocc was %d, changed to %d\n",
                ndocc_[i],newdocc[i]);
      }
      if (nsocc_[i] != newsocc[i]) {
        fprintf(stderr,"  HSOSSCF::set_occupations:  WARNING!!!!\n");
        fprintf(stderr,"    occupations for irrep %d have changed\n",i+1);
        fprintf(stderr,"    nsocc was %d, changed to %d\n",
                nsocc_[i],newsocc[i]);
      }
    }

    memcpy(ndocc_,newdocc,sizeof(int)*nirrep_);
    memcpy(nsocc_,newsocc,sizeof(int)*nirrep_);
    delete[] newdocc;
    delete[] newsocc;
  }
}

//////////////////////////////////////////////////////////////////////////////
//
// scf things
//

void
HSOSSCF::init_vector()
{
  // initialize the two electron integral classes
  tbi_ = integral()->electron_repulsion();

  // calculate the core Hamiltonian
  cl_hcore_ = core_hamiltonian();
  
  // allocate storage for other temp matrices
  cl_dens_ = cl_hcore_.clone();
  cl_dens_.assign(0.0);
  
  cl_dens_diff_ = cl_hcore_.clone();
  cl_dens_diff_.assign(0.0);

  cl_fock_ = cl_hcore_.clone();
  cl_fock_.assign(0.0);
  
  op_dens_ = cl_hcore_.clone();
  op_dens_.assign(0.0);
  
  op_dens_diff_ = cl_hcore_.clone();
  op_dens_diff_.assign(0.0);

  op_fock_ = cl_hcore_.clone();
  op_fock_.assign(0.0);
  
  // gmat is in AO basis
  cl_gmat_ = basis()->matrixkit()->symmmatrix(basis()->basisdim());
  cl_gmat_.assign(0.0);

  op_gmat_ = cl_gmat_.clone();
  op_gmat_.assign(0.0);

  // test to see if we need a guess vector
  if (eigenvectors_.result_noupdate().null())
    eigenvectors_ = hcore_guess();

  scf_vector_ = eigenvectors_.result_noupdate();

  local_ = (LocalSCMatrixKit::castdown(basis()->matrixkit())) ? 1 : 0;
}

void
HSOSSCF::done_vector()
{
  tbi_=0;
  
  // save these if we're doing the gradient or hessian
  if (!gradient_needed() && !hessian_needed()) {
    cl_fock_ = 0;
    op_fock_ = 0;
  }

  cl_hcore_ = 0;
  cl_gmat_ = 0;
  cl_dens_ = 0;
  cl_dens_diff_ = 0;
  op_gmat_ = 0;
  op_dens_ = 0;
  op_dens_diff_ = 0;

  scf_vector_ = 0;
}

void
HSOSSCF::reset_density()
{
  cl_gmat_.assign(0.0);
  cl_dens_diff_.assign(cl_dens_);

  op_gmat_.assign(0.0);
  op_dens_diff_.assign(op_dens_);
}

double
HSOSSCF::new_density()
{
  BlockedSCMatrix *vecp = BlockedSCMatrix::require_castdown(
    scf_vector_, "HSOSSCF::new_density: scf_vector");

  BlockedSymmSCMatrix *densp = BlockedSymmSCMatrix::require_castdown(
    cl_dens_, "HSOSSCF::new_density: density");

  BlockedSymmSCMatrix *ddensp = BlockedSymmSCMatrix::require_castdown(
    cl_dens_diff_, "HSOSSCF::new_density: density difference");
  
  BlockedSymmSCMatrix *odensp = BlockedSymmSCMatrix::require_castdown(
    op_dens_, "HSOSSCF::new_density: open density");

  BlockedSymmSCMatrix *oddensp = BlockedSymmSCMatrix::require_castdown(
    op_dens_diff_, "HSOSSCF::new_density: open density difference");
  
  RefPetiteList pl = integral()->petite_list(basis());
  
  int ij=0;
  double delta=0;
  int istart, iend, jstart, jend, tri;
  double *ddata;

  for (int ir=0; ir < vecp->nblocks(); ir++) {
    int nbasis = pl->nfunction(ir);

    if (!nbasis)
      continue;
    
    RefSCMatrix vir = vecp->block(ir);
    RefSymmSCMatrix dir = densp->block(ir);
    RefSymmSCMatrix ddir = ddensp->block(ir);
    RefSymmSCMatrix odir = odensp->block(ir);
    RefSymmSCMatrix oddir = oddensp->block(ir);
  
    // copy current density into density diff and scale by -1.  later we'll
    // add the new density to this to get the density difference.
    ddir.assign(dir);
    ddir.scale(-1.0);
    dir.assign(0.0);

    oddir.assign(odir);
    oddir.scale(-1.0);
    odir.assign(0.0);
    
    RefSCMatrixSubblockIter diter =
      dir->local_blocks(SCMatrixSubblockIter::Write);

    double *ck = new double[nbasis];
    
    // loop over columns of the scf vector
    int k;
    for (k=0;  k < ndocc_[ir]; k++) {
      RefSCVector rck = vir.get_column(k);
      rck->convert(ck);

      // now loop over blocks of the density matrix and fill them in
      for (diter->begin(); diter->ready(); diter->next()) {
        SCMatrixBlock *dblk = diter->block();

        ddata = get_tri_block(dblk, istart, iend, jstart, jend, tri);
        if (!ddata) {
          fprintf(stderr,"HSOSSCF::new_density: oops...can't get data\n");
          abort();
        }

        int dij=0;
        for (int i=istart; i < iend; i++)
          for (int j=jstart; j <= (tri ? i : jend-1); j++, dij++)
            ddata[dij] += ck[i]*ck[j];
      }
    }
    // force ReplSCMatrices to do the sum
    diter=0;

    dir.scale(2.0);

    diter = odir->local_blocks(SCMatrixSubblockIter::Write);

    for (;  k < ndocc_[ir]+nsocc_[ir]; k++) {
      RefSCVector rck = vir.get_column(k);
      rck->convert(ck);

      // now loop over blocks of the density matrix and fill them in
      for (diter->begin(); diter->ready(); diter->next()) {
        SCMatrixBlock *dblk = diter->block();

        ddata = get_tri_block(dblk, istart, iend, jstart, jend, tri);
        if (!ddata) {
          fprintf(stderr,"HSOSSCF::new_density: oops...can't get open data\n");
          abort();
        }

        int dij=0;
        for (int i=istart; i < iend; i++)
          for (int j=jstart; j <= (tri ? i : jend-1); j++, dij++)
            ddata[dij] += ck[i]*ck[j];
      }
    }

    // force ReplSCMatrices to do the sum
    diter=0;

    delete[] ck;
    
    dir.accumulate(odir);
    ddir.accumulate(dir);
    oddir.accumulate(odir);

    // now calculate rms delta d
    diter = ddir->local_blocks(SCMatrixSubblockIter::Read);

    for (diter->begin(); diter->ready(); diter->next()) {
      SCMatrixBlock *dblk = diter->block();

      ddata = get_tri_block(dblk, istart, iend, jstart, jend, tri);
      if (!ddata) {
        fprintf(stderr,"CLSCF::new_density: oops...can't get diff data\n");
        abort();
      }

      int dij=0;
      for (int i=istart; i < iend; i++)
        for (int j=jstart; j <= (tri ? i : jend-1); j++, dij++, ij++)
          delta += ddata[dij]*ddata[dij];
    }
  }

  if (!local_ && scf_grp_->n() > 1) {
    scf_grp_->sum(&delta, 1);
    scf_grp_->sum(&ij, 1);
  }

  delta = sqrt(delta/ij);
  return delta;
}

double
HSOSSCF::scf_energy()
{
  RefSymmSCMatrix t = cl_fock_.copy();
  t.accumulate(cl_hcore_);

  RefSymmSCMatrix go = op_fock_.copy();
  go.scale(-1.0);
  go.accumulate(cl_fock_);
  
  BlockedSymmSCMatrix *ofockp = BlockedSymmSCMatrix::require_castdown(
    go, "HSOSSCF::scf_energy: open fock");

  BlockedSymmSCMatrix *odensp = BlockedSymmSCMatrix::require_castdown(
    op_dens_, "HSOSSCF::scf_energy: open density");

  BlockedSymmSCMatrix *densp = BlockedSymmSCMatrix::require_castdown(
    cl_dens_, "HSOSSCF::scf_energy: density");

  BlockedSymmSCMatrix *fockp = BlockedSymmSCMatrix::require_castdown(
    t, "HSOSSCF::scf_energy: H+F");

  double eelec=0;
  for (int ir=0; ir < fockp->nblocks(); ir++) {
    RefSymmSCMatrix dir = densp->block(ir);
    RefSymmSCMatrix fhir = fockp->block(ir);
    RefSymmSCMatrix odir = odensp->block(ir);
    RefSymmSCMatrix goir = ofockp->block(ir);

    if (!dir.n())
      continue;

    RefSCMatrixSubblockIter diter =
      dir->local_blocks(SCMatrixSubblockIter::Read);
    RefSCMatrixSubblockIter fiter =
      fhir->local_blocks(SCMatrixSubblockIter::Read);
    RefSCMatrixSubblockIter oditer =
      odir->local_blocks(SCMatrixSubblockIter::Read);
    RefSCMatrixSubblockIter goiter =
      goir->local_blocks(SCMatrixSubblockIter::Read);
    
    SCMatrixJointSubblockIter subi(diter,fiter,oditer,goiter);

    for (subi.begin(); subi.ready(); subi.next()) {
      SCMatrixBlock *dblk=subi.block(0);
      SCMatrixBlock *fblk=subi.block(1);
      SCMatrixBlock *odblk=subi.block(2);
      SCMatrixBlock *goblk=subi.block(3);

      int istart, iend, jstart, jend, tri;

      double *ddata = get_tri_block(dblk, istart, iend, jstart, jend, tri);
      double *fdata = get_tri_block(fblk, istart, iend, jstart, jend, tri);
      double *oddata = get_tri_block(odblk, istart, iend, jstart, jend, tri);
      double *godata = get_tri_block(goblk, istart, iend, jstart, jend, tri);

      if (!ddata || !fdata || !oddata || !godata) {
        fprintf(stderr,"HSOSSCF::scf_energy: oops...can't get data\n");
        abort();
      }

      int ij=0;
      for (int i=istart; i < iend; i++) {
        for (int j=jstart; j <= (tri ? i : jend-1); j++, ij++) {
          eelec += (i==j) ? 0.5*(ddata[ij]*fdata[ij] - oddata[ij]*godata[ij]) :
                                 ddata[ij]*fdata[ij] - oddata[ij]*godata[ij];
        }
      }
    }
  }

  if (!local_ && (scf_grp_->n() > 1))
    scf_grp_->sum(&eelec, 1);

  return eelec;
}

char *
HSOSSCF::init_pmax(double *pmat_data)
{
  double l2inv = 1.0/log(2.0);
  double tol = pow(2.0,-126.0);
  
  GaussianBasisSet& gbs = *basis().pointer();
  
  char * pmax = new char[i_offset(gbs.nshell())];

  int ish, jsh, ij;
  for (ish=ij=0; ish < gbs.nshell(); ish++) {
    int istart = gbs.shell_to_function(ish);
    int iend = istart + gbs(ish).nfunction();
    
    for (jsh=0; jsh <= ish; jsh++,ij++) {
      int jstart = gbs.shell_to_function(jsh);
      int jend = jstart + gbs(jsh).nfunction();
      
      double maxp=0, tmp;

      for (int i=istart; i < iend; i++) {
        int ijoff = i_offset(i);
        for (int j=jstart; j < ((ish==jsh) ? i+1 : jend); j++,ijoff++)
          if ((tmp=fabs(pmat_data[ijoff])) > maxp)
            maxp=tmp;
      }

      if (maxp <= tol)
        maxp=tol;

      pmax[ij] = (signed char) (log(maxp)*l2inv);
    }
  }

  return pmax;
}

void
HSOSSCF::ao_fock()
{
  RefPetiteList pl = integral()->petite_list(basis());
  
  // calculate G.  First transform cl_dens_diff_ to the AO basis, then
  // scale the off-diagonal elements by 2.0
  RefSymmSCMatrix dd = cl_dens_diff_;
  cl_dens_diff_ = pl->to_AO_basis(dd);
  cl_dens_diff_->scale(2.0);
  cl_dens_diff_->scale_diagonal(0.5);

  RefSymmSCMatrix ddo = op_dens_diff_;
  op_dens_diff_ = pl->to_AO_basis(ddo);
  op_dens_diff_->scale(2.0);
  op_dens_diff_->scale_diagonal(0.5);
  
  // now try to figure out the matrix specialization we're dealing with
  // if we're using Local matrices, then there's just one subblock, or
  // see if we can convert G and P to local matrices
  if (local_ || basis()->nbasis() < 700) {
    RefSCMatrixKit lkit = LocalSCMatrixKit::castdown(basis()->matrixkit());
    RefSCDimension ldim = basis()->basisdim();

    RefSymmSCMatrix ptmp = cl_dens_diff_;
    RefSymmSCMatrix potmp = op_dens_diff_;

    RefSymmSCMatrix gtmp = cl_gmat_;
    RefSymmSCMatrix gotmp = op_gmat_;

    // if these aren't local matrices, then make copies of the density
    // matrices
    if (!local_) {
      lkit = new LocalSCMatrixKit;

      ptmp = lkit->symmmatrix(ldim);
      ptmp->convert(cl_dens_diff_);

      potmp = lkit->symmmatrix(ldim);
      potmp->convert(op_dens_diff_);
    }

    // if we're not dealing with local matrices, or we're running on
    // multiple processors, then make a copy of the G matrices
    if (!local_ || scf_grp_->n() > 1) {
      gtmp = lkit->symmmatrix(ldim);
      gotmp = lkit->symmmatrix(ldim);

      gtmp->assign(0.0);
      gotmp->assign(0.0);
    }

    // create block iterators for the G and P matrices
    RefSCMatrixSubblockIter giter =
      gtmp->local_blocks(SCMatrixSubblockIter::Write);
    giter->begin();
    SCMatrixLTriBlock *gblock = SCMatrixLTriBlock::castdown(giter->block());

    RefSCMatrixSubblockIter piter =
      ptmp->local_blocks(SCMatrixSubblockIter::Read);
    piter->begin();
    SCMatrixLTriBlock *pblock = SCMatrixLTriBlock::castdown(piter->block());

    RefSCMatrixSubblockIter goiter =
      gotmp->local_blocks(SCMatrixSubblockIter::Write);
    goiter->begin();
    SCMatrixLTriBlock *goblock = SCMatrixLTriBlock::castdown(goiter->block());

    RefSCMatrixSubblockIter poiter =
      potmp->local_blocks(SCMatrixSubblockIter::Read);
    poiter->begin();
    SCMatrixLTriBlock *poblock = SCMatrixLTriBlock::castdown(poiter->block());

    double *gmat_data = gblock->data;
    double *pmat_data = pblock->data;
    double *gmato_data = goblock->data;
    double *pmato_data = poblock->data;
    char * pmax = init_pmax(pmat_data);
  
    LocalHSOSContribution lclc(gmat_data, pmat_data, gmato_data, pmato_data);
    LocalGBuild<LocalHSOSContribution>
      gb(lclc, tbi_, integral(), basis(), scf_grp_, pmax);
    gb.build_gmat(desired_value_accuracy()/100.0);

    delete[] pmax;

    // if we're running on multiple processors, then sum the G matrices
    if (scf_grp_->n() > 1) {
      scf_grp_->sum(gmat_data, i_offset(basis()->nbasis()));
      scf_grp_->sum(gmato_data, i_offset(basis()->nbasis()));
    }
    
    // if we're running on multiple processors, or we don't have local
    // matrices, then accumulate gtmp back into G
    if (!local_ || scf_grp_->n() > 1) {
      cl_gmat_->convert_accumulate(gtmp);
      op_gmat_->convert_accumulate(gotmp);
    }
  }

  // for now quit
  else {
    fprintf(stderr,"Cannot yet use anything but Local matrices\n");
    abort();
  }
  
  // get rid of AO delta P
  cl_dens_diff_ = dd;
  dd = cl_dens_diff_.clone();

  op_dens_diff_ = ddo;
  ddo = op_dens_diff_.clone();

  // now symmetrize the skeleton G matrix, placing the result in dd
  RefSymmSCMatrix skel_gmat = cl_gmat_.copy();
  skel_gmat.scale(1.0/(double)pl->order());
  pl->symmetrize(skel_gmat,dd);

  skel_gmat = op_gmat_.copy();
  skel_gmat.scale(1.0/(double)pl->order());
  pl->symmetrize(skel_gmat,ddo);
  
  cl_fock_.assign(cl_hcore_);
  cl_fock_.accumulate(dd);

  op_fock_.assign(cl_fock_);
  ddo.scale(-1.0);
  op_fock_.accumulate(ddo);
}

RefSCExtrapData
HSOSSCF::extrap_data()
{
  RefSCExtrapData data = new SymmSCMatrix2SCExtrapData(cl_fock_,op_fock_);
  return data;
}

/////////////////////////////////////////////////////////////////////////////

class HSOSEffFock : public BlockedSCElementOp2 {
  private:
    HSOSSCF *scf_;

  public:
    HSOSEffFock(HSOSSCF* s) : scf_(s) {}
    ~HSOSEffFock() {}

    int has_side_effects() { return 1; }

    void process(SCMatrixBlockIter& bi1, SCMatrixBlockIter& bi2) {
      int ir=current_block();

      for (bi1.reset(), bi2.reset(); bi1 && bi2; bi1++, bi2++) {
        double occi = scf_->occupation(ir,bi1.i());
        double occj = scf_->occupation(ir,bi1.j());

        if (occi==1.0 && occj==2.0)
          bi1.set(2.0*bi1.get()-bi2.get());
        else if (occi==0.0 && occj==1.0)
          bi1.set(bi2.get());
      
      }
    }
};
    
RefSymmSCMatrix
HSOSSCF::effective_fock()
{
  RefSymmSCMatrix mofock = cl_fock_.clone();
  mofock.assign(0.0);
  mofock.accumulate_transform(scf_vector_.t(), cl_fock_);

  RefSymmSCMatrix mofocko = op_fock_.clone();
  mofocko.assign(0.0);
  mofocko.accumulate_transform(scf_vector_.t(), op_fock_);

  RefSCElementOp2 op = new HSOSEffFock(this);
  mofock.element_op(op, mofocko);

  return mofock;
}

/////////////////////////////////////////////////////////////////////////////

void
HSOSSCF::init_gradient()
{
  // presumably the eigenvectors have already been computed by the time
  // we get here
  scf_vector_ = eigenvectors_.result_noupdate();

  local_ = (LocalSCMatrixKit::castdown(basis()->matrixkit())) ? 1 : 0;
}

void
HSOSSCF::done_gradient()
{
  // save these if we're doing the hessian
  if (!hessian_needed()) {
    cl_fock_=0;
  }

  cl_dens_=0;
  op_dens_=0;
  scf_vector_ = 0;
}

/////////////////////////////////////////////////////////////////////////////

// MO lagrangian
//       c    o   v
//  c  |2*FC|2*FC|0|
//     -------------
//  o  |2*FC| FO |0|
//     -------------
//  v  | 0  |  0 |0|
//
class HSOSLag : public BlockedSCElementOp2 {
  private:
    HSOSSCF *scf_;

  public:
    HSOSLag(HSOSSCF* s) : scf_(s) {}
    ~HSOSLag() {}

    int has_side_effects() { return 1; }

    void process(SCMatrixBlockIter& bi1, SCMatrixBlockIter& bi2) {
      int ir=current_block();

      for (bi1.reset(), bi2.reset(); bi1 && bi2; bi1++, bi2++) {
        double occi = scf_->occupation(ir,bi1.i());
        double occj = scf_->occupation(ir,bi1.j());

        if (occi==1.0 && occj==1.0)
          bi1.set(bi2.get());
        else if (occi==0.0)
          bi1.set(0.0);
      }
    }
};

RefSymmSCMatrix
HSOSSCF::lagrangian()
{
  RefSymmSCMatrix mofock = cl_fock_.clone();
  mofock.assign(0.0);
  mofock.accumulate_transform(scf_vector_.t(), cl_fock_);

  RefSymmSCMatrix mofocko = op_fock_.clone();
  mofocko.assign(0.0);
  mofocko.accumulate_transform(scf_vector_.t(), op_fock_);

  mofock.scale(2.0);
  
  RefSCElementOp2 op = new HSOSLag(this);
  mofock.element_op(op, mofocko);
  mofocko=0;

  // transform MO lagrangian to SO basis
  RefSymmSCMatrix so_lag(basis_dimension(), basis_matrixkit());
  so_lag.assign(0.0);
  so_lag.accumulate_transform(scf_vector_, mofock);
  
  // and then from SO to AO
  RefPetiteList pl = integral()->petite_list();
  RefSymmSCMatrix ao_lag = pl->to_AO_basis(so_lag);

  ao_lag.scale(-1.0);

  return ao_lag;
}

RefSymmSCMatrix
HSOSSCF::gradient_density()
{
  cl_dens_ = basis_matrixkit()->symmmatrix(basis_dimension());
  op_dens_ = cl_dens_.clone();
  
  BlockedSCMatrix *vecp = BlockedSCMatrix::require_castdown(
    scf_vector_, "HSOSSCF::gradient_density: scf_vector");

  BlockedSymmSCMatrix *densp = BlockedSymmSCMatrix::require_castdown(
    cl_dens_, "HSOSSCF::gradient_density: density");

  BlockedSymmSCMatrix *odensp = BlockedSymmSCMatrix::require_castdown(
    op_dens_, "HSOSSCF::gradient_density: open density");

  cl_dens_.assign(0.0);
  op_dens_.assign(0.0);
  
  RefPetiteList pl = integral()->petite_list(basis());
  
  int ij=0;
  double delta=0;
  int istart, iend, jstart, jend, tri;
  double *ddata;

  for (int ir=0; ir < vecp->nblocks(); ir++) {
    int nbasis = pl->nfunction(ir);

    if (!nbasis)
      continue;

    RefSCMatrix vir = vecp->block(ir);
    RefSymmSCMatrix dir = densp->block(ir);
    RefSymmSCMatrix odir = odensp->block(ir);
  
    RefSCMatrixSubblockIter diter =
      dir->local_blocks(SCMatrixSubblockIter::Write);

    double *ck = new double[nbasis];
    
    // loop over columns of the scf vector
    int k;
    for (k=0;  k < ndocc_[ir]; k++) {
      RefSCVector rck = vir.get_column(k);
      rck->convert(ck);

      // now loop over blocks of the density matrix and fill them in
      for (diter->begin(); diter->ready(); diter->next()) {
        SCMatrixBlock *dblk = diter->block();

        ddata = get_tri_block(dblk, istart, iend, jstart, jend, tri);
        if (!ddata) {
          fprintf(stderr,"HSOSSCF::gradient_density: oops...can't get data\n");
          abort();
        }

        int dij=0;
        for (int i=istart; i < iend; i++)
          for (int j=jstart; j <= (tri ? i : jend-1); j++, dij++)
            ddata[dij] += ck[i]*ck[j];
      }
    }

    // force repl matrices to do the sum
    diter=0;

    dir.scale(2.0);

    diter = odir->local_blocks(SCMatrixSubblockIter::Write);

    for (;  k < ndocc_[ir]+nsocc_[ir]; k++) {
      RefSCVector rck = vir.get_column(k);
      rck->convert(ck);

      // now loop over blocks of the density matrix and fill them in
      for (diter->begin(); diter->ready(); diter->next()) {
        SCMatrixBlock *dblk = diter->block();

        ddata = get_tri_block(dblk, istart, iend, jstart, jend, tri);
        if (!ddata) {
          fprintf(stderr,"HSOSSCF::gradient_density: "
                         "oops...can't get open data\n");
          abort();
        }

        int dij=0;
        for (int i=istart; i < iend; i++)
          for (int j=jstart; j <= (tri ? i : jend-1); j++, dij++)
            ddata[dij] += ck[i]*ck[j];
      }
    }

    delete[] ck;
  }
  
  cl_dens_ = pl->to_AO_basis(cl_dens_);
  op_dens_ = pl->to_AO_basis(op_dens_);

  RefSymmSCMatrix tdens = cl_dens_.copy();
  tdens.accumulate(op_dens_);

  op_dens_.scale(2.0);
  
  return tdens;
}

/////////////////////////////////////////////////////////////////////////////

void
HSOSSCF::two_body_deriv(double * tbgrad)
{
  RefSCElementMaxAbs m = new SCElementMaxAbs;
  cl_dens_.element_op(m);
  double pmax = m->result();
  m=0;

  // now try to figure out the matrix specialization we're dealing with.
  // if we're using Local matrices, then there's just one subblock, or
  // see if we can convert P to local matrices
  RefSCMatrixKit lkit = LocalSCMatrixKit::castdown(basis()->matrixkit());
  int local = lkit.nonnull();

  if (local || basis()->nbasis() < 700) {
    RefSCDimension ldim = basis()->basisdim();
    RefSymmSCMatrix ptmp = cl_dens_;
    RefSymmSCMatrix potmp = op_dens_;

    // if these aren't local matrices, then make copies of the density
    // matrices
    if (!local) {
      lkit = new LocalSCMatrixKit;

      ptmp = lkit->symmmatrix(ldim);
      ptmp->convert(cl_dens_);

      potmp = lkit->symmmatrix(ldim);
      potmp->convert(op_dens_);
    }
    
    // create block iterators for the G and P matrices
    RefSCMatrixSubblockIter piter =
      ptmp->local_blocks(SCMatrixSubblockIter::Read);
    piter->begin();
    SCMatrixLTriBlock *pblock = SCMatrixLTriBlock::castdown(piter->block());

    RefSCMatrixSubblockIter poiter =
      potmp->local_blocks(SCMatrixSubblockIter::Read);
    poiter->begin();
    SCMatrixLTriBlock *poblock = SCMatrixLTriBlock::castdown(poiter->block());

    double *pmat_data = pblock->data;
    double *pmato_data = poblock->data;
  
    LocalHSOSGradContribution l(pmat_data,pmato_data);
    LocalTBGrad<LocalHSOSGradContribution> tb(l, integral(), basis(),scf_grp_);
    tb.build_tbgrad(tbgrad, pmax, desired_gradient_accuracy());
  }

  // for now quit
  else {
    fprintf(stderr,"can't do gradient yet\n");
    abort();
  }
}

/////////////////////////////////////////////////////////////////////////////

void
HSOSSCF::init_hessian()
{
}

void
HSOSSCF::done_hessian()
{
}
