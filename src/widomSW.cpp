#include "faunus/widomSW.h"
namespace Faunus {
  widomSW::widomSW(int insertions){
    cnt=0;
    ghostin = insertions;
  }
  void widomSW::add(particle &p) {
    g.push_back(p);
    init();
  }
  bool widomSW::overlap(particle &a, particle &b, container &c) {
    double s=a.radius+b.radius;
    return (c.sqdist(a,b)<s*s) ? true : false;
  }
  void widomSW::init() {
    int gspec=g.size();
    chel.resize(gspec);
    chhc.resize(gspec);
    chex.resize(gspec);
    chtot.resize(gspec);
    ewden.resize(gspec);
    ewnom.resize(gspec);
    chint.resize(gspec);
    expuw.resize(gspec);
    chexw.resize(gspec);
    ihc.resize(gspec);
    irej.resize(gspec);

    for(int i=0;i<gspec;i++){
      chel[i]=0.;
      chhc[i]=0.;
      chex[i]=0.;
      chtot[i]=0.;
      ihc[i]=0.;
      ewden[i].resize(11);
      ewnom[i].resize(11);
      chint[i].resize(11);
      for(int j=0; j<11; j++) {
        ewden[i][j]=0.;
        ewnom[i][j]=0.;
        chint[i][j]=0.;
      }
    }
  }
  void widomSW::insert(container &c, energybase &ip) {
    particle ghost;
    double u,cu;
    cnt++;
    for(int i=0; i < ghostin; i++) {
      c.randompos(ghost);
      int goverlap=0;
      for(int k=0; k < g.size(); k++) {
        ghost.radius = g[k].radius;
        irej[k]=0;
        int j=0;
        while(!overlap(ghost,c.p[j],c) && j<c.p.size()){
          j++;
        }
        if(j != c.p.size()) {
          ihc[k]++;
          irej[k]=1;
          goverlap++;
        }
      }

      if(goverlap!=g.size()) {
        cu=0.;
        u=0.;  //el. potential
        for(int l=0; l < c.p.size(); l++) {
          double invdi=1./c.dist(ghost,c.p[l]);
          cu+=invdi;
          u+=invdi*c.p[l].charge;
        } 
        cu=cu*ip.tokT;
        u=u*ip.tokT;
        double ew,ewla,ewd;
        for(int k=0; k < g.size(); k++) {
          if(irej[k]==0) {
            expuw[k]+=exp(-u*g[k].charge);
            for(int cint=0; cint<11; cint++) {
              ew=g[k].charge*(u-double(cint)*0.1*g[k].charge*cu/double(c.p.size()));
              ewla = ew*double(cint)*0.1;
              ewd=exp(-ewla);
              ewden[k][cint]+=ewd;
              ewnom[k][cint]+=ew*ewd;
            }
          }
        }

      }
    }
  }

  string widomSW::info() {
    std::ostringstream o;
    double aint4,aint2,aint1;
    for(int i=0; i<g.size(); i++) {
      for(int cint=0; cint<11; cint++) {
        if(ewden[i][cint]==0) {
          std::cout << "Widom denominator equals zero" << std::endl;
        } else {
          chint[i][cint]=ewnom[i][cint]/ewden[i][cint];
        }
      }
      aint4=chint[i][1]+chint[i][3]+chint[i][5]+chint[i][7]+chint[i][9];
      aint2=chint[i][2]+chint[i][4]+chint[i][6]+chint[i][8];
      aint1=chint[i][0]+chint[i][10];  
      chel[i]=1./30.*(aint1+2*aint2+4*aint4);
    }

    double cnttot;
    cnttot=cnt*ghostin;
    o << "# SINGLE PARTICLE WIDOM ANALYSIS: (w. charge scaling)" << std::endl
      << "#   Reference:             Mol. Phys. 1988, 64, 247-259" << std::endl
      << "#   Number of Insertions = " << cnttot << std::endl
      << "#   Excess chemical potentials (kT):" << std::endl
      << "#         total   elec.  hs       z     r"<< std::endl;
    for(int i=0; i < g.size(); i++) {
      chhc[i]=-log(double(cnttot-ihc[i])/cnttot);
      chexw[i]=-log(expuw[i]);
      chex[i]=chhc[i]+chel[i];
      o.unsetf( std::ios_base::floatfield );
      o << "#   [" << i << "] "
        << std::setprecision(4)
        << std::setw(8) << chex[i]
        << std::setw(8) << chel[i]
        << std::setw(8) << chhc[i]
        << std::setprecision(2) << std::fixed
        << std::setw(6) << g[i].charge
        << std::setw(6) << g[i].radius << std::endl;
    }
    o << std::endl;
    return o.str();
  }
}