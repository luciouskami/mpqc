//
// scpr.cc
//
// Copyright (C) 1997 Limit Point Systems, Inc.
//
// Author: Curtis Janssen <cljanss@limitpt.com>
// Maintainer: LPS
//
// This file is part of the SC Toolkit utilities.
//
// The SC Toolkit utilities are free software; you can redistribute them
// and/or modify them under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2, or (at your
// option) any later version.
//
// The SC Toolkit utilities are distributed in the hope that they will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with the SC Toolkit utilities; see the file COPYING.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#ifdef HAVE_CONFIG_H
#include <scconfig.h>
#endif

#include <new.h>

#include <util/misc/exenv.h>
#include <util/misc/formio.h>
#include <util/group/mstate.h>
#include <util/group/message.h>
#include <util/group/thread.h>
#include <util/group/memory.h>

// Force linkages:
#include <chemistry/qc/wfn/linkage.h>
#include <chemistry/qc/dft/linkage.h>
#include <chemistry/qc/mbpt/linkage.h>
//#include <chemistry/qc/psi/linkage.h>
#include <util/state/linkage.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

//////////////////////////////////////////////////////////////////////////

static void
clean_up(void)
{
  MessageGrp::set_default_messagegrp(0);
}

static void
out_of_memory()
{
  cerr << "ERROR: out of memory" << endl;
  abort();
}

int
main(int argc, char *argv[])
{
  atexit(clean_up);
  set_new_handler(out_of_memory);

  ExEnv::set_args(argc, argv);

#ifdef HAVE_MPI
  // MPI is allowed wait until MPI_Init to fill in argc and argv,
  // so we may have to call MPI_Init before we even know that we
  // want an MPIMessageGrp.  The command name is used to let scpr
  // know that an early init is needed.
  if (!strcmp(ExEnv::program_name(), "scpr-mpi")) {
    MPI_Init(&argc, &argv);
  }
#endif

  int i;
  int debug = 0;
  int version = 0;
  int warranty = 0;
  int license = 0;
  int help = 0;
  const char *working_dir = 0;
  char **files = 0;
  char **objects = 0;
  int nfile = 0;
  int nobject = 0;
  for (i=1; i<argc; i++) {
      char *arg = argv[i];
      if (!strcmp(arg,"-messagegrp")) i++;
      else if (!strcmp(arg,"-memorygrp")) i++;
      else if (!strcmp(arg,"-threadgrp")) i++;
      else if (!strcmp(arg,"-W")) working_dir = argv[++i];
      else if (!strcmp(arg,"-d")) debug = 1;
      else if (!strcmp(arg,"-h")) help = 1;
      else if (!strcmp(arg,"-v")) version = 1;
      else if (!strcmp(arg,"-w")) warranty = 1;
      else if (!strcmp(arg,"-L")) license = 1;
      else if (!strcmp(arg,"-o")) {
          if (argc > i+1) {
              char **newobjects = new char *[nobject+1];
              memcpy(newobjects, objects, sizeof(char*)*nobject);
              delete[] objects;
              objects = newobjects;
              objects[nobject++] = argv[++i];
            }
          else help = 1;
        }
      else {
          char **newfiles = new char *[nfile+1];
          memcpy(newfiles, files, sizeof(char*)*nfile);
          delete[] files;
          files = newfiles;
          files[nfile++] = arg;
        }
    }

  const char *SCPR_VERSION = "1.0";

  if (help || nobject == 0 || nfile == 0) {
      cout << node0
           << indent << "scpr version " << SCPR_VERSION << endl
           << SCFormIO::copyright << endl
           << indent << "usage: " << argv[0] << " [options] file ..." << endl
           << indent << "where options are chosen from:" << endl
           << indent << "-o <$val> (print the object with the name $val)"<<endl
           << indent << "-memorygrp <$val> (which memory group to use)" << endl
           << indent << "-threadgrp <$val> (which thread group to use)" << endl
           << indent << "-messagegrp <$val> (which message group to use)"<<endl
           << indent << "-W <$val> (set the working directory)" << endl
           << indent << "-d (turn on debugging)" << endl
           << indent << "-v (print the version)" << endl
           << indent << "-w (print the warranty)" << endl
           << indent << "-l (detailed list of objects)" << endl
           << indent << "-L (print the license)" << endl
           << indent << "-h (print this help)" << endl;

      cout << node0 << endl
           << indent << "object names take the form classname:ordinal_number"
           << endl
           << indent << "at least one file and object name must be given"
           << endl;
      exit(0);
    }
  
  if (version) {
    cout << node0
         << indent << "scpr version " << SCPR_VERSION << endl
         << SCFormIO::copyright;
    exit(0);
  }
  
  if (warranty) {
    cout << node0
         << indent << "scpr version " << SCPR_VERSION << endl
         << SCFormIO::copyright << endl
         << SCFormIO::warranty;
    exit(0);
  }
  
  if (license) {
    cout << node0
         << indent << "scpr version " << SCPR_VERSION << endl
         << SCFormIO::copyright << endl
         << SCFormIO::license;
    exit(0);
  }

  // set the working dir
  if (working_dir && strcmp(working_dir,"."))
    chdir(working_dir);

  // get the message group.  first try the commandline and environment
  RefMessageGrp grp = MessageGrp::initial_messagegrp(argc, argv);
  if (grp.nonnull())
    MessageGrp::set_default_messagegrp(grp);
  else
    grp = MessageGrp::get_default_messagegrp();

  // get the thread group.  first try the commandline and environment
  RefThreadGrp thread = ThreadGrp::initial_threadgrp(argc, argv);
  if (thread.nonnull())
    ThreadGrp::set_default_threadgrp(thread);
  else
    thread = ThreadGrp::get_default_threadgrp();

  // set up output classes
  SCFormIO::setindent(cout, 0);
  SCFormIO::setindent(cerr, 0);

  SCFormIO::set_printnode(0);
  if (grp->n() > 1)
    SCFormIO::init_mp(grp->me());

  if (debug)
    SCFormIO::set_debug(1);

  for (i=0; i<nfile; i++) {
      if (nfile > 1) {
          cout << node0 << indent << files[i] << ":" << endl;
          cout << incindent;
        }
      BcastStateInBin s(grp,files[i]);
      for (int j=0; j<nobject; j++) {
          if (nobject > 1) {
              cout << node0 << indent << objects[j] << ":" << endl;
              cout << incindent;
            }
          RefSavableState o;
          o.restore_state(s,objects[j]);
          cout << node0 << o;
          if (nobject > 1) cout << decindent;
        }
      if (nfile > 1) cout << decindent;
    }

  delete[] files;

  return 0;
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ"
// End:
