
#ifdef __GNUC__
#pragma implementation
#endif

#include <stdio.h>
#include <stdlib.h>
#include <tmpl.h>
#include <math/array/math_lib.h>
#include <chemistry/qc/intv2/atoms.h>
#include <chemistry/qc/intv2/int_macros.h>
#include <chemistry/qc/intv2/int_flags.h>
#include <chemistry/qc/intv2/int_types.h>

#include <chemistry/qc/intv2/storage.h>
#include <chemistry/qc/intv2/inter.h>
#include <chemistry/qc/intv2/bounds.h>

#include <chemistry/qc/intv2/bounds.gbl>

#define PRINT_STORED 0

#ifdef __GNUC__
// instantiate the templates needed for integral storage
template class EAVLList<IntegralLink,IntegralKey>;
template class EAVLList<IntegralLink,int>;
#endif

/////////////////////////////////////////////////////////////////////////
// IntegralLink members

IntegralLink::IntegralLink(IntegralKey& key, int cost, int size_):
  intlist(key),
  costlist(cost),
  size(size_)
{
}

void*
IntegralLink::operator new(size_t size, int intsize)
{
  return malloc(size + intsize*sizeof(double));
}

void
IntegralLink::operator delete(void* ptr)
{
  free(ptr);
}

void
IntegralLink::print()
{
  printf("(%d %d|%d %d)[%d%d%d]",
         intlist.key.sh0(),
         intlist.key.sh1(),
         intlist.key.sh2(),
         intlist.key.sh3(),
         intlist.key.p12(),
         intlist.key.p34(),
         intlist.key.p13p24());
}

IntegralLink::~IntegralLink()
{
}

//////////////////////////////////////////////////////////////////////////
// IntegralStorer members

DescribedClass_REF_def(IntegralStorer);

#define CLASSNAME IntegralStorer
#define PARENTS public DescribedClass
#define HAVE_CTOR
#define HAVE_KEYVAL_CTOR
#include <util/class/classi.h>
void *
IntegralStorer::_castdown(const ClassDesc*cd)
{
  void* casts[1];
  casts[0] = DescribedClass::_castdown(cd);
  return do_castdowns(casts,cd);
}

IntegralStorer::IntegralStorer()
{
  table_size_ = 1597;
  table_ = new EAVLList<IntegralLink,IntegralKey>[table_size_];
  init(0);
}

IntegralStorer::~IntegralStorer()
{
  init(0);
  delete[] table_;
}

IntegralStorer::IntegralStorer(const RefKeyVal&keyval)
{
  table_size_ = keyval->intvalue("table_size");
  if (table_size_ <= 0) table_size_ = 1597;
  table_ = new EAVLList<IntegralLink,IntegralKey>[table_size_];

  int n_integral = keyval->intvalue("n_integral");
  init(n_integral);
}

void
IntegralStorer::store(IntegralKey &key,
                      int size, int cost, int actualsize)
{
  store(key, int_buffer, size, cost, actualsize);
}

void
IntegralStorer::store(IntegralKey &key, const double *buf,
                      int size, int cost, int actualsize)
{
  IntegralLink *link = new(size) IntegralLink(key, cost, size);

  int i;
  double *buffer = link->buffer();
  for (i=0; i<link->size; i++) {
      buffer[i] = buf[i];
    }

#if PRINT_STORED
  printf("+++++ %d %d %d %d, %d %d %d size %5d cost %7d at 0x%x slot %5d\n",
         key.sh0(),key.sh1(),key.sh2(),key.sh3(),
         key.p12(), key.p34(), key.p13p24(),
         link->size,link->costlist.key,link,link->hash()%table_size_);
#endif

  currentsize_ += actualsize;
  n_integrals_ += size;
  n_shellquart_++;

  // if the table has grown too big, remove some of the members
  while (currentsize_ > maxsize_) {
      IntegralLink *eliminate = costlist.start();
      currentsize_ -= eliminate->actualsize();
      costlist.remove(eliminate);
      table_[eliminate->hash()%table_size_].remove(eliminate);
      n_shellquart_--;
      n_integrals_ -= eliminate->size;
#if PRINT_STORED
      printf("----- %d %d %d %d, %d %d %d size %5d cost %7d at 0x%x slot %5d\n",
             eliminate->intlist.key.sh0(),eliminate->intlist.key.sh1(),
             eliminate->intlist.key.sh2(),eliminate->intlist.key.sh3(),
             eliminate->intlist.key.p12(), eliminate->intlist.key.p34(),
             eliminate->intlist.key.p13p24(),
             eliminate->size, eliminate->costlist.key,
             eliminate,
             eliminate->hash()%table_size_);
#endif
      delete eliminate;
    }

  // add the new shell quartet
  costlist.insert(link);
  table_[link->hash()%table_size_].insert(link);
}

