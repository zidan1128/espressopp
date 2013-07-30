#!/usr/bin/env python
# -*- coding: iso-8859-1 -*-


###########################################################################
#  ESPResSo++                                                             #
#  Test script for Interpolation of tabulated LJ simulation               #
#                                                                         #
###########################################################################

import sys
import time
import os
import espresso
import MPI
import math
import logging
from espresso import Real3D, Int3D
from espresso.tools import timers



# Input values for system
N = 10                                    # box size
size  = (float(N), float(N), float(N))
numParticles = N**3                       # number of particles
nsteps  = 1000                            # number of steps
cutoff  = 2.5                             # cutoff for LJ potential
tabfile = "pot-lj.txt"                    # file for tabulated potential
skin    = 0.3                             # skin for Verlet lists
splinetypes = ['Linear','Akima','Cubic']  # implemented spline types


######################################################################
## IT SHOULD BE UNNECESSARY TO MAKE MODIFICATIONS BELOW THIS LINE   ##
######################################################################

print '\n-- Tabulated Interpolation Test -- \n'
print 'Steps: %3s' % nsteps
print 'Particles: %3s' % numParticles
print 'Cutoff: %3s' % cutoff


# writes the tabulated potential file
def writeTabFile(pot, name, N, low=0.0, high=2.5, body=2):
    outfile = open(name, "w")
    delta = (high - low) / (N - 1)
     
    for i in range(N):
        r = low + i * delta
        energy = pot.computeEnergy(r)
        if body == 2:# this is for 2-body potentials
            force = pot.computeForce(Real3D(r, 0.0, 0.0))[0]
        else: # this is for 3- and 4-body potentials
            force = pot.computeForce(r)
        outfile.write("%15.8g %15.8g %15.8g\n"%(r, energy, force))
     
    outfile.close()

# write the tabulated file for a LJ potential
print 'Generating potential file ... (%2s)' % tabfile
potLJ = espresso.interaction.LennardJones(epsilon=1.0, sigma=1.0, shift=0.0, cutoff=cutoff)
writeTabFile(potLJ, tabfile, N=257, low=0.01, high=potLJ.cutoff)


# compute the number of cells on each node
def calcNumberCells(size, nodes, cutoff):
    ncells = 1
    while size / (ncells * nodes) >= cutoff:
       ncells = ncells + 1
    return ncells - 1




# run simulation for all interpolation types
for spline in range(1,len(splinetypes)+1):
    
    print '\nSpline interpolation (%0d/%0d): %0s'% (spline, len(splinetypes), splinetypes[spline-1])
        
    # set up system
    system = espresso.System()
    system.rng  = espresso.esutil.RNG()
    system.bc = espresso.bc.OrthorhombicBC(system.rng, size)
    system.skin = skin
        
    comm = MPI.COMM_WORLD
        
    nodeGrid = Int3D(1, 1, comm.size)
    cellGrid = Int3D(
        calcNumberCells(size[0], nodeGrid[0], cutoff),
        calcNumberCells(size[1], nodeGrid[1], cutoff),
        calcNumberCells(size[2], nodeGrid[2], cutoff)
        )
    system.storage = espresso.storage.DomainDecomposition(system, nodeGrid, cellGrid)
        
    pid = 0
        
    for i in range(N):
        for j in range(N):
            for k in range(N):
                m = (i + 2*j + 3*k) % 11
                r = 0.45 + m * 0.01
                x = (i + r) / N * size[0]
                y = (j + r) / N * size[1]
                z = (k + r) / N * size[2]
                    
                x = 1.0 * i
                y = 1.0 * j
                z = 1.0 * k
                    
                system.storage.addParticle(pid, Real3D(x, y, z))
                    
                # not yet: dd.setVelocity(id, (1.0, 0.0, 0.0))
                pid = pid + 1
        
        
    system.storage.decompose()
        
    # integrator
    integrator = espresso.integrator.VelocityVerlet(system)
    integrator.dt = 0.005
        
    # now build Verlet List
    # ATTENTION: you must not add the skin explicitly here
    logging.getLogger("Interpolation").setLevel(logging.INFO)
    vl = espresso.VerletList(system, cutoff = cutoff)
        
    potTab = espresso.interaction.Tabulated(itype=spline, filename=tabfile, cutoff=cutoff)
    # ATTENTION: auto shift was enabled
    interTab = espresso.interaction.VerletListTabulated(vl)
    interTab.setPotential(type1=0, type2=0, potential=potTab)
    system.addInteraction(interTab)
        
        
    temp = espresso.analysis.Temperature(system)
    press = espresso.analysis.Pressure(system)
        
    temperature = temp.compute()
    p = press.compute()
    Ek = 0.5 * temperature * (3 * numParticles)
    Ep = interTab.computeEnergy()
        
        
    print 'Start %5s: tot energy = %10.3f pot = %10.3f kin = %10.3f temp = %10.3f p = %10.3f' \
        % ("", Ek + Ep, Ep, Ek, temperature, p)
        
    # langevin thermostat
    langevin = espresso.integrator.LangevinThermostat(system)
    integrator.addExtension(langevin)
    langevin.gamma = 1.0
    langevin.temperature = 1.0
     
    start_time = time.clock()
    integrator.run(nsteps)
    temperature = temp.compute()
    p = press.compute()
    Ek = 0.5 * temperature * (3 * numParticles)
    Ep = interTab.computeEnergy()
    print 'Step %6d: tot energy = %10.3f pot = %10.3f kin = %10.3f temp = %10.3f p = %10.3f\n' % \
            (nsteps, Ek + Ep, Ep, Ek, temperature, p)
        
    end_time = time.clock()
    timers.show(integrator.getTimers(), precision=2)

os.system('rm '+tabfile)
print '\nDone.'

