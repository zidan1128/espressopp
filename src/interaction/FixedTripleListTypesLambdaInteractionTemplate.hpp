/*
  Copyright (C) 2016
      Jakub Krajniak (jkrajniak at gmail.com)

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
#ifndef _INTERACTION_FIXEDTRIPLELISTTYPESLAMBDAINTERACTIONTEMPLATE_HPP
#define _INTERACTION_FIXEDTRIPLELISTTYPESLAMBDAINTERACTIONTEMPLATE_HPP

#include <algorithm>
#include <functional>
#include <vector>

#include "types.hpp"
#include "Interaction.hpp"
#include "Real3D.hpp"
#include "Tensor.hpp"
#include "Particle.hpp"
#include "FixedTripleListLambda.hpp"
#include "esutil/Array3D.hpp"
#include "bc/BC.hpp"
#include "SystemAccess.hpp"
#include "types.hpp"

#include "storage/Storage.hpp"

namespace espressopp {
namespace interaction {
template<typename _Potential>
class FixedTripleListTypesLambdaInteractionTemplate : public Interaction, SystemAccess {
 protected:
  typedef _Potential Potential;

 public:
  FixedTripleListTypesLambdaInteractionTemplate
      (shared_ptr<System> system,
       shared_ptr<FixedTripleListLambda> _fixedtripleList)
      : SystemAccess(system), fixedtripleList(_fixedtripleList) {
    potentialArray = esutil::Array3D<Potential, esutil::enlarge>(0, 0, 0, Potential());
    ntypes = 0;
  }

  virtual ~FixedTripleListTypesLambdaInteractionTemplate() { }

  void setFixedTripleList(shared_ptr<FixedTripleListLambda> _fixedtripleList) {
    fixedtripleList = _fixedtripleList;
  }

  shared_ptr<FixedTripleListLambda> getFixedTripleList() {
    return fixedtripleList;
  }

  void setPotential(int type1, int type2, int type3, const Potential &potential) {
    // typeX+1 because i<ntypes
    ntypes = std::max(ntypes, std::max(std::max(type1 + 1, type2 + 1), type3 + 1));
    potentialArray.at(type1, type2, type3) = potential;
    potentialArray.at(type3, type2, type1) = potential;
  }

  // this is used in the innermost force-loop
  Potential &getPotential(int type1, int type2, int type3) {
    return potentialArray.at(type1, type2, type3);
  }

  shared_ptr<Potential> getPotentialPtr(int type1, int type2, int type3) {
    return make_shared<Potential>(potentialArray.at(type1, type2, type3));
  }

  virtual void addForces();
  virtual real computeEnergy();
  virtual real computeEnergyDeriv();
  virtual real computeEnergyAA();
  virtual real computeEnergyCG();
  virtual void computeVirialX(std::vector<real> &p_xx_total, int bins);
  virtual real computeVirial();
  virtual void computeVirialTensor(Tensor &w);
  virtual void computeVirialTensor(Tensor &w, real z);
  virtual void computeVirialTensor(Tensor *w, int n);
  virtual real getMaxCutoff();
  virtual int bondType() { return Angular; }

 protected:
  int ntypes;
  shared_ptr<FixedTripleListLambda> fixedtripleList;
  esutil::Array3D<Potential, esutil::enlarge> potentialArray;
};

//////////////////////////////////////////////////
// INLINE IMPLEMENTATION
//////////////////////////////////////////////////
template<typename _Potential>
inline void
FixedTripleListTypesLambdaInteractionTemplate<_Potential>::addForces() {
  LOG4ESPP_INFO(theLogger, "add forces computed by the FixedTriple List");
  const bc::BC &bc = *getSystemRef().bc;

  for (FixedTripleListLambda::IteratorParticleLambda it(fixedtripleList->getParticleTriples()); it.isValid(); ++it) {
    Particle &p1 = *it->first;
    Particle &p2 = *it->second;
    Particle &p3 = *it->third;
    real lambda = it->lambda;
    int type1 = p1.type();
    int type2 = p2.type();
    int type3 = p3.type();
    const Potential &potential = getPotential(type1, type2, type3);

    Real3D dist12, dist32;
    bc.getMinimumImageVectorBox(dist12, p1.position(), p2.position());
    bc.getMinimumImageVectorBox(dist32, p3.position(), p2.position());
    Real3D force12, force32;
    potential._computeForce(force12, force32, dist12, dist32);
    p1.force() += lambda*force12;
    p2.force() -= lambda*(force12 + force32);
    p3.force() += lambda*force32;
  }
}

template<typename _Potential>
inline real
FixedTripleListTypesLambdaInteractionTemplate<_Potential>::
computeEnergy() {
  LOG4ESPP_INFO(theLogger, "compute energy of the FixedTriple list pairs");

  real e = 0.0;
  const bc::BC &bc = *getSystemRef().bc;  // boundary conditions
  for (FixedTripleListLambda::IteratorParticleLambda it(fixedtripleList->getParticleTriples()); it.isValid(); ++it) {
    const Particle &p1 = *it->first;
    const Particle &p2 = *it->second;
    const Particle &p3 = *it->third;
    real lambda = it->lambda;
    int type1 = p1.type();
    int type2 = p2.type();
    int type3 = p3.type();

    const Potential &potential = getPotential(type1, type2, type3);

    Real3D dist12 = bc.getMinimumImageVector(p1.position(), p2.position());
    Real3D dist32 = bc.getMinimumImageVector(p3.position(), p2.position());
    e += lambda*potential._computeEnergy(dist12, dist32);
    LOG4ESPP_TRACE(theLogger, "id1=" << p1.id() << " id2="
        << p2.id() << " id3=" << p3.id() << " potential energy=" << e);
  }

  // reduce over all CPUs
  real esum;
  boost::mpi::all_reduce(*mpiWorld, e, esum, std::plus<real>());
  return esum;
}

template < typename _AngularPotential > inline real
FixedTripleListTypesLambdaInteractionTemplate < _AngularPotential >::
computeEnergyDeriv() {
  std::cout << "Warning! At the moment computeEnergyDeriv() in FixedTripleListTypesLambdaInteractionTemplate does not work." << std::endl;
  return 0.0;
}

template<typename _Potential>
inline real
FixedTripleListTypesLambdaInteractionTemplate<_Potential>::
computeEnergyAA() {
}

template<typename _Potential>
inline real
FixedTripleListTypesLambdaInteractionTemplate<_Potential>::
computeEnergyCG() {
}

template<typename _Potential>
inline void
FixedTripleListTypesLambdaInteractionTemplate<_Potential>::
computeVirialX(std::vector<real> &p_xx_total, int bins) {
}

template<typename _Potential>
inline real
FixedTripleListTypesLambdaInteractionTemplate<_Potential>::
computeVirial() {
  LOG4ESPP_INFO(theLogger, "compute the virial for the FixedTriple List with types");

  real w = 0.0;
  const bc::BC &bc = *getSystemRef().bc;  // boundary conditions
  for (FixedTripleListLambda::IteratorParticleLambda it(fixedtripleList->getParticleTriples()); it.isValid(); ++it) {
    const Particle &p1 = *it->first;
    const Particle &p2 = *it->second;
    const Particle &p3 = *it->third;
    real lambda = it->lambda;
    int type1 = p1.type();
    int type2 = p2.type();
    int type3 = p3.type();
    const Potential &potential = getPotential(type1, type2, type3);

    Real3D dist12, dist32;
    bc.getMinimumImageVectorBox(dist12, p1.position(), p2.position());
    bc.getMinimumImageVectorBox(dist32, p3.position(), p2.position());
    Real3D force12, force32;
    potential._computeForce(force12, force32, dist12, dist32);
    w += dist12 * lambda*force12 + dist32 * lambda*force32;
  }

  // reduce over all CPUs
  real wsum;
  boost::mpi::all_reduce(*mpiWorld, w, wsum, std::plus<real>());
  return wsum;
}

template<typename _Potential>
inline void
FixedTripleListTypesLambdaInteractionTemplate<_Potential>::computeVirialTensor(Tensor &w) {
  LOG4ESPP_INFO(theLogger, "compute the virial tensor for the FixedTriple List");

  Tensor wlocal(0.0);
  const bc::BC& bc = *getSystemRef().bc;
  for (FixedTripleListLambda::IteratorParticleLambda it(fixedtripleList->getParticleTriples()); it.isValid(); ++it) {
    const Particle &p1 = *it->first;
    const Particle &p2 = *it->second;
    const Particle &p3 = *it->third;
    real lambda = it->lambda;
    int type1 = p1.type();
    int type2 = p2.type();
    int type3 = p3.type();
    const Potential &potential = getPotential(type1, type2, type3);
    Real3D r12, r32;
    bc.getMinimumImageVectorBox(r12, p1.position(), p2.position());
    bc.getMinimumImageVectorBox(r32, p3.position(), p2.position());
    Real3D force12, force32;
    potential._computeForce(force12, force32, r12, r32);
    wlocal += Tensor(r12, lambda*force12) + Tensor(r32, lambda*force32);
  }

  // reduce over all CPUs
  Tensor wsum(0.0);
  boost::mpi::all_reduce(*mpiWorld, wlocal, wsum, std::plus<Tensor>());
  w += wsum;
}

template<typename _Potential>
inline void
FixedTripleListTypesLambdaInteractionTemplate<_Potential>::
computeVirialTensor(Tensor &w, real z) {
  LOG4ESPP_INFO(theLogger, "compute the virial tensor of the triples");
}

template<typename _Potential>
inline void
FixedTripleListTypesLambdaInteractionTemplate<_Potential>::
computeVirialTensor(Tensor *w, int n) {
  LOG4ESPP_INFO(theLogger, "compute the virial tensor for the FixedTriple List");
}

template<typename _Potential>
inline real
FixedTripleListTypesLambdaInteractionTemplate<_Potential>::getMaxCutoff() {
  real cutoff = 0.0;
  for (int i = 0; i < ntypes; i++) {
    for (int j = 0; j < ntypes; j++) {
      for (int k = 0; k < ntypes; k++) {
        cutoff = std::max(cutoff, getPotential(i, j, k).getCutoff());
      }
    }
  }
  return cutoff;
}
}  // namespace interaction
}  // namespace espressopp
#endif
