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

#ifndef _FIXEDPAIRLAMBDALIST_HPP
#define _FIXEDPAIRLAMBDALIST_HPP

#include "log4espp.hpp"
#include "python.hpp"
#include "Particle.hpp"
#include "esutil/ESPPIterator.hpp"
#include "FixedPairList.hpp"
#include <map>
#include <boost/signals2.hpp>
#include "types.hpp"

namespace espressopp {

class ParticlePairLambda {
 public:
  ParticlePairLambda(Particle *p1_, Particle *p2_, real l)
      : p1(p1_), p2(p2_), lambda(l) { }

  Particle *p1;
  Particle *p2;
  real lambda;
};

class FixedPairListLambda : public FixedPairList {
 public:
  typedef boost::unordered_multimap<longint, std::pair<longint, real> > PairsLambda;
  typedef std::vector<ParticlePairLambda> ParticlePairsLambda;
  typedef esutil::ESPPIterator<std::vector<ParticlePairLambda> > IteratorParticleLambda;

 protected:
  shared_ptr <storage::Storage> storage;
  using PairList::add;
  static LOG4ESPP_DECL_LOGGER(theLogger);

 public:
  FixedPairListLambda(shared_ptr <storage::Storage> _storage, real lambda0);

  virtual bool add(longint pid1, longint pid2);
  virtual bool iadd(longint pid1, longint pid2);
  virtual bool remove(longint pid1, longint pid2, bool no_signal = false);
  /**
   * Remove all bonds where first argument is pid1 or remove n-th bonds.
   * @param pid1 Particle id
   * @param noSignal Do not throw onTupleRemoved signal
   * @param removeAll if true then remove all bonds with pid1
   * @param removeCounter remove n-th bonds
   * @return
   */
  virtual bool removeByPid1(longint pid1, bool noSignal, bool removeAll, longint removeCounter);

  virtual void beforeSendParticles(ParticleList& pl, class OutBuffer& buf);
  virtual void afterRecvParticles(ParticleList& pl, class InBuffer& buf);
  virtual void onParticlesChanged();
  virtual void updateParticlesStorage();

  virtual std::vector<longint> getPairList();
  virtual python::list getBonds();
  virtual python::list getAllBonds();
  virtual GlobalPairs* getGlobalPairs() { throw std::runtime_error("getGlobalPairs method not implemented!"); };

  /** Get the number of bonds in the GlobalPairs list */
  virtual int size() { return pairsLambda_.size(); }
  virtual int totalSize();

  // Lambda support
  real getLambda(longint pid1, longint pid2);
  void setLambda(longint pid1, longint pid2, real lambda);
  void setAllLambda(real lambda0);
  void incrementAllLambda(real d_lambda);

  ParticlePairsLambda& getParticlePairs() { return particlePairsLambda_; }

  static void registerPython();
 private:
  real lambda0_;
  PairsLambda pairsLambda_;
  ParticlePairsLambda particlePairsLambda_;
};

}
#endif

