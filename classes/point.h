#ifndef _POINT_H
#define _POINT_H

#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include "slump.h"

using std::ostream;
using namespace std;

/*!
 * \brief Cartesian coordinates
 * \author Mikael Lund
 * \date 2002-2007
 */
class point {
  public:
    double x,y,z;                       ///< The coordinates
    point();                            ///< Constructor, zero data.
    void clear();                       ///< Zero all data.
    double len(); 
    inline double sqdist(point &);      //!< Squared distance to another point
    inline double sqdist(point &,       //!< -- / / -- 3D minimum image
                        double &, double &);
    inline double dist(point &);        ///< Distance to another point
    inline double dist(point &, double &, double &);        ///< Distance to another point
    double dot(point &);                ///< Angle with another point
    point operator-();                  ///< Sign reversal
    point operator*(point);             ///< Multiply two vectors
    point operator*(double);            ///< Scale vector
    point operator+(point);             ///< Add two vectors
    point operator-(point);             ///< Substract vector
    point operator+(double);            ///< Displace x,y,z by value
    void operator+=(point);
    friend ostream &operator<<(ostream &, point &); /// Print x,y,z
    string str();
};

/*!
 * \brief Class for particles
 * \author Mikael Lund
 * \date 2002-2007
 *
 * Example\n
 * \code
 * vector<particle> p(2);
 * p[0].radius = 2.0;
 * p[1].z = 10;
 * p[0].overlap( p[1] ); --> false
 * \endcode
 */
class particle : public point {
  public:
    particle();
   
    //! Particle type identifier
    enum type {FIRST=0,GLY,ALA,VAL,LEU,ILE,PHE,TRP,TYR,HIS,SER,THR,MET,CYS,
      ASP,GLN,GLU,ASN,LYS,ARG,PRO,UNK,NTR,CTR,NA,K,CL,BR,I,SO4,ION,CATION,ANION,GHOST,
      RNH3,RNH4,RCOOH,RCOO,LAST}; 

    double charge;                      //!< Charge number
    double radius;                      //!< Radius
    float mw;                           //!< Molecular weight
    type id;                            //!< Particle identifier
    inline bool overlap(particle &);    //!< Hardsphere overlap test
    inline double potential(point &);   //!< Electric potential in point
    double vol(double=1);               //!< Estimate volume from weight
    double rad(double=1);               //!< Estimate radius from weight           
    void operator=(point);              //!< Copy coordinates from a point
};

/*! \brief Class for spherical coordinates
 *  \author Mikael Lund
 *  \date Canberra, 2005-2006
 */
class spherical : private slump {
 public:
  double r,     //!< Radial
         theta, //!< Zenith angle \f$[0:\pi]\f$
         phi;   //!< Azimuthal angle \f$[0:2\pi]\f$
  spherical(double=0,double=0,double=0);
  point cartesian();                            //!< Convert to cartesian coordinates
  void operator=(point &);                      //!< Convert from cartesian coordinates
  void random_angles();                         //!< Randomize angles
};

inline void spherical::operator=(point &p) {
  r=p.len();
  theta=acos(p.z/r);
  phi=asin( p.y/sqrt(p.x*p.x+p.y*p.y) );
  if (p.x<0)
    phi=acos(-1.) - phi;
}

inline point spherical::cartesian() {
  point::point p;
  p.x=r*sin(theta)*cos(phi);
  p.y=r*sin(theta)*sin(phi);
  p.z=r*cos(theta);
  return p;
}

//! \todo This function is not completed
inline void spherical::random_angles() {
  r=1.0 ;
}

/*!
 * \return \f$ |r_{12}|^2 = \Delta x^2 + \Delta y^2 + \Delta z^2 \f$
 */
inline double point::sqdist(point &p) {
  register double dx,dy,dz;
  dx=x-p.x;
  dy=y-p.y;
  dz=z-p.z;
  return dx*dx + dy*dy + dz*dz;
}

inline double point::sqdist(point &p, double &len, double &inv_len) {
  register double dx,dy,dz;
  dx=x-p.x;
  dy=y-p.y;
  dz=z-p.z;
  dx-=len*floor(dx*inv_len+.5);
  dy-=len*floor(dy*inv_len+.5);
  dz-=len*floor(dz*inv_len+.5);
  return dx*dx + dy*dy + dz*dz;
}

inline double point::dist(point &p) { return sqrt(sqdist(p)); }
inline double point::dist(point &p, double &len, double &inv_len) { 
  return sqrt(sqdist(p, len, inv_len)); }

/*!
 * \return \f$ \phi = \frac{z}{r_{12}}\f$
 * \note Not multiplied with the Bjerrum length!
 */
inline double particle::potential(point &p) { return charge / dist(p); }

/*!
 * \return True if \f$ r_{12}<(\sigma_1+\sigma_2)/2 \f$ - otherwise false.
 */
inline bool particle::overlap(particle &p) {
  double r=radius+p.radius;
  return (sqdist(p) < r*r) ? true : false;
}

#endif
