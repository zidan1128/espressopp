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

#include "bindings.hpp"
#include "MDIntegrator.hpp"
#include "VelocityVerlet.hpp"
#include "VelocityVerletOnGroup.hpp"

#include "Extension.hpp"
#include "TDforce.hpp"
#include "FreeEnergyCompensation.hpp"
#include "Adress.hpp"
#include "BerendsenBarostat.hpp"
#include "BerendsenBarostatAnisotropic.hpp"
#include "BerendsenThermostat.hpp"
#include "Isokinetic.hpp"
#include "StochasticVelocityRescaling.hpp"
#include "LangevinThermostat.hpp"
#include "LangevinThermostat1D.hpp"
#include "DPDThermostat.hpp"
#include "LangevinBarostat.hpp"
#include "FixPositions.hpp"
#include "LatticeBoltzmann.hpp"
#include "LatticeSite.hpp"
#include "LBInit.hpp"
#include "LBInitConstForce.hpp"
#include "LBInitPeriodicForce.hpp"
#include "LBInitPopUniform.hpp"
#include "LBInitPopWave.hpp"
#include "ExtForce.hpp"
#include "CapForce.hpp"
#include "ExtAnalyze.hpp"
#include "VelocityVerletOnRadius.hpp"

namespace espresso {
  namespace integrator {
    void registerPython() {
      MDIntegrator::registerPython();
      VelocityVerlet::registerPython();
      VelocityVerletOnGroup::registerPython();
      Extension::registerPython();
      Adress::registerPython();
      BerendsenBarostat::registerPython();
      BerendsenBarostatAnisotropic::registerPython();
      BerendsenThermostat::registerPython();
      LangevinBarostat::registerPython();
      Isokinetic::registerPython();
      StochasticVelocityRescaling::registerPython();
      TDforce::registerPython();
      FreeEnergyCompensation::registerPython();
      LangevinThermostat::registerPython();
      LangevinThermostat1D::registerPython();
      DPDThermostat::registerPython();
      FixPositions::registerPython();
      LatticeBoltzmann::registerPython();
      LBInit::registerPython();
      LBInitConstForce::registerPython();
      LBInitPeriodicForce::registerPython();
      LBInitPopUniform::registerPython();
      LBInitPopWave::registerPython();
      ExtForce::registerPython();
      CapForce::registerPython();
      ExtAnalyze::registerPython();
      VelocityVerletOnRadius::registerPython();
    }
  }
}


