/*---------------------------------------------------------------------------~*
 * Copyright (c) 2017 Los Alamos National Security, LLC
 * All rights reserved.
 *---------------------------------------------------------------------------~*/

#ifndef WEIGHT_H_INC_
#define WEIGHT_H_INC_

#include <cmath>
#include <array>
#include <cassert>
#include <limits>

#include "portage/support/Point.h"

namespace Portage {
namespace Meshfree {
namespace Weight {

using std::pow;
using std::abs;
using std::array;
using std::numeric_limits;

//\///////////////////////////////////////////////////////////////////////////
// constants
//\///////////////////////////////////////////////////////////////////////////

/// usual constant
const double Pi = 3.141592653589793;

/// normalization constants for cubic B-spline:
/** linear, cylindrical, and spherical */
const double normconst[4] = {2./3., 1./(.7*Pi), 1./Pi, 1./Pi};

//\///////////////////////////////////////////////////////////////////////////
// math functions
//\///////////////////////////////////////////////////////////////////////////

/// scalar sign function
inline double Sign(double x) {
  double result;
  if(x>0.) result =  1.;
  else if(x<0.) result =  -1.;
  else result= 0.;
  return result;
}

/// scalar step function
inline double UnitStep(double x){
  return .5*(1.+Sign(x));
}

/// translation of scalar exponentiation from Mathematica
inline double Power(double x, double y){double z; z=pow(x,y); return z;}

/// translation of scalar exponentiation from Mathematica
inline double Power(double x, int y){double z; z=pow(x,static_cast<double>(y)); return z;}

/// translation of scalar absolute value from Mathematica
inline double Abs(double x){double y; y=abs(x); return y;}

//\///////////////////////////////////////////////////////////////////////////
// various scalar kernels and derivatives
//\///////////////////////////////////////////////////////////////////////////

/// scalar cubic b-spline
inline double b4(double x){
  double y=
      (Power(2. - Abs(x),3)*UnitStep(2. - Abs(x))*UnitStep(-1. + Abs(x)))/4. +
      (1. - 1.5*Power(x,2) + 0.75*Power(Abs(x),3))*UnitStep(1. - Abs(x));
  return y;
}

/// scalar cubic b-spline derivative
inline double db4(double x){
  double y=
      (-3.*x + (9.*Power(x,2)*Sign(x))/4.)*UnitStep(1. - Abs(x)) -
      (3.*Power(2. - Abs(x),2)*Sign(x)*UnitStep(2. - Abs(x))*
          UnitStep(-1. + Abs(x)))/4.;
  return y;
}

/// scalar cubic b-spline second derivative
inline double ddb4(double x){
  double y=
      (-3. + (9.*x*Sign(x))/2.)*UnitStep(1. - Abs(x)) +
      (3.*(2. - Abs(x))*UnitStep(2. - Abs(x))*
          UnitStep(-1. + Abs(x)))/2.;
  return y;
}

/// scalar cubic b-spline anti-derivative
inline double ib4(double x){
  double y=
      UnitStep(-2. + x) + 0.6666666666666666*
      ((1.4375 + (0.25 - Power(-2. + x,4)/4.)/4.)*
          UnitStep(2. - x)*UnitStep(-1. + x) +
          (0.75 + x - Power(x,3)/2. + (3.*Power(x,4))/16.)*
          UnitStep(1. - x)*UnitStep(x) +
          (0.75 + x - Power(x,3)/2. - (3.*Power(x,4))/16.)*
          UnitStep(-x)*UnitStep(1. + x) +
          (Power(2. + x,4)*UnitStep(-1. - x)*UnitStep(2. + x))/16.);
  return y;
}

/// scalar left half cubic b-spline
inline double b4lh(double x){
  double y=
      0.0625*Power(2. - Abs(x),3)*(1. + Sign(-1. - x))*
      (1. + Sign(2. + x)) +
      0.5*(1. - (3.*Power(x,2))/2. + (3.*Power(Abs(x),3))/4.)*
      (1. + Sign(1. + x))*UnitStep(-x);
  return y;
}

/// scalar right half cubic b-spline
inline double b4rh(double x){
  double y=
      0.0625*Power(2. - Abs(x),3)*(1. + Sign(2. - x))*
      (1. + Sign(-1. + x)) +
      0.5*(1. - (3.*Power(x,2))/2. + (3.*Power(Abs(x),3))/4.)*
      (1. + Sign(1. - x))*UnitStep(x);
  return y;
}

/// scalar epanechnikov kernel
inline double epanechnikov(double x) {
  double y= .375*(1.-x*x*.25);
  if (fabs(x)>=2.) y=0.;
  return y;
}

/// scalar epanechnikov kernel derivative
inline double depanechnikov(double x) {
  double y=  -.1875*x;
  if (fabs(x)>=2.) y=0.;
  return y;
}

/// scalar epanechnikov kernel second derivative
inline double ddepanechnikov(double x) {
  double y= -.1875;
  if (fabs(x)>=2.) y=0.;
  return y;
}

/// scalar square kernel
inline double square(double x) {
  double y= Abs(x);
  if (y<=2.) return 1.;
  else return 0.;
}

/// scalar square kernel derivative
inline double dsquare(double x) {
  return 0.;
}

/// scalar square kernel second derivative
inline double ddsquare(double x) {
  return 0.;
}

/// scalar smooth ramp for faceted weight
inline double polyramp(double x){
  double y;
  y=((1.5 - x)*(1. + Sign(1. - x)))*.5 +
      ((2. + (-2. + .5*x)*x)*(1. + Sign(2. - x))*(1. + Sign(-1. + x)))*.25;
  return y;
}

/// scalar smooth ramp for faceted weight derivative
inline double dpolyramp(double x){
  double y;
  y=((-1.)*(1. + Sign(1. - x)))*.5 +
      ((- 2. + x)*(1. + Sign(2. - x))*(1. + Sign(-1. + x)))/4.;
  return y;
}

/// scalar smooth ramp for faceted weight derivative
inline double ddpolyramp(double x){
  double y;
  y=((0.)*(1. + Sign(1. - x)))*.5 +
      ((1.)*(1. + Sign(2. - x))*(1. + Sign(-1. + x)))/4.;
  return y;
}

/// inverse square root kernel and derivatives
inline double invsqrt(double x){
  double ax,y;
  ax = abs(x);
  y= .5*(1. + Sign(2. - ax))*((ax-2.)*ax+4.)*
      pow(ax+numeric_limits<double>::epsilon(),-.5);
  return y;
}
inline double dinvsqrt(double x){
  double sx,ax,y;
  sx = x>0?1.:1.;
  ax = abs(x);
  y= .25*(1. + Sign(2. - ax))*sx*((3.*ax-4.)*ax-4.)*
      pow(ax+numeric_limits<double>::epsilon(),-1.5);
  return y;
}
inline double ddinvsqrt(double x){
  double ax,y;
  ax = abs(x);
  y= .125*(1. + Sign(2. - ax))*((3.*ax+4.)*ax+12.)*
      pow(ax+numeric_limits<double>::epsilon(),-2.5);
  return y;
}

/// coulomb weight
inline double coulomb(double x){
  double y;
  y = 1./(abs(x)+numeric_limits<double>::epsilon());
  return y;
}

/// coulomb weight derivative
inline double dcoulomb(double x){
  double y;
  y = -Sign(x)/(x*x+numeric_limits<double>::epsilon());
  return y;
}

/// coulomb weight second derivative
inline double ddcoulomb(double x){
  double y;
  y = 2./(x*x*abs(x)+numeric_limits<double>::epsilon());
  return y;
}

enum Kernel {B4, SQUARE, EPANECHNIKOV, POLYRAMP, INVSQRT, COULOMB};

/// general kernel function
double kernel(const Kernel kern, double x) {
  double result;
  switch (kern) {
    B4:{result = b4(x); break;}
    SQUARE:{result = square(x); break;}
    EPANECHNIKOV:{result = epanechnikov(x); break;}
    POLYRAMP:{result = polyramp(x); break;}
    INVSQRT:{result = invsqrt(x); break;}
    COULOMB:{result = coulomb(x); break;}
    default:
      assert(false);
  }
  return result;
}

//\///////////////////////////////////////////////////////////////////////////
// general multi-dimensional evaluation function - what the public uses
//\///////////////////////////////////////////////////////////////////////////

enum Geometry {ELLIPTIC, TENSOR, FACETED};

/// generic elliptically symmetric weight function argument
template<size_t dim>
double elliptic(Point<dim> x, Point<dim> y, array<double,dim> &h) {
  double distance = 0.0, result;
  for (size_t i=0; i<dim; i++) {
    distance += (x[i]-y[i])*(x[i]-y[i])/(h[i]*h[i]);
  }
  distance = sqrt(distance);
  return distance;
}

/// generic tensor weight function arguments
template<double f(double), size_t dim>
array<double,dim> tensor(Point<dim> x, Point<dim> y, array<double,dim> &h) {
  array<double,dim> result;
  for (size_t i=0; i<dim; i++) {
    result[i] = (x[i]-y[i])/h[i];
  }
  return result;
}

/// evaluation function for elliptic or tensor product weights
template<size_t dim>
double eval(const Geometry geo,
            const Kernel kern,
            const Point<dim> x, const Point<dim> y,
            array<double,dim> h)
{
  double result;
  double norm = kernel(kern, 0.0);
  switch (geo) {
    ELLIPTIC: {
      double arg = spherical(x,y,h);
      result = kernel(kern, arg) / norm;
      break;
    }

    TENSOR:{
      result = 1.;
      array<double,dim> arg = tensor(x,y,h);
      for (size_t i=0; i<dim; i++) {
        result *= kernel(kern, arg[i]) / norm;
      }
      break;
    }
    default:
      throw std::runtime_error("invalid weight geometry");
      return result;
  }
}

/// data for specifying a faceted weight
template<size_t dim>
struct FacetData {
  double smoothing;
  array<double,dim> normal;
};

/// faceted weight function
template<size_t dim>
double faceted(const Point<dim> x, const Point<dim> y,
               FacetData<dim>* facets, size_t nsides)
{
  double result = 1.;
  static double polyramp0 = polyramp(0);
  for (size_t i=0; i<nsides; i++) {
    double arg = 0.;
    for (size_t j=0; j<dim; j++) arg += facets[i].normal[j]*(y[j]-x[j]);
    arg /= facets[i].smoothing;
    result *= polyramp(arg) / polyramp0;
  }
}

}
}
}

#endif // WEIGHT_H_INC_
