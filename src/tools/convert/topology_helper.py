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


# Some helper classes usefull when parsing the gromacs topology

import espresso
import math
import gromacs
import os

class FileBuffer():
    def __init__(self):
        self.linecount=0
        self.lines=[]
        self.pos=0
    def appendline(self, line):
        self.lines.append(line)
    def readline(self):
        try:
            line=self.lines[self.pos]
        except:
            return ''
        self.pos+=1
        return line
    def readlastline(self):
        try:
            line=self.lines[self.pos-1]
        except:
            return ''
        return line
    def seek(self, p):
        self.pos=p
    def tell(self):
        return self.pos


def FillFileBuffer(fname, filebuffer, context=None):
    f = open(fname, 'r')
    base_path = os.path.dirname(fname)

    skip_lines = False
    in_ifdef = False

    if context is None:
        context = {}

    special_symbols = ['define', 'ifdef', 'ifndef', 'else', 'endif']

    for line in f:
        # Skip commends
        if line.strip().startswith(';'):
            continue

        # Support for define
        if 'define' in line:
            tmp = line.split()
            context[tmp[1]] = tmp[2:]
            continue

        # By definition skip the ifdef clauses
        if 'ifdef' in line:
            define_var = line.split()[1]
            if define_var not in context:
                skip_lines = True
            in_ifdef = True
            continue

        if 'ifndef' in line:
            define_var = line.split()[1]
            if define_var in context:
                skip_lines = True
            in_ifdef = True
            continue

        if 'else' in line:
            skip_lines = not skip_lines
            continue

        if 'endif' in line:
            if not in_ifdef:
                raise ValueError('Wrong sequence of if-else-endif')
            skip_lines = False
            in_ifdef = False
            continue

        # Support for define variables
        if line.startswith('#'):
            symbol = line.strip().lstrip('#').split()[0]
            if symbol not in special_symbols:
                value = context.get(symbol)
                if value:
                    filebuffer.appendline(' '.join(value))

        if not skip_lines:
            if "include" in line:
                name = os.path.join(base_path, (line.split()[1]).strip('\"'))
                FillFileBuffer(name, filebuffer, context)
            else:
                l = line.rstrip('\n')
                if l:
                    filebuffer.appendline(l)
    f.close()
    return


def FindType(proposedtype, typelist):
    list=[typeid for (typeid,atype) in typelist.iteritems() if atype==proposedtype ]
    if len(list)>1:
        print "Error: duplicate type definitons", proposedtype.parameters
        exit()
    elif len(list)==0:
        return None
    return list[0]


class InteractionType:
    def __init__(self, parameters):
        self.parameters=parameters
    def __eq__(self,other):
        # interaction types are defined to be equal if all parameters are equal
        for k, v in self.parameters.iteritems():
            if k not in other.parameters: return False
            if other.parameters[k]!=v: return False
        return True
    def createEspressoInteraction(self, system, fpl):
        print "WARNING: could not set up interaction for", self.parameters, ": Espresso potential not implemented"
        return None
    def automaticExclusion(self):
        #overwrite in derrived class if the particular interaction is automatically excluded
        return False

class HarmonicBondedInteractionType(InteractionType):
    def createEspressoInteraction(self, system, fpl):
        # interaction specific stuff here
        # spring constant kb is half the gromacs spring constant
        pot = espresso.interaction.Harmonic(self.parameters['kb']/2.0, self.parameters['b0'])
        interb = espresso.interaction.FixedPairListHarmonic(system, fpl, pot)
        return interb
    def automaticExclusion(self):
        return True

class MorseBondedInteractionType(InteractionType):
    def createEspressoInteraction(self, system, fpl):
        # interaction specific stuff here
        pot = espresso.interaction.Morse(self.parameters['D'], self.parameters['beta'], self.parameters['rmin'])
        interb = espresso.interaction.FixedPairListMorse(system, fpl, pot)
        return interb
    def automaticExclusion(self):
        return True

class FENEBondedInteractionType(InteractionType):
    def createEspressoInteraction(self, system, fpl):
        # interaction specific stuff here
        # spring constant kb is half the gromacs spring constant
        pot = espresso.interaction.Fene(self.parameters['kb']/2.0, self.parameters['b0'])
        interb = espresso.interaction.FixedPairListFene(system, fpl, pot)
        return interb
    def automaticExclusion(self):
        return True

class HarmonicAngleInteractionType(InteractionType):
    def createEspressoInteraction(self, system, fpl):
        # interaction specific stuff here
        # spring constant kb is half the gromacs spring constant. Also convert deg to rad
        pot = espresso.interaction.AngularHarmonic(self.parameters['k']/2.0, self.parameters['theta']*2*math.pi/360)
        interb = espresso.interaction.FixedTripleListAngularHarmonic(system, fpl, pot)
        return interb


class TabulatedBondInteractionType(InteractionType):
    def createEspressoInteraction(self, system, fpl):
        spline = 3
        fg = "table_b"+str(self.parameters['tablenr'])+".xvg"
        fe = fg.split(".")[0]+".tab" # name of espresso file
        gromacs.convertTable(fg, fe)
        potTab = espresso.interaction.Tabulated(itype=spline, filename=fe)
        interb = espresso.interaction.FixedPairListTabulated(system, fpl, potTab)
        return interb
    def automaticExclusion(self):
        return self.parameters['excl']

