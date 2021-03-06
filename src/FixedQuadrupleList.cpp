/*
  Copyright (C) 2016-2017
      Jakub Krajniak (jkrajniak at gmail.com)
  Copyright (C) 2012,2013,2016
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

#include "python.hpp"
#include <sstream>
#include "FixedQuadrupleList.hpp"

#include <boost/bind.hpp>
#include "storage/Storage.hpp"
#include "Buffer.hpp"

#include "esutil/Error.hpp"

//using namespace std;

namespace espressopp {

  /*
  FixedQuadrupleList::FixedQuadrupleList(shared_ptr< storage::Storage > _storage)
  : FixedListComm (_storage){}
  */


  LOG4ESPP_LOGGER(FixedQuadrupleList::theLogger, "FixedQuadrupleList");

  FixedQuadrupleList::FixedQuadrupleList(shared_ptr< storage::Storage > _storage) 
    : storage(_storage), globalQuadruples()
  {
    LOG4ESPP_INFO(theLogger, "construct FixedQuadrupleList");

    sigBeforeSend = storage->beforeSendParticles.connect
      (boost::bind(&FixedQuadrupleList::beforeSendParticles, this, _1, _2));
    sigAfterRecv = storage->afterRecvParticles.connect
      (boost::bind(&FixedQuadrupleList::afterRecvParticles, this, _1, _2));
    sigOnParticlesChanged = storage->onParticlesChanged.connect
      (boost::bind(&FixedQuadrupleList::onParticlesChanged, this));
  }

  FixedQuadrupleList::~FixedQuadrupleList() {

    LOG4ESPP_INFO(theLogger, "~FixedQuadrupleList");

    sigBeforeSend.disconnect();
    sigAfterRecv.disconnect();
    sigOnParticlesChanged.disconnect();
  }


  /*
  bool FixedQuadrupleList::add(longint pid1, longint pid2, longint pid3, longint pid4) {
      std::vector<longint> tmp;
      tmp.push_back(pid2);
      tmp.push_back(pid3);
      tmp.push_back(pid4);
      tmp.push_back(pid1); // this is used as key

      return FixedListComm::add(tmp);
  }*/

  bool FixedQuadrupleList::iadd(longint pid1, longint pid2, longint pid3, longint pid4) {
    bool returnVal = true;
    System& system = storage->getSystemRef();

    // ADD THE LOCAL QUADRUPLET
    Particle *p1 = storage->lookupLocalParticle(pid1);
    Particle *p2 = storage->lookupRealParticle(pid2);
    Particle *p3 = storage->lookupLocalParticle(pid3);
    Particle *p4 = storage->lookupLocalParticle(pid4);
    if (!p2){
      // Particle does not exist here, return false
      returnVal = false;
    }
    else{
      std::stringstream msg;
      if (!p1) {
        msg << "quadruple particle p1 " << pid1 << " does not exists here and cannot be added";
        throw std::runtime_error(msg.str());
      }
      if (!p3) {
        msg << "quadruple particle p3 " << pid3 << " does not exists here and cannot be added";
        throw std::runtime_error(msg.str());
      }
      if (!p4) {
        msg << "quadruple particle p4 " << pid4 << " does not exists here and cannot be added";
        throw std::runtime_error(msg.str());
      }
    }

    if (returnVal) {

      bool found = false;
      std::pair<GlobalQuadruples::const_iterator, GlobalQuadruples::const_iterator> equalRange, equalRange_rev;
      equalRange = globalQuadruples.equal_range(pid2);
      if (equalRange.first != globalQuadruples.end()) {
        // otherwise test whether the quadruple already exists
        for (GlobalQuadruples::const_iterator it = equalRange.first; it != equalRange.second && !found; ++it)
          found = found || (it->second == Triple<longint, longint, longint>(pid1, pid3, pid4));
      }
      // Check reverse order.
      if (!found) {
        equalRange_rev = globalQuadruples.equal_range(pid3);
        if (equalRange_rev.first != globalQuadruples.end()) {
          for (GlobalQuadruples::const_iterator it = equalRange_rev.first; it != equalRange_rev.second && !found; ++it) {
            found = found || (it->second == Triple<longint, longint, longint>(pid4, pid2, pid1));
          }
        }
      }

      returnVal = !found;
      if (!found) {
        // add the quadruple locally
        this->add(p1, p2, p3, p4);
        // if not, insert the new quadruple
        globalQuadruples.insert(equalRange.first,
                                std::make_pair(pid2, Triple<longint, longint, longint>(pid1, pid3, pid4)));
        onTupleAdded(pid1, pid2, pid3, pid4);
        LOG4ESPP_DEBUG(theLogger, "added fixed quadruple to global quadruple list: " << pid1 << "-" << pid2
            << "-" << pid3 << "-" << pid4);
      } else {
        LOG4ESPP_INFO(theLogger, "quadruple " << pid1 << "-" << pid2 << "-" << pid3 << "-" << pid4
            << " already exists");
      }
    }
    return returnVal;
  }


  bool FixedQuadrupleList::
  add(longint pid1, longint pid2, longint pid3, longint pid4) {
    // here we assume pid1 < pid2 < pid3 < pid4
    bool returnVal = true;
    System& system = storage->getSystemRef();
    esutil::Error err(system.comm);

    // ADD THE LOCAL QUADRUPLET
    Particle *p1 = storage->lookupLocalParticle(pid1);
    Particle *p2 = storage->lookupRealParticle(pid2);
    Particle *p3 = storage->lookupLocalParticle(pid3);
    Particle *p4 = storage->lookupLocalParticle(pid4);
    if (!p2){
      // Particle does not exist here, return false
      returnVal = false;
    }
    else{
      if (!p1) {
        std::stringstream msg;
        msg << "quadruple particle p1 " << pid1 << " does not exists here and cannot be added";
        err.setException( msg.str() );
      }
      if (!p3) {
        std::stringstream msg;
        msg << "quadruple particle p3 " << pid3 << " does not exists here and cannot be added";
        err.setException( msg.str() );
      }
      if (!p4) {
        std::stringstream msg;
        msg << "quadruple particle p4 " << pid4 << " does not exists here and cannot be added";
        err.setException( msg.str() );
      }
    }
    err.checkException();
    
    if (returnVal) {

      // ADD THE GLOBAL QUADRUPLET
      // see whether the particle already has quadruples
      bool found = false;
      std::pair<GlobalQuadruples::const_iterator, GlobalQuadruples::const_iterator> equalRange, equalRange_rev;
      equalRange = globalQuadruples.equal_range(pid2);
      if (equalRange.first != globalQuadruples.end()) {
        // otherwise test whether the quadruple already exists
        for (GlobalQuadruples::const_iterator it = equalRange.first; it != equalRange.second && !found; ++it)
          found = found || (it->second == Triple<longint, longint, longint>(pid1, pid3, pid4));
      }
      // Check reverse order.
      if (!found) {
        equalRange_rev = globalQuadruples.equal_range(pid3);
        if (equalRange_rev.first != globalQuadruples.end()) {
          for (GlobalQuadruples::const_iterator it = equalRange_rev.first;
               it != equalRange_rev.second && !found; ++it) {
            found = found || (it->second == Triple<longint, longint, longint>(pid4, pid2, pid1));
          }
        }
      }

      returnVal = !found;
      if (!found) {
        // add the quadruple locally
        this->add(p1, p2, p3, p4);
        // if not, insert the new quadruple
        globalQuadruples.insert(equalRange.first,
          std::make_pair(pid2, Triple<longint, longint, longint>(pid1, pid3, pid4)));
        onTupleAdded(pid1, pid2, pid3, pid4);
        LOG4ESPP_INFO(theLogger, "added fixed quadruple to global quadruple list: " << pid1 << "-" << pid2 << "-" << pid3 << "-" << pid4);
      } else {
        LOG4ESPP_DEBUG(theLogger, "quadruple " << pid1 << "-" << pid2 << "-" << pid3 << "-" << pid4 << " already exists");
      }
    }
    return returnVal;
  }

  bool FixedQuadrupleList::remove(longint pid1, longint pid2, longint pid3, longint pid4) {
    bool returnVal = false;
    std::pair<GlobalQuadruples::iterator, GlobalQuadruples::iterator> equalRange, equalRange_rev;
    equalRange = globalQuadruples.equal_range(pid2);
    if (equalRange.first != globalQuadruples.end()) {
      // otherwise test whether the quadruple already exists
      for (GlobalQuadruples::iterator it = equalRange.first; it != equalRange.second;)
        if (it->second == Triple<longint, longint, longint>(pid1, pid3, pid4)) {
          onTupleRemoved(pid1, pid2, pid3, pid4);
          returnVal = true;
          it = globalQuadruples.erase(it);
          LOG4ESPP_DEBUG(theLogger, "dihedral " << pid1 << "-" << pid2 << "-" << pid3 << "-" << pid4 << " removed");
        } else {
          ++it;
        }
    }
    equalRange_rev = globalQuadruples.equal_range(pid3);
    if (equalRange_rev.first != globalQuadruples.end()) {
      // otherwise test whether the quadruple already exists
      for (GlobalQuadruples::iterator it = equalRange_rev.first; it != equalRange_rev.second;)
        if (it->second == Triple<longint, longint, longint>(pid4, pid2, pid1)) {
          onTupleRemoved(pid4, pid3, pid2, pid1);
          returnVal = true;
          it = globalQuadruples.erase(it);
          LOG4ESPP_DEBUG(theLogger, "dihedral " << pid4 << pid3 << pid2 << pid1 << " removed");
        } else {
          ++it;
        }
    }
    return returnVal;
  }

  bool FixedQuadrupleList::removeByBond(longint pid1, longint pid2) {
    bool return_val = false;
    // TODO(jakub): this has to be changed in more efficient way.
    for (GlobalQuadruples::iterator it = globalQuadruples.begin(); it != globalQuadruples.end();) {
      longint q2 = it->first;
      longint q1 = it->second.first;
      longint q3 = it->second.second;
      longint q4 = it->second.third;
      if ((q1 == pid1 && q2 == pid2) || (q1 == pid2 && q2 == pid1) ||
          (q2 == pid1 && q3 == pid2) || (q2 == pid2 && q3 == pid1) ||
          (q3 == pid1 && q4 == pid2) || (q3 == pid2 && q4 == pid1)) {
        onTupleRemoved(q1, q2, q3, q4);
        LOG4ESPP_DEBUG(theLogger, "dihedral " << q1 << q2 << q3 << q4 << " removed");
        it = globalQuadruples.erase(it);
        return_val = true;
      } else {
        ++it;
      }
    }
    return return_val;
  }

  python::list FixedQuadrupleList::getQuadruples()
  {
	python::tuple quadruple;
	python::list quadruples;
	for (GlobalQuadruples::const_iterator it=globalQuadruples.begin(); it != globalQuadruples.end(); it++) {
      quadruple = python::make_tuple(it->second.first, it->first, it->second.second, it->second.third);
      quadruples.append(quadruple);
    }

	return quadruples;
  }

  std::vector<longint> FixedQuadrupleList::getQuadrupleList() {
    std::vector<longint> ret;
    for (GlobalQuadruples::const_iterator it=globalQuadruples.begin(); it != globalQuadruples.end(); it++) {
      ret.push_back(it->second.first);
      ret.push_back(it->first);
      ret.push_back(it->second.second);
      ret.push_back(it->second.third);
    }
    return ret;
  }

  python::list FixedQuadrupleList::getAllQuadruples() {
    std::vector<longint> local_quadruples;
    std::vector<std::vector<longint> > global_quadruples;
    python::list quadruples;

    for (GlobalQuadruples::const_iterator it=globalQuadruples.begin(); it != globalQuadruples.end(); it++) {
      local_quadruples.push_back(it->second.first);
      local_quadruples.push_back(it->first);
      local_quadruples.push_back(it->second.second);
      local_quadruples.push_back(it->second.third);
    }

    System& system = storage->getSystemRef();
    if (system.comm->rank() == 0) {
      mpi::gather(*system.comm, local_quadruples, global_quadruples, 0);

      for (std::vector<std::vector<longint> >::iterator it = global_quadruples.begin();
           it != global_quadruples.end(); ++it) {
        for (std::vector<longint>::iterator iit = it->begin(); iit != it->end();) {
          longint pid1 = *(iit++);
          longint pid2 = *(iit++);
          longint pid3 = *(iit++);
          longint pid4 = *(iit++);
          quadruples.append(python::make_tuple(pid1, pid2, pid3, pid4));
        }
      }
    } else {
      mpi::gather(*system.comm, local_quadruples, global_quadruples, 0);
    }
    return quadruples;
  }

  void FixedQuadrupleList::
  beforeSendParticles(ParticleList& pl, OutBuffer& buf) {
    
    std::vector< longint > toSend;
    // loop over the particle list
    for (ParticleList::Iterator pit(pl); pit.isValid(); ++pit) {
      longint pid = pit->id();
      
      // LOG4ESPP_DEBUG(theLogger, "send particle with pid " << pid << ", find quadruples");
      //printf ("me = %d: send particle with pid %d find quadruples\n", mpiWorld->rank(), pid);

      // find all quadruples that involve this particle
      int n = globalQuadruples.count(pid);
      //printf ("me = %d: send particle with pid %d, has %d global quadruples\n", 
                //mpiWorld->rank(), pid, n);

      if (n > 0) {
	    std::pair<GlobalQuadruples::const_iterator, GlobalQuadruples::const_iterator> equalRange = globalQuadruples.equal_range(pid);
	    // first write the pid of this particle
	    // then the number of partners (n)
	    // and then the pids of the partners
	    toSend.reserve(toSend.size()+3*n+1);
	    toSend.push_back(pid);
	    toSend.push_back(n);
	    for (GlobalQuadruples::const_iterator it = equalRange.first; it != equalRange.second; ++it) {
	      toSend.push_back(it->second.first);
	      toSend.push_back(it->second.second);
	      toSend.push_back(it->second.third);
          //printf ("send global quadruple: pid %d and partner %d\n", pid, it->second.first);
          //printf ("send global quadruple: pid %d and partner %d\n", pid, it->second.second);
          //printf ("send global quadruple: pid %d and partner %d\n", pid, it->second.third);
        }
	    // delete all of these quadruples from the global list
	    globalQuadruples.erase(equalRange.first, equalRange.second);
      }
    }
    // send the list
    buf.write(toSend);
    LOG4ESPP_INFO(theLogger, "prepared fixed quadruple list before send particles");
  }

  void FixedQuadrupleList::
  afterRecvParticles(ParticleList &pl, InBuffer& buf) {

    std::vector< longint > received;
    int n;
    longint pid1, pid2, pid3, pid4;
    GlobalQuadruples::iterator it = globalQuadruples.begin();
    // receive the quadruple list
    buf.read(received);
    int size = received.size(); int i = 0;
    while (i < size) {
      // unpack the list
      pid2 = received[i++];
      n = received[i++];
      //printf ("me = %d: recv particle with pid %d, has %d global quadruples\n",
                //mpiWorld->rank(), pid1, n);
      for (; n > 0; --n) {
	pid1 = received[i++];
	pid3 = received[i++];
	pid4 = received[i++];
	// add the quadruple to the global list
        //printf("received quadruple %d %d %d %d, add quadruple to global list\n", pid1, pid2, pid3, pid4);
	it = globalQuadruples.insert(it, std::make_pair(pid2,
          Triple<longint, longint, longint>(pid1, pid3, pid4)));
      }
    }
    if (i != size) {
      printf("ATTETNTION:  recv particles might have read garbage\n");
    }
    LOG4ESPP_INFO(theLogger, "received fixed quadruple list after receive particles");
  }

  void FixedQuadrupleList::onParticlesChanged() {
    
    // (re-)generate the local quadruple list from the global list
    //printf("FixedQuadrupleList: rebuild local quadruple list from global\n");
    System& system = storage->getSystemRef();
    esutil::Error err(system.comm);
    
    this->clear();
    longint lastpid2 = -1;
    Particle *p1;
    Particle *p2;
    Particle *p3;
    Particle *p4;
    for (GlobalQuadruples::const_iterator it = globalQuadruples.begin(); it != globalQuadruples.end(); ++it) {
      //printf("lookup global quadruple %d %d %d %d\n",
      //it->first, it->second.first, it->second.second, it->second.third);
      if (it->first != lastpid2) {
	  p2 = storage->lookupRealParticle(it->first);
      if (p2 == NULL) {
        std::stringstream msg;
        msg << "quadruple particle p2 " << it->first << " does not exists here";
        msg << "#" << it->second.first << "-" << it->first << "-" << it->second.second;
        msg << "-" << it->second.third;
        err.setException( msg.str() );
      }
	  lastpid2 = it->first;
      }
      p1 = storage->lookupLocalParticle(it->second.first);
      if (p1 == NULL) {
        std::stringstream msg;
        msg << "quadruple particle p1 " << it->second.first << " does not exists here";
        msg << "#" << it->second.first << "-" << it->first << "-" << it->second.second;
        msg << "-" << it->second.third;
        err.setException( msg.str() );
      }
      p3 = storage->lookupLocalParticle(it->second.second);
      if (p3 == NULL) {
        std::stringstream msg;
        msg << "quadruple particle p3 " << it->second.second << " does not exists here";
        msg << "#" << it->second.first << "-" << it->first << "-" << it->second.second;
        msg << "-" << it->second.third;
        err.setException( msg.str() );
      }
      p4 = storage->lookupLocalParticle(it->second.third);
      if (p4 == NULL) {
        std::stringstream msg;
        msg << "quadruple particle p4 " << it->second.third << " does not exists here";
        msg << "#" << it->second.first << "-" << it->first << "-" << it->second.second;
        msg << "-" << it->second.third;
        err.setException( msg.str() );
      }
      this->add(p1, p2, p3, p4);
    }
    LOG4ESPP_INFO(theLogger, "regenerated local fixed quadruple list from global list");
  }

  void FixedQuadrupleList::updateParticlesStorage() {
    // (re-)generate the local quadruple list from the global list
    System& system = storage->getSystemRef();

    this->clear();
    longint lastpid2 = -1;
    Particle *p1;
    Particle *p2;
    Particle *p3;
    Particle *p4;
    for (GlobalQuadruples::const_iterator it = globalQuadruples.begin(); it != globalQuadruples.end(); ++it) {
      if (it->first != lastpid2) {
        p2 = storage->lookupRealParticle(it->first);
        if (p2 == NULL) {
          std::stringstream msg;
          msg << "quadruple particle p2 " << it->first << " does not exists here (updateParticleStorage)";
          throw std::runtime_error(msg.str());
        }
        lastpid2 = it->first;
      }
      p1 = storage->lookupLocalParticle(it->second.first);
      if (p1 == NULL) {
        std::stringstream msg;
        msg << "quadruple particle p1 " << it->second.first << " does not exists here (updaeParticleStorage)";
        throw std::runtime_error(msg.str());
      }
      p3 = storage->lookupLocalParticle(it->second.second);
      if (p3 == NULL) {
        std::stringstream msg;
        msg << "quadruple particle p3 " << it->second.second << " does not exists here (updateParticleStorage)";
        throw std::runtime_error(msg.str());
      }
      p4 = storage->lookupLocalParticle(it->second.third);
      if (p4 == NULL) {
        std::stringstream msg;
        msg << "quadruple particle p4 " << it->second.third << " does not exists here (updateParticleStorage)";
        throw std::runtime_error(msg.str());
      }
      this->add(p1, p2, p3, p4);
    }
    LOG4ESPP_INFO(theLogger, "regenerated local fixed quadruple list from global list");
  }

  int FixedQuadrupleList::totalSize() {
    int local_size = globalQuadruples.size();
    int global_size;
    System& system = storage->getSystemRef();
    mpi::all_reduce(*system.comm, local_size, global_size, std::plus<int>());
    return global_size;
  }


  /****************************************************
  ** REGISTRATION WITH PYTHON
  ****************************************************/

  void FixedQuadrupleList::registerPython() {

    using namespace espressopp::python;

    bool (FixedQuadrupleList::*pyAdd)(longint pid1, longint pid2,
           longint pid3, longint pid4) = &FixedQuadrupleList::add;
    //bool (FixedQuadrupleList::*pyAdd)(pvec pids)
    //          = &FixedQuadrupleList::add;

    class_< FixedQuadrupleList, shared_ptr< FixedQuadrupleList >, boost::noncopyable >
      ("FixedQuadrupleList", init< shared_ptr< storage::Storage > >())
      .def("add", pyAdd)
      .def("size", &FixedQuadrupleList::size)
      .def("totalSize", &FixedQuadrupleList::totalSize)
      .def("getQuadruples",  &FixedQuadrupleList::getQuadruples)
      .def("getAllQuadruples", &FixedQuadrupleList::getAllQuadruples)
     ;
  }
}
