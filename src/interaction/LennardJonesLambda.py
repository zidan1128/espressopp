#  Copyright (C) 2015
#      Jakub Krajniak (jkrajniak at gmail.com)
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


r"""
*************************************************
**espressopp.interaction.LennardJonesLambda**
*************************************************
.. math::
	V(r) = 4 \varepsilon \left[ \left( \frac{\lambda \sigma}{r} \right)^{12} -
	\left( \frac{\lambda \sigma}{r} \right)^{6} \right]

"""
from espressopp import pmi, infinity
from espressopp.esutil import *

from espressopp.interaction.Potential import *
from espressopp.interaction.Interaction import *
from _espressopp import interaction_LennardJonesLambda, \
                        interaction_VerletListLennardJonesLambda

class LennardJonesLambdaLocal(PotentialLocal, interaction_LennardJonesLambda):
    def __init__(self, epsilon=1.0, sigma=1.0, cutoff=infinity, shift="auto"):
        """Initialize the local Lennard Jones object."""
        if pmi.workerIsActive():
            if shift =="auto":
                cxxinit(self, interaction_LennardJonesLambda,
                        epsilon, sigma, cutoff)
            else:
                cxxinit(self, interaction_LennardJonesLambda,
                        epsilon, sigma, cutoff, shift)


class VerletListLennardJonesLambdaLocal(InteractionLocal, interaction_VerletListLennardJonesLambda):

    def __init__(self, vl):
        if not (pmi._PMIComm and pmi._PMIComm.isActive()) or pmi._MPIcomm.rank in pmi._PMIComm.getMPIcpugroup():
            cxxinit(self, interaction_VerletListLennardJonesLambda, vl)

    def setPotential(self, type1, type2, potential):
        if not (pmi._PMIComm and pmi._PMIComm.isActive()) or pmi._MPIcomm.rank in pmi._PMIComm.getMPIcpugroup():
            self.cxxclass.setPotential(self, type1, type2, potential)

    def getPotential(self, type1, type2):
        if not (pmi._PMIComm and pmi._PMIComm.isActive()) or pmi._MPIcomm.rank in pmi._PMIComm.getMPIcpugroup():
            return self.cxxclass.getPotential(self, type1, type2)

    def getVerletListLocal(self):
        if not (pmi._PMIComm and pmi._PMIComm.isActive()) or pmi._MPIcomm.rank in pmi._PMIComm.getMPIcpugroup():
            return self.cxxclass.getVerletList(self)


if pmi.isController:
    class LennardJonesLambda(Potential):
        'The Lennard-Jones potential.'
        pmiproxydefs = dict(
            cls = 'espressopp.interaction.LennardJonesLambdaLocal',
            pmiproperty = ['epsilon', 'sigma', 'max_force']
            )

    class VerletListLennardJonesLambda(Interaction):
        __metaclass__ = pmi.Proxy
        pmiproxydefs = dict(
            cls =  'espressopp.interaction.VerletListLennardJonesLambdaLocal',
            pmicall = ['setPotential', 'getPotential', 'getVerletList']
            )