class TabulatedAngleInteractionType(InteractionType):
    def createEspressoInteraction(self, system, fpl):
        spline = 3
        fg = "table_a"+str(self.parameters['tablenr'])+".xvg"
        fe = fg.split(".")[0]+".tab" # name of espresso file
        gromacs.convertTable(fg, fe)
        potTab = espresso.interaction.TabulatedAngular(itype=spline, filename=fe)
        interb = espresso.interaction.FixedTripleListTabulatedAngular(system, fpl, potTab)
        return interb
class TabulatedDihedralInteractionType(InteractionType):
    def createEspressoInteraction(self, system, fpl):
        spline = 3
        fg = "table_d"+str(self.parameters['tablenr'])+".xvg"
        fe = fg.split(".")[0]+".tab" # name of espresso file
        gromacs.convertTable(fg, fe)
        potTab = espresso.interaction.TabulatedDihedral(itype=spline, filename=fe)
        interb = espresso.interaction.FixedQuadrupleListTabulatedDihedral(system, fpl, potTab)
        return interb

"""class HarmonicDihedralInteractionType(InteractionType):
# Not implemented yet in Esp++
    def createEspressoInteraction(self, system, fpl):
        # interaction specific stuff here
        # spring constant kb is half the gromacs spring constant. Also convert deg to rad
        print "setting up dihedral ", self.parameters
        pot = espresso.interaction.DihedralHarmonicCos(K=self.parameters['k']/2.0, phi0=self.parameters['phi']*2*math.pi/360)
        interb = espresso.interaction.FixedQuadrupleListDihedralHarmonicCos(system, fpl, pot)
        return interb          """

def ParseBondTypeParam(line):
    tmp = line.split()
    btype= tmp[2]
    # TODO: handle exclusions automatically
    if btype == "8":
        p=TabulatedBondInteractionType({"tablenr":int(tmp[3]),"k":float(tmp[4]), 'excl':True})
    elif btype == "9":
        p=TabulatedBondInteractionType({"tablenr":int(tmp[3]), "k":float(tmp[4]), 'excl':False})
    elif btype == "1":
        p=HarmonicBondedInteractionType({"b0":float(tmp[3]), "kb":float(tmp[4])})
    elif btype == "3":
        p=MorseBondedInteractionType({"b0":float(tmp[3]), "D":float(tmp[4]), "beta":float(tmp[5])})
    elif btype == "7":
        p=FENEBondedInteractionType({"b0":float(tmp[3]), "kb":float(tmp[4])})
    elif btype == "9":
        p=TabulatedBondInteractionType({"tablenr":int(tmp[3]), "k":float(tmp[4])})
    else:
        print "Unsupported bond type", tmp[2], "in line:"
        print line
        return None
    return p

def ParseAngleTypeParam(line):
    tmp = line.split()
    type= int(tmp[3])
    if type == 1:
        p=HarmonicAngleInteractionType({"theta":float(tmp[4]), "k":float(tmp[5])})
    elif type == 8:
        p=TabulatedAngleInteractionType({"tablenr":int(tmp[4]),"k":float(tmp[5])})
    else:
        print "Unsupported angle type", type, "in line:"
        print line
        return None
    return p

def ParseDihedralTypeParam(line):
    tmp = line.split()
    type= int(tmp[4])
    if type == 8:
        p = TabulatedDihedralInteractionType({"tablenr":int(tmp[5]), "k":float(tmp[6])})
    else:
        print "Unsupported dihedral type", type, "in line:"
        print line
        return None
    return p



# Usefull code for generating the regular exclusions

class Node():
    def __init__(self, id):
	self.id=id
	self.neighbours=[]
    def addNeighbour(self, nb):
	self.neighbours.append(nb)

def FindNodeById(id, nodes):
    list=[n for n in nodes if n.id==id ]
    if len(list)>1:
        print "Error: duplicate nodes", id
        exit()
    elif len(list)==0:
        return None
    return list[0]

def FindNNextNeighbours(startnode, numberNeighbours, neighbours, forbiddenNodes):
    if numberNeighbours==0:
	return neighbours

    #avoid going back the same path
    forbiddenNodes.append(startnode)

    # Loop over next neighbours and add them to the neighbours list
    # Recursively call the function with numberNeighbours-1
    for n in startnode.neighbours:
	if not n in forbiddenNodes:
	    if n not in neighbours: neighbours.append(n) # avoid double counting in rings
	    FindNNextNeighbours(n, numberNeighbours-1, neighbours, forbiddenNodes)


def GenerateRegularExclusions(bonds, nrexcl, exclusions):
    nodes=[]
    # make a Node object for each atom involved in bonds
    for b in bonds:
        bids=b[0:2]
        for i in bids:
            if FindNodeById(i, nodes)==None:
               n=Node(i)
               nodes.append(n)

    # find the next neighbours for each node and append them
    for b in bonds:
        permutations=[(b[0], b[1]), (b[1], b[0])]
        for p in permutations:
            n=FindNodeById(p[0], nodes)
            nn=FindNodeById(p[1], nodes)
            n.addNeighbour(nn)

    # for each atom, call the FindNNextNeighbours function, which recursively
    # seraches for nrexcl next neighbours
    for n in nodes:
        neighbours=[]
        FindNNextNeighbours(n, nrexcl, neighbours, forbiddenNodes=[])
        for nb in neighbours:
            # check if the permutation is already in the exclusion list
            # this may be slow, but to do it in every MD step is even slower...
            # TODO: find a clever algorithm which does avoid permuations from the start
            if not (n.id, nb.id) in exclusions:
                if not (nb.id, n.id) in exclusions:
                    exclusions.append((n.id, nb.id))

    return exclusions
