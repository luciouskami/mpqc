
#ifndef _math_isosurf_triangle_h
#define _math_isosurf_triangle_h

#ifdef __GNUC__
#pragma interface
#endif

#include <math/isosurf/edge.h>

class Triangle: public VRefCount {
  protected:
    unsigned int _orientation[3];
    RefEdge _edges[3];
  public:
    Triangle(RefEdge v1, RefEdge v2, RefEdge v3, unsigned int orient0 = 0);
    inline RefEdge edge(int i) { return _edges[i]; };
    virtual ~Triangle();
    void add_edges(SetRefEdge&);
    void add_vertices(SetRefVertex&);

    // returns the surface area element
    // 0<=r<=1, 0<=s<=1, 0<=r+s<=1
    // RefVertex is the intepolated vertex (both point and gradient)
    virtual double interpolate(double r,double s,RefVertex&v);

    // returns a vertex in the triangle
    // i = 0 is the (0,0) vertex
    // i = 1 is the (r=0,s=1) vertex
    // i = 2 is the (r=1,s=0) vertex
    RefVertex vertex(int i);

    virtual double area();
};

REF_dec(Triangle);
ARRAY_dec(RefTriangle);
SET_dec(RefTriangle);
ARRAYSET_dec(RefTriangle);

// 10 vertex triangle
class Triangle10: public Triangle {
  protected:
    RefVertex _vertices[10];
  public:
    Triangle10(RefEdge4 v1, RefEdge4 v2, RefEdge4 v3,
               RefVolume vol,
               double isovalue, unsigned int orientation0 = 0);
    virtual ~Triangle10();
    double interpolate(double r,double s,RefVertex&v);
    double area();
};

REF_dec(Triangle10);
ARRAY_dec(RefTriangle10);
SET_dec(RefTriangle10);
ARRAYSET_dec(RefTriangle10);

class TriangleIntegrator {
  private:
    int _n;
    double* _r;
    double* _s;
    double* _w;
  protected:
    void set_r(int i,double r);
    void set_s(int i,double s);
    void set_w(int i,double w);
  public:
    TriangleIntegrator(int n);
    virtual ~TriangleIntegrator();
    inline double w(int i) { return _w[i]; }
    inline double r(int i) { return _r[i]; }
    inline double s(int i) { return _s[i]; }
    inline int n() { return _n; }
};

class GaussTriangleIntegrator: public TriangleIntegrator {
  public:
    GaussTriangleIntegrator(int order);
    ~GaussTriangleIntegrator();
};

#endif
