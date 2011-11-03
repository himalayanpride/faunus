#include <faunus/move.h>
#include <faunus/space.h>
#include <faunus/slump.h>
#include <faunus/group.h>
#include <faunus/energy.h>
#include <faunus/species.h>
#include <faunus/inputfile.h>
#include <faunus/geometry.h>
#include <faunus/faunus.h>
#include <faunus/textio.h>
#include <faunus/physconst.h>

namespace Faunus {

  namespace Move {

    using namespace textio;

    Movebase::Movebase(Energy::Energybase &e, Space &s, string pfx) {
      pot=&e;
      spc=&s;
      prefix=pfx;
      cnt=cnt_accepted=0;
      dusum=0;
      w=22;
      runfraction=1;
    }

    Movebase::~Movebase() {
    }

    void Movebase::trialMove() {
      assert(spc->geo!=nullptr); // space geometry MUST be set before moving!
      cnt++;
      _trialMove();
    }

    void Movebase::acceptMove() {
      cnt_accepted++;
      _acceptMove();
    }
    
    void Movebase::rejectMove() {
      _rejectMove();
    }
    
    double Movebase::energyChange() {
      return _energyChange();
    }
   
    /*! \note Please try not to overload */
    double Movebase::move() {
      if (!run())
        return 0;
      trialMove();
      double du=energyChange();
      if ( !metropolis(du) ) {
        rejectMove();
        du=0;
      }
      else {
        acceptMove();
        dusum+=du;
      }
      return du;
    }

    bool Movebase::metropolis(const double &du) const {
      if (du>0)
        if ( slp_global.randOne()>std::exp(-du) )
          return false;
      return true;
    }

    bool Movebase::run() const {
      if (slp_global.randOne() < runfraction)
        return true;
      return false;
    }

    void Movebase::test(UnitTest &t) {
      t(prefix+"_acceptance", double(cnt_accepted)/cnt*100 );
      _test(t);
    }

    void Movebase::_test(UnitTest&) {
    }

    string Movebase::info() {
      assert(!title.empty() && "Markov Moves must have a title");
      std::ostringstream o;
      o << header("Markov Move: " + title);
      if (!cite.empty()) {
        o << pad(SUB,w,"More information:") << cite << endl;
      }
      o << pad(SUB,w,"Runfraction") << runfraction*100 << percent << endl;
      if (cnt>0) {
        o << pad(SUB,w,"Number of trials") << cnt << endl;
        o << pad(SUB,w,"Acceptance") << double(cnt_accepted)/cnt*100 << percent << endl;
        o << pad(SUB,w,"Total energy change") << dusum << kT << endl;
      }
      o << _info();
      return o.str();
    }

    // TRANSLATE

    ParticleTranslation::ParticleTranslation(InputMap &in,Energy::Energybase &e, Space &s, string pfx) : Movebase(e,s,pfx) {
      title="Single Particle Translation";
      iparticle=-1;
      igroup=nullptr;
      dir.x=dir.y=dir.z=1;
      w=25;
      in.get(prefix+"_runfraction",0.);
    }

    void ParticleTranslation::setGroup(Group &g) {
      igroup=&g;
      iparticle=-1;
    }

    void ParticleTranslation::setParticle(int i) {
      iparticle=i;
      igroup=nullptr;
    }

    void ParticleTranslation::_trialMove() {
      if (igroup!=nullptr)
        iparticle=igroup->random();
      if (iparticle>-1) {
        double dp = atom[ spc->p[iparticle].id ].dp;
        spc->trial[iparticle].x += dir.x * dp * slp_global.randHalf();
        spc->trial[iparticle].y += dir.y * dp * slp_global.randHalf();
        spc->trial[iparticle].z += dir.z * dp * slp_global.randHalf();
        spc->geo->boundary( spc->trial[iparticle] );
      }
    }

    void ParticleTranslation::_acceptMove() {
      double r2=spc->geo->sqdist( spc->p[iparticle], spc->trial[iparticle] );
      sqrmap[ spc->p[iparticle].id ] += r2;
      accmap[ spc->p[iparticle].id ] += 1;
      spc->p[iparticle] = spc->trial[iparticle];
    }

    void ParticleTranslation::_rejectMove() {
      spc->trial[iparticle] = spc->p[iparticle];
      sqrmap[ spc->p[iparticle].id ] += 0;
      accmap[ spc->p[iparticle].id ] += 0;
    }

    double ParticleTranslation::_energyChange() {
      if (iparticle>-1) {
        assert( spc->geo->collision(spc->p[iparticle])==false && "An accepted particle collides with simulation container.");
        if ( spc->geo->collision( spc->trial[iparticle], Geometry::Geometrybase::BOUNDARY ) )
          return pc::infty;
        return
          pot->i_total(spc->trial, iparticle) - pot->i_total(spc->p, iparticle);
        //pot->i2all(spc->trial, iparticle) - pot->i2all(spc->p, iparticle);
      }
      return 0;
    }

