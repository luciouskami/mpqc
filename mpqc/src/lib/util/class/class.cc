//
// class.cc
//
// Copyright (C) 1996 Limit Point Systems, Inc.
//
// Author: Curtis Janssen <cljanss@limitpt.com>
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

#ifdef __GNUG__
#pragma implementation
#endif

#ifdef HAVE_CONFIG_H
#include <scconfig.h>
#endif

#include <stdlib.h>
#include <string.h>
#if defined(HAVE_DLFCN_H)
#include <dlfcn.h>
#endif // HAVE_DLFCN_H

#include <util/class/class.h>

#include <util/class/classMap.h>
#include <util/class/classImplMap.h>

#include <util/class/classkeySet.h>
#include <util/class/classkeyImplSet.h>

ClassKeyClassDescPMap* ClassDesc::all_ = 0;
char * ClassDesc::classlib_search_path_ = 0;
ClassKeySet* ClassDesc::unresolved_parents_ = 0;

/////////////////////////////////////////////////////////////////

ClassKey::ClassKey() :
  classname_(0)
{
}

ClassKey::ClassKey(const char* name):
  classname_(::strcpy(new char[strlen(name)+1],name))
{
}

ClassKey::ClassKey(const ClassKey& key)
{
  if (key.classname_) {
      classname_ = ::strcpy(new char[strlen(key.classname_)+1],key.classname_);
    }
  else {
      classname_ = 0;
    }
}

ClassKey::~ClassKey()
{
  delete[] classname_;
}

ClassKey& ClassKey::operator=(const ClassKey& key)
{
  if (classname_ && classname_ != key.classname_) {
      delete[] classname_;
      classname_ = ::strcpy(new char[strlen(key.classname_)+1],key.classname_);
    }
  return *this;
}

int ClassKey::operator==(ClassKey& ck)
{
  if (!classname_) {
      if (!ck.classname_) return 1;
      else return 0;
    }
  else if (!ck.classname_) return 0;
  
  return !::strcmp(classname_,ck.classname_);
}

int
ClassKey::hash() const
{
  int r=0;
  int i;

  // Even numbered bytes make up the lower part of the hash index
  for (i=0; i < ::strlen(classname_); i+=2) {
      r ^= classname_[i];
    }

  // Odd numbered bytes make up the upper part of the hash index
  for (i=1; i < ::strlen(classname_); i+=2) {
      r ^= classname_[i]<<8;
    }

  return r;
}

int ClassKey::cmp(ClassKey&ck) const
{
  return strcmp(classname_,ck.classname_);
}

char* ClassKey::name() const
{
  return classname_;
}

/////////////////////////////////////////////////////////////////

ParentClass::ParentClass(ClassDesc*classdesc,Access access,int is_virtual):
  _access(access),
  _is_virtual(is_virtual),
  _classdesc(classdesc)
{
}

ParentClass::ParentClass(const ParentClass&p):
  _access(p._access),
  _is_virtual(p._is_virtual),
  _classdesc(p._classdesc)
{
}

ParentClass::~ParentClass()
{
}

int ParentClass::is_virtual() const
{
  return _is_virtual;
}

const ClassDesc* ParentClass::classdesc() const
{
  return _classdesc;
}

void ParentClass::change_classdesc(ClassDesc*n)
{
  _classdesc = n;
}

/////////////////////////////////////////////////////////////////

ParentClasses::ParentClasses():
  _n(0),
  _classes(0)
{
}

void
ParentClasses::init(const char* parents)
{
  // if parents is empty then we are done
  if (!parents || strlen(parents) == 0) return;

  char* tokens = ::strcpy(new char[strlen(parents)+1],parents);
  const char* whitesp = "\t\n,() ";
  char* token;
  int is_virtual = 0;
  ParentClass::Access access = ParentClass::Private;
  for (token = ::strtok(tokens,whitesp);
       token;
       token = ::strtok(0,whitesp)) {
      if (!strcmp(token,"virtual")) {
          is_virtual = 1;
        }
      else if (!strcmp(token,"public")) {
          access = ParentClass::Public;
        }
      else if (!strcmp(token,"protected")) {
          access = ParentClass::Protected;
        }
      else if (!strcmp(token,"private")) {
          access = ParentClass::Private;
        }
      else {
          ClassKey parentkey(token);
          // if the parents class desc does not exist create a temporary
          // the temporary will be incorrect,because it does not have the
          // parent's parents   ' for braindead compiler
          if (!ClassDesc::all()[parentkey]) {
              ClassDesc::all()[parentkey] = new ClassDesc(token);
              if (ClassDesc::unresolved_parents_ == 0) {
                  ClassDesc::unresolved_parents_ = new SETCTOR;
                }
              ClassDesc::unresolved_parents_->add(token);
            }
          ParentClass* p = new ParentClass(ClassDesc::all()[parentkey],
                                           access,
                                           is_virtual);
          add(p);
          access = ParentClass::Private;
          is_virtual = 0;
        }
    }
  delete[] tokens;
  
}

