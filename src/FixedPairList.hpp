/*
  Copyright (C) 2012,2013,2016
      Max Planck Institute for Polymer Research
  Copyright (C) 2008,2009,2010,2011
      Max-Planck-Institute for Polymer Research & Fraunhofer SCAI
  Copyright (C) 2017
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
#ifndef _FIXEDPAIRLIST_HPP
#define _FIXEDPAIRLIST_HPP

#include "log4espp.hpp"
#include "python.hpp"
#include "types.hpp"
#include "Particle.hpp"
#include "esutil/ESPPIterator.hpp"
#include <boost/unordered_map.hpp>
#include <boost/signals2.hpp>

//#include "FixedListComm.hpp"

namespace espressopp {
	class FixedPairList : public PairList {
	  public:
	    typedef boost::unordered_multimap<longint, longint> GlobalPairs;

	  protected:
		boost::signals2::connection sigBeforeSend, sigOnParticlesChanged, sigAfterRecv;
		shared_ptr <storage::Storage> storage;
		GlobalPairs globalPairs;
		using PairList::add;
		real longtimeMaxBondSqr;

	  public:
        FixedPairList() {}
		FixedPairList(shared_ptr <storage::Storage> _storage);
		virtual ~FixedPairList();

		real getLongtimeMaxBondSqr();
		void setLongtimeMaxBondSqr(real d);
		void resetLongtimeMaxBondSqr();

		/** Add the given particle pair to the list on this processor if the
		particle with the lower id belongs to this processor.  Note that
		this routine does not check whether the pair is inserted on
		another processor as well.
		\return whether the particle was inserted on this processor.
		*/
		virtual bool add(longint pid1, longint pid2);
		virtual bool iadd(longint pid1, longint pid2);
	    virtual bool remove(longint pid1, longint pid2, bool no_signal = false);
	    virtual bool removeByPid1(longint pid1, bool noSignal, bool removeAll, longint removeCounter);

	    virtual void beforeSendParticles(ParticleList& pl, class OutBuffer& buf);
		virtual void afterRecvParticles(ParticleList& pl, class InBuffer& buf);
		virtual void onParticlesChanged();
    void clearAndRemove();
	    virtual void updateParticlesStorage();

	    virtual std::vector<longint> getPairList();
	    virtual python::list getBonds();
	    virtual python::list getAllBonds();
	    virtual GlobalPairs* getGlobalPairs() {return &globalPairs;};

	    /** Get the number of bonds in the GlobalPairs list */
	    virtual int size() { return globalPairs.size(); }

	    virtual int totalSize();

	  	boost::signals2::signal<void  (longint, longint)> onTupleAdded;
	    boost::signals2::signal<void (longint, longint)> onTupleRemoved;

	    static void registerPython();

	  private:
		  static LOG4ESPP_DECL_LOGGER(theLogger);
	};
}

#endif