int
IntegralStorer::should_store(int cost, int actualsize)
{
  int neededsize = actualsize - (maxsize_ - currentsize_);
  if (neededsize < 0) neededsize = 0;

  IntegralLink *i;
  int accumsize = 0;
  int accumcost = 0;
  for (i=costlist.start(); i; costlist.next(i)) {
      if (accumsize >= neededsize || accumcost >= cost) break;
      accumsize += i->actualsize();
      accumcost += i->cost();
    }
  //printf("should_store: asize = %5d nsize = %5d acost = %7d cost = %7d\n",
  //       accumsize, neededsize, accumcost, cost);
  if (accumsize >= neededsize) {
      if (accumcost < cost) return 1;
      return 0;
    }
  return 0;
}

IntegralLink*
IntegralStorer::find(IntegralKey& key)
{
  int hash = IntegralLink::shells_to_hash(key.sh0(),
                                          key.sh1(),
                                          key.sh2(),
                                          key.sh3());
  EAVLList<IntegralLink,IntegralKey> &list = table_[hash % table_size_];
  IntegralLink* link = list.find(key);
  return link;
}

void
IntegralStorer::init(int nbytes)
{
  costlist.initialize(&IntegralLink::costlist);
  for (int i=0; i<table_size_; i++) {
      table_[i].initialize(&IntegralLink::intlist);
    }
  done();
  maxsize_ = nbytes;
}

void
IntegralStorer::done()
{
  n_shellquart_ = 0;
  n_integrals_ = 0;
  currentsize_ = 0;
  maxsize_ = 0;

  int i;
  for (i=0; i<table_size_; i++) {
      table_[i].clear_without_delete();
    }
  costlist.clear();
}

void
IntegralStorer::print_stats()
{
  if (currentsize_ == 0) return;
  printf("IntegralStorer: n_integrals  = %d\n", n_integrals_);
  printf("IntegralStorer: n_shellquart = %d\n", n_shellquart_);
  printf("IntegralStorer: currentsize = %d\n", currentsize_);
  printf("IntegralStorer: maxsize     = %d\n", maxsize_);
#if 0
  printf("IntegralStorer: hash table distribution:\n");
  int tlength = 0;
  int i;
  for (i=0; i<table_size_; i++) {
      int length = table_[i].length();
      tlength += length;
      printf(" %5d", length);
      if ((i+1)%13 == 0) printf("\n");
    }
  if (i%13 != 0) printf("\n");
  printf("IntegralStorer: hash table length sum = %d\n", tlength);
  printf("IntegralStorer: cost list length      = %d\n", costlist.length());
#endif
}

/////////////////////////////////////////////////////////////////////////
// C language interface to a global integral storer

static RefIntegralStorer storer;

void
int_storage(int size)
{
  storer = new IntegralStorer();
  storer->init(size);
  if (size) int_integral_storage = 1;
  else int_integral_storage = 0;
}

void
int_reduce_storage_threshold()
{
}

void
int_done_storage()
{
  if (storer.nonnull()) {
      storer->done();
    }
  int_integral_storage = 0;
}

int
int_have_stored_integral(int sh1,int sh2,int sh3,int sh4,
                         int p12,int p34,int p13p24)
{
  IntegralKey key(sh1,sh2,sh3,sh4,p12,p34,p13p24);
  IntegralLink *integral = storer->find(key);

  if (!integral) return 0;

#if PRINT_STORED
  if (sh1 != integral->intlist.key.sh0()
      ||sh2 != integral->intlist.key.sh1()
      ||sh3 != integral->intlist.key.sh2()
      ||sh4 != integral->intlist.key.sh3()
      ||p12 != integral->intlist.key.p12()
      ||p34 != integral->intlist.key.p34()
      ||p13p24 != integral->intlist.key.p13p24()) {
      printf("!!!!! SHELL INFO INCONSISTENCY\n");
      abort();
    }
  printf("===== %d %d %d %d, %d %d %d size %5d cost %7d at 0x%x slot %5d\n",
         sh1, sh2, sh3, sh4,
         p12, p34, p13p24,
         integral->size, integral->costlist.key,
         integral, integral->hash()%storer->table_size());
#endif

  int i;
  double *buffer = integral->buffer();
  for (i=0; i<integral->size; i++) {
      int_buffer[i] = buffer[i];
    }
  
  return 1;
}

void
int_store_integral(int sh1,int sh2,int sh3,int sh4,
                   int p12,int p34,int p13p24,
                   int size)
{
  // the cost of the integral is the time to evaluate it
  // times the number of times it is needed
  // divided by the amount of memory required to store it
  int cost;
  if (int_Qvec) cost = int_erep_4bound(sh1,sh2,sh3,sh4) + 30;
  else cost = 1;
  if (cost <= 0) return;
  cost *=  int_shell1->nprim
         * int_shell2->nprim
         * int_shell3->nprim
         * int_shell4->nprim
         * size
         * 1024; // the 1024 is arbitrary
  int actualsize = IntegralLink::size_to_actualsize(size);
  cost /= actualsize;

  if (storer->should_store(cost, actualsize)) {
      IntegralKey key(sh1,sh2,sh3,sh4,p12,p34,p13p24);
      storer->store(key,size,cost,actualsize);
    }
}