ParentClasses::~ParentClasses()
{
  for (int i=0; i<_n; i++) delete _classes[i];
  
  if (_classes) delete[] _classes;
  _classes = 0;
  _n = 0;
}

void
ParentClasses::add(ParentClass*p)
{
  ParentClass** newpp = new ParentClass*[_n+1];
  for (int i=0; i<_n; i++) newpp[i] = _classes[i];
  newpp[_n] = p;
  _n++;
  delete[] _classes;
  _classes = newpp;
}

void
ParentClasses::change_parent(ClassDesc*oldcd,ClassDesc*newcd)
{
  for (int i=0; i<_n; i++) {
      if (parent(i).classdesc() == oldcd) parent(i).change_classdesc(newcd);
    }
}

////////////////////////////////////////////////////////////////////////

ARRAY_def(ClassDescP);
SET_def(ClassDescP);
ARRAYSET_def(ClassDescP);

ARRAY_def(CClassDescP);
SET_def(CClassDescP);
ARRAYSET_def(CClassDescP);

ClassDesc::ClassDesc(char* name, int version,
                     char* parents,
                     DescribedClass* (*ctor)(),
                     DescribedClass* (*keyvalctor)(const RefKeyVal&),
                     DescribedClass* (*stateinctor)(StateIn&)
                     ):
  classname_(0),
  version_(version),
  children_(0),
  ctor_(ctor),
  keyvalctor_(keyvalctor),
  stateinctor_(stateinctor)
{
  // make sure that the static members have been initialized
  if (!all_) {
      all_ = new MAPCTOR;
      const char* tmp = getenv("LD_LIBRARY_PATH");
      if (tmp) {
          // Needed for misbehaving getenv's.
          if (strncmp(tmp, "LD_LIBRARY_PATH=", 16) == 0) {
              tmp = strchr(tmp,'=');
              tmp++;
            }
        }
      else tmp = ".";
      classlib_search_path_ = ::strcpy(new char[strlen(tmp)+1],tmp);
    }
  
  parents_.init(parents);

  classname_ = ::strcpy(new char[strlen(name)+1],name);

  // test the version number to see if it is valid
  if (version <= 0) {
      cerr << "error in ClassDesc ctor: version <= 0" << endl;
      exit(1);
    }

  ClassKey key(name);

  // let each of the parents know that this is a child
  for (int i=0; i<parents_.n(); i++) {
      ClassKey parentkey(parents_[i].classdesc()->name());
      if (!(*all_)[parentkey]->children_)
        (*all_)[parentkey]->children_ = new SETCTOR;
      // let the parents know about the child
#ifndef GNUBUG
      ((*all_)[parentkey]->children_)->add(key);
#else
      ClassKeySet *ppp=(*all_)[parentkey]->children_;
      ppp->add(key);
#endif
    }

  // if this class is aleady in all_, then it was put there by a child
  // preserve children info, destroy the old entry, and put this there
  if ((*all_)[key]) {
      children_ = (*all_)[key]->children_;
      (*all_)[key]->children_ = 0;

      if (!children_) {
          cerr << "ClassDesc: inconsistency in initialization"
               << "--perhaps a duplicated CTOR call" << endl;
          abort();
        }

      // go thru the list of children and correct their
      // parent class descriptors
      for (Pix i=children_->first(); i; children_->next(i)) {
          (*all_)[children_->operator()(i)]->change_parent((*all_)[key],this);
        }

      delete (*all_)[key];
      unresolved_parents_->del(key);
      if (unresolved_parents_->length() == 0) {
          delete unresolved_parents_;
          unresolved_parents_ = 0;
        }
    }
  (*all_)[key] = this;
}

ClassDesc::~ClassDesc()
{
  // remove references to this class descriptor
  if (children_) {
      for (Pix i=children_->first(); i; children_->next(i)) {
          if (all_->contains(children_->operator()(i))) {
              (*all_)[children_->operator()(i)]->change_parent(this,0);
            }
        }
    }
  // delete this ClassDesc from the list of all ClassDesc's
  ClassKey key(classname_);
  all_->del(key);
  // if the list of all ClassDesc's is empty, delete it
  if (all_->length() == 0) {
      delete all_;
      all_ = 0;
      delete[] classlib_search_path_;
      classlib_search_path_ = 0;
    }

  // delete local data
  delete[] classname_;
  if (children_) delete children_;
}

