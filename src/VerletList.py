#  Copyright (C) 2012,2013,2015
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
*************************
**espressopp.VerletList**
*************************


.. function:: espressopp.VerletList(system, cutoff, exclusionlist)

		:param system: 
		:param cutoff: 
		:param exclusionlist: (default: [])
		:type system: 
		:type cutoff: 
		:type exclusionlist: 

.. function:: espressopp.VerletList.exclude(exclusionlist)

		:param exclusionlist: 
		:type exclusionlist: 
		:rtype: 

.. function:: espressopp.VerletList.getAllPairs()

		:rtype: 

.. function:: espressopp.VerletList.localSize()

		:rtype: 

.. function:: espressopp.VerletList.totalSize()

		:rtype: 
"""
from espressopp import pmi
import _espressopp 
import espressopp
from espressopp.esutil import cxxinit


class DynamicExcludeListLocal(_espressopp.DynamicExcludeList):
    def __init__(self, integrator, exclusionlist=None):
        if pmi.workerIsActive():
            cxxinit(self, _espressopp.DynamicExcludeList, integrator)
            if exclusionlist is not None:
                for pid1, pid2 in exclusionlist:
                    self.cxxclass.exclude(self, pid1, pid2)
                self.cxxclass.update(self)

    def exclude(self, pid1, pid2):
        if pmi.workerIsActive():
            self.cxxclass.exclude(self, pid1, pid2)

    def unexclude(self, pid1, pid2):
        if pmi.workerIsActive():
            self.cxxclass.unexclude(self, pid1, pid2)

    def observe_tuple(self, fpl):
        if pmi.workerIsActive():
            self.cxxclass.observe_tuple(self, fpl)

    def observe_triple(self, ftl):
        if pmi.workerIsActive():
            self.cxxclass.observe_triple(self, ftl)

    def observe_quadruple(self, fql):
        if pmi.workerIsActive():
            self.cxxclass.observe_quadruple(self, fql)

class VerletListLocal(_espressopp.VerletList):


    def __init__(self, system, cutoff, exclusionlist=None):

        if pmi.workerIsActive():
            if exclusionlist is None:
                # rebuild list in constructor
                cxxinit(self, _espressopp.VerletList, system, cutoff, True)
            elif isinstance(exclusionlist, DynamicExcludeListLocal):
                cxxinit(self, _espressopp.VerletList, system, cutoff, exclusionlist, True)
            else:
                # do not rebuild list in constructor
                cxxinit(self, _espressopp.VerletList, system, cutoff, False)
                # add exclusions
                for pair in exclusionlist:
                    pid1, pid2 = pair
                    self.cxxclass.exclude(self, pid1, pid2)
                # now rebuild list with exclusions
                self.cxxclass.rebuild(self)
                
            
    def totalSize(self):

        if pmi.workerIsActive():
            return self.cxxclass.totalSize(self)
        
    def localSize(self):

        if pmi.workerIsActive():
            return self.cxxclass.localSize(self)
        
    def exclude(self, exclusionlist):
        """
        Each processor takes the broadcasted exclusion list
        and adds it to its list.
        """
        if pmi.workerIsActive():
            for pair in exclusionlist:
                pid1, pid2 = pair
                self.cxxclass.exclude(self, pid1, pid2)
            # rebuild list with exclusions
            self.cxxclass.rebuild(self)
            
    def getAllPairs(self):

        if pmi.workerIsActive():
            pairs=[]
            npairs=self.localSize()
            for i in range(npairs):
              pair=self.cxxclass.getPair(self, i+1)
              pairs.append(pair)
            return pairs 


if pmi.isController:
  class DynamicExcludeList(object):
    __metaclass__ = pmi.Proxy
    pmiproxydefs = dict(
        cls='espressopp.DynamicExcludeListLocal',
        pmiproperty=['is_dirty', 'size'],
        pmicall=['exclude', 'unexclude', 'connect', 'disconnect', 'observe_tuple',
                 'observe_triple', 'observe_quadruple', 'update'],
        pmiinvoke=['get_list']
    )

  class VerletList(object):
    __metaclass__ = pmi.Proxy
    pmiproxydefs = dict(
      cls = 'espressopp.VerletListLocal',
      pmiproperty = [ 'builds' ],
      pmicall = [ 'totalSize', 'exclude', 'connect', 'disconnect', 'getVerletCutoff', 'setVerletCutoff' ],
      pmiinvoke = [ 'getAllPairs' ]
    )