    double ParticleTranslation::move() {
      if (!run())
        return 0;
      if (igroup!=nullptr) {
        double du=0;
        for (int i=0; i<igroup->size(); i++) {
          iparticle = igroup->random(); // set random particle for trialmove()
          if ( atom[spc->p[iparticle].id].dp > 1e-5 )
            du+=Movebase::move();
        }
        iparticle=-1;
        return du;
      } else return Movebase::move();
    }

    string ParticleTranslation::_info() {
      char l=12;
      std::ostringstream o;
      o << pad(SUB,w,"Displacement vector") << dir << endl;
      if (cnt>0) {
        o << endl
          << indent(SUB) << "Individual particle movement:" << endl << endl
          << indent(SUBSUB) << std::left << string(7,' ')
          << setw(l-6) << "dp"
          << setw(l+1) << "Acc. "+percent
          << setw(l+7) << bracket("r"+squared)+"/"+angstrom+squared
          << rootof+bracket("r"+squared)+"/"+angstrom << endl;
        for (auto m : sqrmap) {
          short id=m.first;
          o << indent(SUBSUB) << std::left << setw(7) << atom[id].name
            << setw(l-6) << atom[id].dp;
          o.precision(3);
          o << setw(l) << accmap[id].avg()*100
            << setw(l) << sqrmap[id].avg()
            << setw(l) << sqrt(sqrmap[id].avg()) << endl;
        }
      }
      return o.str();
    }

    RotateGroup::RotateGroup(InputMap &in,Energy::Energybase &e, Space &s, string pfx) : Movebase(e,s,pfx) {
      title="Group Rotation/Translation";
      igroup=nullptr;
      w=30;
      dir.x=dir.y=dir.z=1;
      groupWiseEnergy=false;
      runfraction = in.get<double>(prefix+"_runfraction",1.0);
      dp_trans = in.get<double>(prefix+"_transdp", 2);
      dp_rot   = in.get<double>(prefix+"_rotdp", 100);
      if (dp_rot>4*pc::pi) // no need to rotate more than
        dp_rot=4*pc::pi;   // +/- 2 pi.
      if (dp_rot<1e-6 && dp_trans<1e-6)
        runfraction=0;
    }

    void RotateGroup::setGroup(Group &g) {
      assert(&g!=nullptr);
      igroup=&g;
    }

    void RotateGroup::_trialMove() {
      assert(igroup!=nullptr);
      angle=dp_rot*slp_global.randHalf();
      Point p;
      double r=2;
      while (r>1) {
        p.x=2*slp_global.randHalf(); // random vector
        p.y=2*slp_global.randHalf(); // inside a sphere
        p.z=2*slp_global.randHalf();
        r=sqrt(p.x*p.x+p.y*p.y+p.z*p.z);
      }
      p=igroup->cm+p; // set endpoint for rotation 
      igroup->rotate(*spc, p, angle);
      p.x=dir.x * dp_trans * slp_global.randHalf();
      p.y=dir.y * dp_trans * slp_global.randHalf();
      p.z=dir.z * dp_trans * slp_global.randHalf();
      igroup->translate(*spc, p);
    }

    void RotateGroup::_acceptMove() {
      double r2 = spc->geo->sqdist( igroup->cm, igroup->cm_trial );
      sqrmap_t[ igroup->name ] += r2;
      sqrmap_r[ igroup->name ] += pow(angle*180/pc::pi, 2);
      accmap[ igroup->name ] += 1;
      igroup->accept(*spc);
    }

    void RotateGroup::_rejectMove() {
      sqrmap_t[ igroup->name ] += 0;
      sqrmap_r[ igroup->name ] += 0;
      accmap[ igroup->name ] += 0;
      igroup->undo(*spc);
    }

    double RotateGroup::_energyChange() {
      for (int i=(*igroup).beg; i<=(*igroup).end; i++)  // check for container collision
        if ( spc->geo->collision( spc->trial[i], Geometry::Geometrybase::BOUNDARY ) )
          return pc::infty;
      double uold = pot->g2all(spc->p, *igroup) + pot->g_external(spc->p, *igroup);
      double unew = pot->g2all(spc->trial, *igroup) + pot->g_external(spc->trial, *igroup);
      return unew-uold;
    }

    string RotateGroup::_info() {
      char l=12;
      std::ostringstream o;
      o << pad(SUB,w,"Displacement vector") << dir << endl
        << pad(SUB,w,"Max. translation") << pm << dp_trans/2 << textio::_angstrom << endl
        << pad(SUB,w,"Max. rotation") << pm << dp_rot/2*180/pc::pi << textio::degrees << endl;
      if (cnt>0) {
        o << endl
          << indent(SUB) << "Move Statistics:" << endl << endl
          << indent(SUBSUB) << std::left << setw(20) << "Group name" //<< string(20,' ')
          << setw(l+1) << "Acc. "+percent
          << setw(l+9) << rootof+bracket("dR"+squared)+"/"+angstrom
          << setw(l+5) << rootof+bracket("d"+theta+squared)+"/"+degrees << endl;
        for (auto m : accmap) {
          string id=m.first;
          o << indent(SUBSUB) << std::left << setw(20) << id;
          o.precision(3);
          o << setw(l) << accmap[id].avg()*100
            << setw(l) << sqrt(sqrmap_t[id].avg())
            << setw(l) << sqrt(sqrmap_r[id].avg()) << endl;
        }
      }
      return o.str();
    }

    void RotateGroup::_test(UnitTest &t) {
      for (auto m : accmap) {                                                                                   
        string id=m.first,
               idtrim="_"+textio::trim(id)+"_";
        t(prefix+idtrim+"acceptance", accmap[id].avg()*100);
        t(prefix+idtrim+"dRot", sqrt(sqrmap_r[id].avg()));
        t(prefix+idtrim+"dTrans", sqrt(sqrmap_t[id].avg()));
      }
    }

    Isobaric::Isobaric(InputMap &in, Energy::Hamiltonian &e, Space &s, string pfx) : Movebase(e,s,pfx) {
      title="Isobaric Volume Fluctuations";
      w=30;
      dV = in.get<double>(prefix+"_dV", 0.);
      P = in.get<double>(prefix+"_P", 0.)/1e30*pc::Nav; //pressure mM -> 1/A^3
      runfraction = in.get(prefix+"_runfraction",1.0);
      if (dV<1e-6)
        runfraction=0;
      hamiltonian = &e;
      e.create( Energy::ExternalPressure( e.getGeometry(), P ) );
    }

    string Isobaric::_info() {
      using namespace textio;
      std::ostringstream o;
      const double tomM=1e30/pc::Nav;
      int N,Natom=0, Nmol=0;
      for (auto g : spc->g)
        if (g->id==Group::ATOMIC)
          Natom += g->size();
        else
          Nmol++;
      N = Natom + Nmol;
      double Pascal = P*pc::kB*pc::T*1e30;
      o << pad(SUB,w, "Displacement parameter") << dV << endl
        << pad(SUB,w, "Number of molecules") <<N<< " (" <<Nmol<< " molecular + " <<Natom<< " atomic)" << endl 
        << pad(SUB,w, "Pressure") << P*tomM << " mM" << " = " << Pascal << " Pa = " << Pascal/0.980665e5 << " atm" << endl
        << pad(SUB,w, "Temperature") << pc::T << " K" << endl;
      if (cnt>0) {
        char l=14;
        o << pad(SUB,w, "Mean displacement") << "\u221b"+rootof+bracket("dV"+squared) << " = " << pow(sqrV.avg(), 1/6.) << _angstrom << endl
          << pad(SUB,w, "Osmotic coefficient") << P / (N/V.avg()) << endl
          << endl
          << indent(SUBSUB) << std::right << setw(10) << ""
          << setw(l+5) << bracket("V")
          << setw(l+8) << "\u221b"+bracket("V")
          << setw(l+8) << bracket("N/V") << endl
          << indent(SUB) << setw(10) << "Averages"
          << setw(l) << V.avg() << _angstrom << cubed
          << setw(l) << pow(V.avg(),1/3.) << _angstrom
          << setw(l) << N/V.avg()*tomM << " mM" << endl;
      }
      return o.str();
    }

    void Isobaric::_test(UnitTest &t) {
      t(prefix+"_averageSideLength", pow( V.avg(), 1/3.) );
      t(prefix+"_MSQDisplacement", pow(sqrV.avg(), 1/6.) );
    }

    void Isobaric::_trialMove() {
      assert(spc->g.size()>0 && "Space has empty group vector - NPT move not possible.");
      oldV = spc->geo->getVolume();
      newV = std::exp( std::log(oldV) + slp_global.randHalf()*dV );
      for (auto g : spc->g)
        g->scale(*spc, newV);
    }

    void Isobaric::_acceptMove() {
      V += newV;
      sqrV += pow( oldV-newV, 2 );
      hamiltonian->setVolume(newV);
      for (auto g : spc->g )
        g->accept(*spc);
    }

    void Isobaric::_rejectMove() {
      sqrV += 0;
      V += oldV;
      hamiltonian->setVolume(oldV);
      for (auto g : spc->g )
        g->undo(*spc);
    }

    double Isobaric::_energy(const p_vec &p) {
      double u=0;
      for (size_t i=0; i<spc->g.size()-1; ++i)      // group-group
        for (size_t j=i+1; j<spc->g.size(); ++j)
          u += pot->g2g(p, *spc->g[i], *spc->g[j]);
      for (auto g : spc->g) {
        u += pot->g_external(p, *g);
        if (g->id==Group::ATOMIC)
          u+=pot->g_internal(p, *g);
      }
      return u + pot->external();
    }

    double Isobaric::_energyChange() {
      double uold,unew;
      uold = _energy(spc->p);
      hamiltonian->setVolume( newV );
      for (auto g : spc->g) // In spherical geometries molecules may collide with cell boundary upon scling mass center.
        for (int i=g->beg; i<=g->end; i++)
          if ( spc->geo->collision( spc->trial[i], Geometry::Geometrybase::BOUNDARY ) )
            return pc::infty;
       unew = _energy(spc->trial);
      return unew-uold;
    }

    void bath::add(Group &g) {
      iondata d;
      for (auto &a : atom.list)
        if (a.chempot!=0) {
          d.pos.clear();
          for (int i=g.beg; i<=g.end; i++)
            if (spc->p[i].id==a.id)
              d.pos.push_back(i);
          if (!d.pos.empty()) {
            d.z=(unsigned short)std::abs(a.charge);
            if (a.charge>0)
              cations[a.id]=d;
            if (a.charge<0)
              anions[a.id]=d;
          }
        }
      assert(!cations.empty() && !anions.empty() && "No GC ions found!");
    }

    /*!
     * Remove j'th ion from cation and anion list as well as from the particle
     * vectors in Space. Particle index in the ion lists are reduced if they
     * are larger than j.
     */
    void bath::remove(int j) {
      for (auto &m : cations) {
        if ( std::find(m.second.pos.begin(), m.second.pos.end(), j) != m.second.pos.end() )
            m.second.pos.erase( m.second.pos.begin()+j );
        for (auto &i : m.second.pos)
          if (i>j) i--;
      }
      for (auto &m : anions) {
        if ( std::find(m.second.pos.begin(), m.second.pos.end(), j) != m.second.pos.end() )
            m.second.pos.erase( m.second.pos.begin()+j );
        for (auto &i : m.second.pos)
          if (i>j) i--;
      }
      spc->remove(j);
    }

    /*!
     * Insert trial vector of particles into Space and add their new index to
     * the ion lists.
     */
    void bath::insert() {
      assert( !trial_ins.empty() && "Cannot insert empty salt pair!");
      int cnt=spc->p.size();
      for (auto &t : trial_ins) {
        spc->insert(t);
        if (t.charge>0) cations[t.id].pos.push_back(cnt++);
        if (t.charge<0) anions[t.id].pos.push_back(cnt++);
      }
    }

    void bath::_trialMove() {
      trial_ins.clear();
      trial_del.clear();
      particle pa, pb;
      short ida; //pick by random
      short idb; 
      short Na=cations[ida].z;
      short Nb=anions[idb].z;
      std::random_shuffle( cations[ida].pos.begin(), cations[ida].pos.end());
      std::random_shuffle( anions[idb].pos.begin(), anions[idb].pos.end());
      pa=atom[ida];
      pb=atom[idb];

      //insert -- fill trial vector
      do { trial_ins.push_back(pa); } while (Na-->0);
      do { trial_ins.push_back(pb); } while (Nb-->0);
      for (auto &p : trial_ins)
        spc->geo->randompos(p);
    }

    void bath::_acceptMove() {
      // if insertion
      insert();

      // if delete
      for (auto i : trial_del)
        remove(i);
    }

    double bath::_energyChange() {
      double u=0;
      if (action) {
        u+=pot->v2v(spc->p, trial_ins);
        for (auto i=trial_ins.begin(); i!=trial_ins.end()-1; i++)
          for (auto j=i+1; j!=trial_ins.end(); j++)
            u+=pot->p2p(*i,*j);
        for (auto i=trial_ins.begin(); i!=trial_ins.end(); i++)
          u+=pot->p_external(*i);
      }
      else {
        for (auto i=trial_del.begin(); i!=trial_del.end(); i++)
          u+=pot->i_total(spc->p, *i);
        for (auto i=trial_del.begin(); i!=trial_del.end()-1; i++)
          for (auto j=i+1; j!=trial_del.end(); j++)
            u-=pot->i2i(spc->p, *i, *j);
      }
      return u;
    }

  }//namespace
}//namespace