ClassKeyClassDescPMap&
ClassDesc::all()
{
  if (!all_) {
      cerr << "ClassDesc::all(): all not initialized" << endl;
      abort();
    }
  return *all_;
}

ClassDesc*
ClassDesc::name_to_class_desc(const char* name)
{
  ClassKey key(name);
  return (*all_)[key];
}

DescribedClass*
ClassDesc::create() const
{
  if (ctor_) return (*ctor_)();
  return 0;
}

DescribedClass*
ClassDesc::create(const RefKeyVal&keyval) const
{
  DescribedClass* result;
  if (keyvalctor_) {
      result = (*keyvalctor_)(keyval);
    }
  else result = 0;
  return result;
}

DescribedClass*
ClassDesc::create(StateIn&statein) const
{
  if (stateinctor_) return (*stateinctor_)(statein);
  return 0;
}

void
ClassDesc::change_parent(ClassDesc*oldcd,ClassDesc*newcd)
{
  parents_.change_parent(oldcd,newcd);
}

void
ClassDesc::list_all_classes()
{
  cout << "Listing all classes:" << endl;
  for (Pix ind=all_->first(); ind!=0; all_->next(ind)) {
      ClassDesc* classdesc = all_->contents(ind);
      cout << "class " << classdesc->name() << endl;
      ParentClasses& parents = classdesc->parents_;
      if (parents.n()) {
          cout << "  parents:";
          for (int i=0; i<parents.n(); i++) {
              if (parents[i].is_virtual()) {
                  cout << " virtual";
                }
              if (parents[i].access() == ParentClass::Public) {
                  cout << " public";
                }
              else if (parents[i].access() == ParentClass::Protected) {
                  cout << " protected";
                }
              cout << " " << parents[i].classdesc()->name();
            }
          cout << endl;
        }
      ClassKeySet* children = classdesc->children_;
      if (children) {
          cout << "  children:";
          for (Pix pind=children->first(); pind!=0; children->next(pind)) {
              cout << " " << (*children)(pind).name();
            }
          cout << endl;
        }
    }
}

DescribedClass* ClassDesc::create_described_class() const
{
    return create();
}

// Returns 0 for success and -1 for failure.
int
ClassDesc::load_class(const char* classname)
{
  // See if the class has already been loaded.
  if (name_to_class_desc(classname) != 0) {
      return 0;
    }
  
#if HAVE_DLFCN_H
  // make a copy of the library search list
  char* path = new char[strlen(classlib_search_path_) + 1];
  strcpy(path, classlib_search_path_);

  // go through each directory in the library search list
  char* dir = strtok(path,":");
  while (dir) {
      // find the 'classes' files
      char* filename = new char[strlen(dir) + 8 + 1];
      strcpy(filename,dir);
      strcat(filename,"/classes");
      cout << "ClassDesc::load_class looking for \"" << filename << "\""
           << endl;
      FILE* fp = fopen(filename, "r");
      delete[] filename;

      if (fp) {
          // read the lines in the classes file
          const int bufsize = 10000;
          char buf[bufsize];
          while(fgets(buf, bufsize, fp)) {
              if (buf[0] != '\0' && buf[strlen(buf)-1] == '\n') {
                  buf[strlen(buf)-1] = '\0';
                }
              char* lib = strtok(buf," ");
              char* testclassname = strtok(0," ");
              cout << "lib = \"" << lib << "\"" << endl;
              while(testclassname) {
                  cout << "classname = \"" << testclassname << "\"" << endl;
                  if (strcmp(testclassname,classname) == 0) {
                      // found it
                      char* libname = new char[strlen(lib) + strlen(dir) + 2];
                      strcpy(libname, dir);
                      strcat(libname, "/");
                      strcat(libname, lib);
                      // load the libraries this lib depends upon

                      // i should look in the library's .dep file to
                      // get the dependencies, but this makes it a little
                      // difficult to make sure the same library doesn't
                      // get loaded twice (which is important) so for now
                      // i'll just wait until after i load the library and
                      // then look in the unresolved parents set
                      // and load parents until nothing is left

                      // load the library
                      cout << "loading \"" << libname << "\"" << endl;
                      dlopen(libname, RTLD_LAZY);

                      // load code for parents
                      while (unresolved_parents_
                             && unresolved_parents_->length()) {
                          load_class(unresolved_parents_->operator()
                                     (unresolved_parents_->first()).name());
                        }

                      fclose(fp);
                      delete[] path;
                      // make sure it worked.
                      if (name_to_class_desc(classname) == 0) {
                          cerr << "load of \"" << classname << "\" from \""
                               << libname << "\" failed" << endl;
                          delete[] libname;
                          return -1;
                        }
                      cout << "loaded \"" << classname << "\" from \""
                           << libname << "\"" << endl;
                      delete[] libname;
                      return 0;
                    }
                  testclassname = strtok(0," ");
                }
            }
          fclose(fp);
        }
      
      dir = strtok(0, ":");
    }

  delete[] path;
#endif // HAVE_DLFCN_H

  cerr << "ClassDesc::load_class(\"" << classname << "\"): load failed"
       << endl;

  return -1;
}

