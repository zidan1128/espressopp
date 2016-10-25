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

#ifndef _INTEGRATOR_ATRPActivator_HPP
#define _INTEGRATOR_ATRPActivator_HPP

#include <algorithm>
#include "types.hpp"
#include "logging.hpp"
#include "Extension.hpp"
#include "boost/signals2.hpp"
#include "Real3D.hpp"
#include "Particle.hpp"
#include "iterator/CellListIterator.hpp"
#include "storage/NodeGrid.hpp"
#include "storage/DomainDecomposition.hpp"

namespace espressopp {
namespace integrator {

struct ReactiveCenter {
  longint min_state;  ///<! minimal chemical state
  longint max_state;  ///<! maximum chemical state
  longint delta_state;  ///<! update of chemical potential
  real p;  ///<! probability constant [0, 1);
  shared_ptr<ParticleProperties> new_property;  ///<! new property

  ReactiveCenter() {
    min_state = -1;
    max_state = -1;
    delta_state = 0;
    p = 0.0;
  }

  ReactiveCenter(longint min_state_, longint max_state_, longint delta_state_, real p_,
                 shared_ptr<ParticleProperties> new_property_) {
    min_state = min_state_;
    max_state = max_state_;
    delta_state = delta_state_;
    p = p_;
    new_property = new_property_;
  }
};

class ATRPActivator: public Extension {
 public:
  ATRPActivator(shared_ptr<System> system, longint interval, longint num_per_interval);

  virtual ~ATRPActivator() {};

  void addReactiveCenter(longint type_id,
                         longint min_state,
                         longint max_state,
                         shared_ptr<ParticleProperties> pp,
                         longint delta_state,
                         real p);

  /** Register this class so it can be used from Python. */
  static void registerPython();

 private:
  boost::signals2::connection sig_aftIntV;
  longint interval_;
  longint num_per_interval_;

  typedef boost::unordered_multimap<longint, ReactiveCenter> SpeciesMap;
  SpeciesMap species_map_;

  shared_ptr<esutil::RNG> rng_;

  void updateParticles();

  void connect();
  void disconnect();

  /** Logger */
  static LOG4ESPP_DECL_LOGGER(theLogger);
  void updateGhost(const std::vector<Particle *> &modified_particles);

};

}
}

#endif
