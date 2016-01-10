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
#ifndef _FIXEDPAIRLAMBDALIST_HPP
#define _FIXEDPAIRLAMBDALIST_HPP

#include "log4espp.hpp"
#include "python.hpp"
#include "Particle.hpp"
#include "esutil/ESPPIterator.hpp"
#include <map>
#include <boost/signals2.hpp>
#include "types.hpp"

namespace espressopp {
class FixedPairLambdaList: public PairList {
 protected:
  typedef std::multimap <longint, std::pair<longint, real> > PairsLambda;
  boost::signals2::connection con1, con2, con3;
  shared_ptr <storage::Storage> storage;
  PairsLambda pairsLambda;
  using PairList::add;

 public:
  FixedPairLambdaList(shared_ptr <storage::Storage> _storage, real initLambda);
  virtual ~FixedPairLambdaList();

  /** Add the given particle pair to the list on this processor if the
  particle with the lower id belongs to this processor.  Note that
  this routine does not check whether the pair is inserted on
  another processor as well.
  \return whether the particle was inserted on this processor.
  */
  virtual bool add(longint pid1, longint pid2);
  virtual void beforeSendParticles(ParticleList &pl, class OutBuffer &buf);
  void afterRecvParticles(ParticleList &pl, class InBuffer &buf);
  virtual void onParticlesChanged();

  python::list getPairs();
  python::list getPairsLambda();

  real getLambda(longint pid1, longint pid2);
  /** Get the number of bonds in the GlobalPairs list */
  int size() { return pairsLambda.size(); }

  boost::signals2::signal2 <void, longint, longint> onTupleAdded;

  static void registerPython();

 private:
  real initLambda_;
  static LOG4ESPP_DECL_LOGGER(theLogger);
};
}

#endif