////////////////////////////////////////////////////

ClassDesc DescribedClass::class_desc_("DescribedClass");

DescribedClass::DescribedClass()
{
}

DescribedClass::DescribedClass(const DescribedClass&) {}
DescribedClass& DescribedClass::operator=(const DescribedClass&)
{
  return *this;
}

DescribedClass::~DescribedClass()
{
}

const ClassDesc*
DescribedClass::class_desc() const
{
  return &class_desc_;
}

void*
DescribedClass::_castdown(const ClassDesc*cd)
{
  if (cd == &class_desc_) return this;
  return 0;
}

DescribedClass*
DescribedClass::castdown(DescribedClass*p)
{
  return (DescribedClass*) p->_castdown(DescribedClass::static_class_desc());
}

const ClassDesc*
DescribedClass::static_class_desc()
{
  return &class_desc_;
}

const char* DescribedClass::class_name() const
{
    return class_desc()->name();
}

int DescribedClass::class_version() const
{
    return class_desc()->version();
}

///////////////////////////////////////////////////////////////////////
// DCRefBase members

void
DCRefBase::require_nonnull() const
{
  if (parentpointer() == 0) {
      cerr << "RefDescribedClass: needed a nonnull pointer but got null"
           << endl;
      abort();
    }
}

DCRefBase::~DCRefBase()
{
}

void
DCRefBase::warn(const char * msg) const
{
  RefBase::warn(msg);
}

void
DCRefBase::warn_ref_to_stack() const
{
  RefBase::warn_ref_to_stack();
}

void
DCRefBase::warn_skip_stack_delete() const
{
  RefBase::warn_skip_stack_delete();
}

void
DCRefBase::warn_bad_ref_count() const
{
  RefBase::warn_bad_ref_count();
}

void
DCRefBase::ref_info(VRefCount*p, ostream& os) const
{
  RefBase::ref_info(p,os);
}

int
DCRefBase::operator==(const DescribedClass*a) const
{
  return eq(parentpointer(),a);
}

int
DCRefBase::operator!=(const DescribedClass*a) const
{
  return ne(parentpointer(),a);
}

int
DCRefBase::operator>=(const DescribedClass*a) const
{
  return ge(parentpointer(),a);
}

int
DCRefBase::operator<=(const DescribedClass*a) const
{
  return le(parentpointer(),a);
}

int
DCRefBase::operator> (const DescribedClass*a) const
{
  return gt(parentpointer(),a);
}

int
DCRefBase::operator< (const DescribedClass*a) const
{
  return lt(parentpointer(),a);
}

int
DCRefBase::operator==(const DCRefBase &a) const
{
  return eq(parentpointer(),a.parentpointer());
}

int
DCRefBase::operator!=(const DCRefBase &a) const
{
  return ne(parentpointer(),a.parentpointer());
}

int
DCRefBase::operator>=(const DCRefBase &a) const
{
  return ge(parentpointer(),a.parentpointer());
}

int
DCRefBase::operator<=(const DCRefBase &a) const
{
  return le(parentpointer(),a.parentpointer());
}

int
DCRefBase::operator> (const DCRefBase &a) const
{
  return gt(parentpointer(),a.parentpointer());
}

int
DCRefBase::operator< (const DCRefBase &a) const
{
  return lt(parentpointer(),a.parentpointer());
}

void
DCRefBase::check_pointer() const
{
  if (parentpointer() && parentpointer()->nreference() <= 0) {
      warn_bad_ref_count();
    }
}

void
DCRefBase::ref_info( ostream& os) const
{
  DCRefBase::ref_info(parentpointer(),os);
}

void
DCRefBase::reference(VRefCount *p)
{
  if (p) {
      if (DO_REF_CHECK_STACK(p)) {
          DO_REF_UNMANAGE(p);
          warn_ref_to_stack();
        }
      p->reference();
    }
}

void
DCRefBase::dereference(VRefCount *p)
{
  if (p && p->dereference()<=0) {
      delete p;
    }
}

DescribedClass_REF_def(DescribedClass);
ARRAY_def(RefDescribedClass);
SET_def(RefDescribedClass);
ARRAYSET_def(RefDescribedClass);


/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ"
// End:
