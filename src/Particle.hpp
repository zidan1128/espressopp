/*
  Copyright (C) 2012,2013
      Max Planck Institute for Polymer Research
  Copyright (C) 2008,2009,2010,2011
      Max-Planck-Institute for Polymer Research & Fraunhofer SCAI

  This file is part of ESPResSo++.

  ESPResSo++ is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  ESPResSo++ is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// ESPP_CLASS
#ifndef _PARTICLE_HPP
#define _PARTICLE_HPP

#include "types.hpp"
#include "Single.hpp"
#include "Triple.hpp"
#include "Quadruple.hpp"
//#include <vector>
#include <boost/serialization/access.hpp>
#include <boost/serialization/list.hpp>
#include <boost/mpi.hpp>
#include "esutil/ESPPIterator.hpp"
#include "Real3D.hpp"
#include "Int3D.hpp"
#include <map>

namespace espressopp {

  struct ParticleProperties {
    size_t id;
    size_t type;
    real mass;
    real q;
    real lambda;
    real drift;
    real lambdaDeriv;
    int state;
    int res_id;

    int incr_state;

    static void registerPython();

    void init() {
      id = 0;
      type = 0;
      mass = 0.0;
      q = 0.0;
      lambda = 0.0;
      lambdaDeriv = 0.0;
      state = 0;
      res_id = 0;

      incr_state = 0;

      change_flag = 0;
    }

    enum ChangeFlags {
      CHANGE_TYPE=1,  // 0
      CHANGE_MASS=2,  // 1
      CHANGE_Q=4,  // 2
      CHANGE_STATE=8,  // 3
      CHANGE_RESID=16,  // 4
      CHANGE_LAMBDA=32,  // 5
      INCR_STATE=64  // 6
    };

    void setType(size_t t) {
      type = t;
      change_flag |= CHANGE_TYPE;
    }

    void setMass(real m) {
      mass = m;
      change_flag |= CHANGE_MASS;
    }

    void setQ(real q_) {
      q = q_;
      change_flag |= CHANGE_Q;
    }

    void setState(int s) {
      state = s;
      change_flag |= CHANGE_STATE;
    }

    void setIncrState(int s) {
      incr_state = s;
      change_flag |= INCR_STATE;
    }

    void setResId(int rs) {
      res_id = rs;
      change_flag |= CHANGE_RESID;
    }

    void setLambda(real l) {
      lambda = l;
      change_flag |= CHANGE_LAMBDA;
    }

    bool updateParticleProperties(Particle *p);
    bool hasChanges() { return change_flag != 0; }

    friend bool operator==(const ParticleProperties &l, const ParticleProperties &r) {
      return (l.type == r.type &&
              l.mass == r.mass &&
              l.q == r.q &&
              l.state == r.state &&
              l.res_id == r.res_id &&
              l.lambda == r.lambda &&
              l.incr_state == r.incr_state &&
              l.change_flag == r.change_flag);
    }

  private:
    int change_flag;

    friend class boost::serialization::access;
    template< class Archive >
    void serialize(Archive &ar, const unsigned int version)
    {
      ar & id;
      ar & type;
      ar & mass;
      ar & q;
      ar & lambda;
      ar & drift;
      ar & lambdaDeriv;
      ar & state;
      ar & res_id;
      ar & change_flag;
      ar & incr_state;
    }
  };

  /**
   * \brief position-like properties
   *
   * This class contains all properties of a particle that behave like
   * positions. Further extensions might be orientations. This is useful
   * to classify how properties behave e.g. on communication.
   */
  struct ParticlePosition {

    Real3D p;
    real radius;
    real extVar;

    void copyShifted(ParticlePosition& dst, const Real3D& shift) const {
      dst.p = p + shift;
    }
  private:
    friend class boost::serialization::access;
    template< class Archive >
    void serialize(Archive &ar, const unsigned int version) {
      for (int i = 0; i < 3; ++i)
        ar & p[i];
      ar & radius;
      ar & extVar;
    }
  };


  /**
   * \brief force-like properties
   *
   * This class contains all properties of a particle that behave like
   * forces. Further extensions might contain torques. This is useful
   * to classify how properties behave e.g. on communication.
   * Important: combiner operatior += must be available
   * to combine results of ghosts with real particles.
   */
  struct ParticleForce {

    Real3D f;
    // force associated with second derivative of particle radius
    real fradius;

    /** add all force type properties of two particles
	(typically used between a real particle and its
	ghost image(s))
    */
    ParticleForce& operator+=(const ParticleForce& otherF) {
      f += otherF.f;
      fradius += otherF.fradius;
      return *this;
    }
  private:
    friend class boost::serialization::access;
    template< class Archive >
    void serialize(Archive &ar, const unsigned int version){
      for (int i = 0; i < 3; ++i)
        ar & f[i];
      ar & fradius;
    }
  };


  /**
   * \brief momentum-like properties
   *
   * This class contains all properties of a particle that behave like
   * a momentum. Further extensions might contain angular momentum. This is useful
   * to classify how properties behave e.g. on communication.
   */
  struct ParticleMomentum {

    Real3D v;
    // force associated with first derivative of particle radius
    real vradius;
  private:
    friend class boost::serialization::access;
    template< class Archive >
    void serialize(Archive &ar, const unsigned int version) {
      for (int i = 0; i < 3; ++i)
        ar & v[i];
      ar & vradius;
    }
  };


  struct ParticleLocal {

    // the image of the particle
    Int3D i;
    bool ghost;
    bool dummy1;
    bool dummy2;
    bool dummy3;
  private:
    friend class boost::serialization::access;
    template< class Archive >
    void serialize(Archive &ar, const unsigned int version) {
      for (int ii = 0; ii < 3; ++ii)
        ar & i[ii];
      ar & ghost;
    }
  };

  struct Particle {

    friend class InBuffer;
    friend class OutBuffer;

    /** bitmask: which extra data elements to in- or exclude from
        ghost sending
    */

    enum ExtraDataElements {
      DATA_PROPERTIES=1,
      DATA_MOMENTUM=2,
      DATA_LOCAL=4
    };

    Particle() { init(); }

    void init() {
      m.v[0] = m.v[1] = m.v[2] = 0.0;
      p.type         = 0;
      p.mass         = 1.0;
      p.q            = 0.0;
      r.radius       = 1.0;
      f.fradius      = 0.0;
      m.vradius      = 0.0;
      l.ghost        = false;
      p.lambda       = 0.0;
      p.drift        = 0.0;      
      p.lambdaDeriv  = 0.0;
      r.extVar       = 0.0;      
      p.state        = 0;
      p.res_id       = 0;
    }

    // getter and setter used for export in Python

    // Properties

    size_t& id() { return p.id; }
    const size_t& id() const { return p.id; }
    longint getId() const { return p.id; }

    size_t& type() { return p.type; }
    const size_t& type() const { return p.type; }
    int getType() const { return p.type; }
    void setType(int type) { p.type = type; }

    real& mass() { return p.mass; }
    const real& mass() const { return p.mass; }
    real getMass() const { return p.mass; }
    void setMass(real mass) { p.mass = mass; }

    real& q() { return p.q; }
    const real& q() const { return p.q; }
    real getQ() const { return p.q; }
    void setQ(real q) { p.q = q; }

    // Radius
    real& radius() { return r.radius; }
    const real& radius() const { return r.radius; }
    real getRadius() const { return r.radius; }
    void setRadius(real q) { r.radius = q; }
    
    // Extended Variable for Generalized Langevin Friction
    real& extVar() { return r.extVar; }
    const real& extVar() const { return r.extVar; }
    real getExtVar() const { return r.extVar; }
    void setExtVar(real q) { r.extVar = q; }

    // Position
    Real3D& position() { return r.p; }
    const Real3D& position() const { return r.p; }
    Real3D getPos() const { return r.p; }
    void setPos(const Real3D& pos) { r.p = pos; }

    // All Forces
    ParticleForce& particleForce() { return f; }
    const ParticleForce& particleForce() const { return f; }

    // Force
    Real3D& force() { return f.f; }
    const Real3D& force() const { return f.f; }

    Real3D getF() const { return f.f; }
    void setF(const Real3D& force) { f.f = force; }

    real& fradius() {return f.fradius; }
    const real& fradius() const { return f.fradius; }

    real getFRadius() const { return f.fradius; }
    void setFRadius(const real &fr) { f.fradius = fr; }

    // Momentum
    Real3D& velocity() { return m.v; }
    const Real3D& velocity() const { return m.v; }
    Real3D getV() const { return m.v; }
    void setV(const Real3D& velocity) { m.v = velocity; }

    real& vradius() { return m.vradius; }
    const real& vradius() const { return m.vradius; }

    real getVRadius() const { return m.vradius; }
    void setVRadius(const real& vr) { m.vradius = vr;}

    // Image, Ghost
    Int3D& image() { return l.i; }
    const Int3D& image() const { return l.i; }
    Int3D getImageBox() const { return l.i; }
    void setImageBox(const Int3D& img) { l.i = img; }

    bool& ghost() { return l.ghost; }
    const bool& ghost() const { return l.ghost; }
    bool getGhostStatus() const { return l.ghost; }
    void setGhostStatus(const bool& gs) { l.ghost = gs; }

    // weight/lambda (used in H-Adress)
    real& lambda() { return p.lambda; }
    const real& lambda() const { return p.lambda; }
    real getLambda() const { return p.lambda; }
    void setLambda(const real& _lambda) { p.lambda = _lambda; }
    
    // drift (used in H-Adress)
    real& drift() { return p.drift; }
    const real& drift() const { return p.drift; }
    real getDrift() const { return p.drift; }
    void setDrift(const real& _drift) { p.drift = _drift; }
    
    // weight/lambda derivative (used in H-Adress)
    real& lambdaDeriv() { return p.lambdaDeriv; }
    const real& lambdaDeriv() const { return p.lambdaDeriv; }
    real getLambdaDeriv() const { return p.lambdaDeriv; }
    void setLambdaDeriv(const real& _lambdaDeriv) { p.lambdaDeriv = _lambdaDeriv; }

    // state (used in AssociationReaction)
    int& state() { return p.state; }
    const int& state() const { return p.state; }
    int getState() const { return p.state; }
    void setState(const int& _state) { p.state = _state; }

    // res_id (eg. define the id of the polymer chain)
    int& res_id() { return p.res_id; }
    const int& res_id() const { return p.res_id; }
    int getResId() const { return p.res_id; }
    void setResId(const int& _res_id) { p.res_id = _res_id; }

    static void registerPython();

    void copyAsGhost(const Particle& src, int extradata, const Real3D& shift) {

      src.r.copyShifted(r, shift);
      if (extradata & DATA_PROPERTIES) {
        p = src.p;
      }
      if (extradata & DATA_MOMENTUM) {
        m = src.m;
      }
      if (extradata & DATA_LOCAL) {
        l = src.l;
      }
      ghost() = 1;
    }

    bool operator<(Particle other) const {
      return p.id < other.id();
    }

    bool operator<(const Particle &other) {
      return p.id < other.id();
    }

    bool operator<(Particle *other) {
      return p.id < other->id();
    }
    
    friend std::ostream& operator<<(std::ostream &output, const Particle &p) {
      output << "P.id=" << p.id() << " type=" << p.type()
             << " pos=" << p.position() << " v=" << p.velocity()
             << " f=" << p.force() << " ghost=" << p.ghost()
             << " lmb=" << p.lambda() << " state=" << p.state()
             << " resid=" << p.res_id();
      return output;
    }

  private:
    ParticleProperties p;
    ParticlePosition r;
    ParticleMomentum m;
    ParticleLocal l;
    ParticleForce f;

    friend class boost::serialization::access;
    template< class Archive >
    void serialize(Archive &ar, const unsigned int version)
    {
      ar &p &r &m &f &l;
    }
  };

  struct ParticleList
    : public esutil::ESPPContainer < std::vector< Particle > >
  {};

  // singles
  class ParticleSingle
    : public Single< class Particle* >
  {
  private:
    typedef Single< class Particle* > Super;
  public:
    ParticleSingle() : Super() {}
    ParticleSingle(Particle *p) : Super(p) {}
    ParticleSingle(Particle &p) : Super(&p) {}
  };

  struct SingleList
    : public esutil::ESPPContainer< std::vector< ParticleSingle > >
  {
    void add(Particle *p) {
    	this->push_back(ParticleSingle(p));
    }

    void add(Particle &p) {
    	this->add(&p);
    }

    void add(std::vector<Particle*> particles) {
    	this->add(particles.at(0));
    }
  };

  // pairs
  class ParticlePair
    : public std::pair< class Particle*, class Particle* >
  {
  private:
    typedef std::pair< class Particle*, class Particle* > Super;
  public:
    ParticlePair() : Super() {}
    ParticlePair(Particle* p1, Particle* p2)
      : Super(p1, p2) {}
    ParticlePair(Particle &p1, Particle &p2)
      : Super(&p1, &p2) {}
  };

  struct PairList
    : public esutil::ESPPContainer< std::vector< ParticlePair > >
  {
    void add(Particle *p1, Particle *p2) {
        this->push_back(ParticlePair(p1, p2));
    }

    void add(Particle &p1, Particle &p2)
    { this->add(&p1, &p2); }

    void add(std::vector<Particle*> particles) {
        this->add(particles.at(0), particles.at(1));
    }
  };

  // triples
  class ParticleTriple
    : public Triple< class Particle*, class Particle*, class Particle* >
  {
  private:
    typedef Triple< class Particle*, class Particle*, class Particle* > Super;
  public:
    ParticleTriple() : Super() {}
    ParticleTriple(Particle* p1, Particle* p2, Particle* p3)
      : Super(p1, p2, p3) {}
    ParticleTriple(Particle &p1, Particle &p2, Particle &p3)
      : Super(&p1, &p2, &p3) {}
  };

  struct TripleList
    : public esutil::ESPPContainer< std::vector< ParticleTriple > >
  {
    void add(Particle *p1, Particle *p2, Particle *p3) {
        this->push_back(ParticleTriple(p1, p2, p3));
    }

    void add(Particle &p1, Particle &p2, Particle &p3)
    { this->add(&p1, &p2, &p3); }

    void add(std::vector<Particle*> particles) {
        this->add(particles.at(0), particles.at(1), particles.at(2));
    }
  };

  // quadruples
  class ParticleQuadruple
    : public Quadruple< class Particle*, class Particle*,
                        class Particle*, class Particle* >
  {
  private:
    typedef Quadruple< class Particle*, class Particle*,
                       class Particle*, class Particle* > Super;
  public:
    ParticleQuadruple() : Super() {}
    ParticleQuadruple(Particle* p1, Particle* p2, Particle* p3, Particle* p4)
      : Super(p1, p2, p3, p4) {}
    ParticleQuadruple(Particle &p1, Particle &p2, Particle &p3, Particle &p4)
      : Super(&p1, &p2, &p3, &p4) {}
  };

  struct QuadrupleList
    : public esutil::ESPPContainer< std::vector< ParticleQuadruple > >
  {
    void add(Particle* p1, Particle* p2, Particle* p3, Particle* p4) {
        this->push_back(ParticleQuadruple(p1, p2, p3, p4));
    }

    void add(Particle& p1, Particle& p2, Particle& p3, Particle& p4)
    { this->add(&p1, &p2, &p3, &p4); }

    void add(std::vector<Particle*> particles) {
        this->add(particles.at(0), particles.at(1),
                particles.at(2), particles.at(3));
    }
  };

  struct TupleList
   : public esutil::ESPPContainer< std::map<Particle*, std::vector<Particle*> > >  {
     void add(Particle* p, std::vector<Particle*> particles) {
         this->insert(make_pair(p, particles));
     }
  };

}


BOOST_IS_MPI_DATATYPE(espressopp::ParticleProperties)
BOOST_IS_MPI_DATATYPE(espressopp::ParticlePosition)
BOOST_IS_MPI_DATATYPE(espressopp::ParticleForce)
BOOST_IS_MPI_DATATYPE(espressopp::ParticleMomentum)
BOOST_IS_MPI_DATATYPE(espressopp::ParticleLocal)
BOOST_IS_MPI_DATATYPE(espressopp::Particle)

BOOST_CLASS_TRACKING(espressopp::ParticleProperties,track_never)
BOOST_CLASS_TRACKING(espressopp::ParticlePosition,track_never)
BOOST_CLASS_TRACKING(espressopp::ParticleForce,track_never)
BOOST_CLASS_TRACKING(espressopp::ParticleMomentum,track_never)
BOOST_CLASS_TRACKING(espressopp::ParticleLocal,track_never)
BOOST_CLASS_TRACKING(espressopp::Particle,track_never)

#endif
