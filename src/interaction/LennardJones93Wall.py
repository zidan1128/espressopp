#  Copyright (C) 2014
#      Pierre de Buyl
#  Copyright (C) 2012,2013
#      Max Planck Institute for Polymer Research
#  Copyright (C) 2008,2009,2010,2011
#      Max-Planck-Institute for Polymer Research & Fraunhofer SCAI
#
#  This file is part of ESPResSo++.
#
#  ESPResSo++ is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  ESPResSo++ is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""
*******************************************
**espresso.interaction.LennardJones93Wall**
*******************************************

"""
from espresso import pmi
from espresso.esutil import *

from espresso.interaction.SingleParticlePotential import *
from espresso.interaction.Interaction import *
from _espresso import interaction_LennardJones93Wall, interaction_SingleParticleLennardJones93Wall


class LennardJones93WallLocal(SingleParticlePotentialLocal, interaction_LennardJones93Wall):
    'The (local) LennardJones93Wall potential.'
    def __init__(self):
        """Initialize the local LennardJones93Wall object."""
        if not (pmi._PMIComm and pmi._PMIComm.isActive()) or pmi._MPIcomm.rank in pmi._PMIComm.getMPIcpugroup():
            cxxinit(self, interaction_LennardJones93Wall)


class SingleParticleLennardJones93WallLocal(InteractionLocal, interaction_SingleParticleLennardJones93Wall):
    'The (local) LennardJones93Wall interaction.'
    def __init__(self, system, potential):
        if not (pmi._PMIComm and pmi._PMIComm.isActive()) or pmi._MPIcomm.rank in pmi._PMIComm.getMPIcpugroup():
            cxxinit(self, interaction_SingleParticleLennardJones93Wall, system, potential)

    def setPotential(self, potential):
        if not (pmi._PMIComm and pmi._PMIComm.isActive()) or pmi._MPIcomm.rank in pmi._PMIComm.getMPIcpugroup():
            self.cxxclass.setPotential(self, potential)

    def getParams(self, type_var):
        if not (pmi._PMIComm and pmi._PMIComm.isActive()) or pmi._MPIcomm.rank in pmi._PMIComm.getMPIcpugroup():
            return self.cxxclass.getParams(self, type)

if pmi.isController:
    class LennardJones93Wall(SingleParticlePotential):
        'The LennardJones93Wall potential.'
        pmiproxydefs = dict(
            cls = 'espresso.interaction.LennardJones93WallLocal',
            pmicall = ['setParams', 'getParams']
            )

    class SingleParticleLennardJones93Wall(Interaction):
        __metaclass__ = pmi.Proxy
        pmiproxydefs = dict(
            cls = 'espresso.interaction.SingleParticleLennardJones93WallLocal',
            pmicall = ['setPotential']
            )
