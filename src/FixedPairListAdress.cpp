#include "storage/Storage.hpp"
#include <boost/bind.hpp>
#include "FixedPairListAdress.hpp"
#include "Buffer.hpp"
#include "esutil/Error.hpp"

//using namespace std;

namespace espresso {

  LOG4ESPP_LOGGER(FixedPairListAdress::theLogger, "FixedPairListAdress");


  FixedPairListAdress::
  FixedPairListAdress(shared_ptr< storage::Storage > _storage,
          shared_ptr<FixedTupleList> _fixedtupleList)
          : FixedPairList(_storage), fixedtupleList(_fixedtupleList) {
    LOG4ESPP_INFO(theLogger, "construct FixedPairListAdress");
    
    con = fixedtupleList->beforeSendATParticles.connect
          (boost::bind(&FixedPairListAdress::beforeSendATParticles, this, _1, _2));
  }

  FixedPairListAdress::~FixedPairListAdress() {

    LOG4ESPP_INFO(theLogger, "~FixedPairListAdress");

    con.disconnect();
  }


  // override parent function (use lookupAdrATParticle)
  bool FixedPairListAdress::add(longint pid1, longint pid2) {

    if (pid1 > pid2)
      std::swap(pid1, pid2);

    bool returnVal = true;
    System& system = storage->getSystemRef();
    esutil::Error err(system.comm);
    
    // ADD THE LOCAL PAIR
    Particle *p1 = storage->lookupAdrATParticle(pid1);
    Particle *p2 = storage->lookupAdrATParticle(pid2);
    if (!p1){
      // Particle does not exist here, return false
      returnVal = false;
    }
    else{
      if (!p2) {
        std::stringstream msg;
        msg << "atomistic bond particle p2 " << pid2 << " does not exists here and cannot be added";
        //std::runtime_error(err.str());
        err.setException( msg.str() );
      }
    }
    err.checkException();
    
    if(returnVal){
      // add the pair locally
      this->add(p1, p2);
      // ADD THE GLOBAL PAIR
      // see whether the particle already has pairs
      std::pair<GlobalPairs::const_iterator,
        GlobalPairs::const_iterator> equalRange
        = globalPairs.equal_range(pid1);
      if (equalRange.first == globalPairs.end()) {
        // if it hasn't, insert the new pair
        globalPairs.insert(std::make_pair(pid1, pid2));
      }
      else {
        // otherwise test whether the pair already exists
        for (GlobalPairs::const_iterator it = equalRange.first; it != equalRange.second; ++it) {
  	      if (it->second == pid2) {
  	        // TODO: Pair already exists, generate error!
  	        ;
  	      }
        }
        // if not, insert the new pair
        globalPairs.insert(equalRange.first, std::make_pair(pid1, pid2));
      }
      LOG4ESPP_INFO(theLogger, "added fixed pair to global pair list");
    }
    return returnVal;
  }


  void FixedPairListAdress::beforeSendATParticles(std::vector<longint>& atpl,
          OutBuffer& buf) {

        //std::cout << "beforeSendATParticles() fixed pl (size " << atpl.size() << ")\n";

        std::vector< longint > toSend;

        // loop over the VP particle list
        for (std::vector<longint>::iterator it = atpl.begin();
                it != atpl.end(); ++it) {
          longint pid = *it;

          LOG4ESPP_DEBUG(theLogger, "send particle with pid " << pid << ", find pairs");

          // find all pairs that involve this particle

          int n = globalPairs.count(pid);

          if (n > 0) {
            std::pair<GlobalPairs::const_iterator,
              GlobalPairs::const_iterator> equalRange
              = globalPairs.equal_range(pid);

            // first write the pid of the first particle
            // then the number of partners
            // and then the pids of the partners
            toSend.reserve(toSend.size()+n+1);
            toSend.push_back(pid);
            toSend.push_back(n);
            for (GlobalPairs::const_iterator it = equalRange.first;
                 it != equalRange.second; ++it) {
              toSend.push_back(it->second);
              LOG4ESPP_DEBUG(theLogger, "send global bond: pid "
                           << pid << " and partner " << it->second);
            }

            // delete all of these pairs from the global list
            globalPairs.erase(pid);
          }
        }

        // send the list
        buf.write(toSend);
        LOG4ESPP_INFO(theLogger, "prepared fixed pair list before send particles");
  }


  // override parent function, this one should be empty
  void FixedPairListAdress::beforeSendParticles(ParticleList& pl,
                                                    OutBuffer& buf) {
        //std::cout << storage->getSystem()->comm->rank() << ": beforeSendParticles() fixed pl (size " << pl.size() << ")\n";
  }


  // override parent function (use lookupAdrATParticle())
  void FixedPairListAdress::onParticlesChanged() {

    LOG4ESPP_INFO(theLogger, "rebuild local bond list from global\n");

    System& system = storage->getSystemRef();
    esutil::Error err(system.comm);
    
    this->clear();
    longint lastpid1 = -1;
    Particle *p1;
    Particle *p2;

    for (GlobalPairs::const_iterator it = globalPairs.begin();
	 it != globalPairs.end(); ++it) {

        if (it->first != lastpid1) {
            p1 = storage->lookupAdrATParticle(it->first);
            if (p1 == NULL) {
                std::stringstream msg;
                msg << "atomistic bond particle p1 " << it->first << " does not exists here";
                //std::runtime_error(err.str());
                err.setException( msg.str() );
            }
            lastpid1 = it->first;
        }

        p2 = storage->lookupAdrATParticle(it->second);
        if (p2 == NULL) {
            std::stringstream msg;
            msg << "atomistic bond particle p2 " << it->second << " does not exists here";
            //std::runtime_error(err.str());
            err.setException( msg.str() );
        }

        //std::cout << " adding (" << p1->getId() << ", " << p2->getId() << ")\n";
        this->add(p1, p2);
    }
    err.checkException();
    
    LOG4ESPP_INFO(theLogger, "regenerated local fixed pair list from global list");
  }

  /****************************************************
  ** REGISTRATION WITH PYTHON
  ****************************************************/

  void FixedPairListAdress::registerPython() {

    using namespace espresso::python;

    bool (FixedPairListAdress::*pyAdd)(longint pid1, longint pid2)
      = &FixedPairListAdress::add;

    class_<FixedPairListAdress, shared_ptr<FixedPairListAdress> >
      ("FixedPairListAdress",
              init <shared_ptr<storage::Storage>,
                     shared_ptr<FixedTupleList> >())
      .def("add", pyAdd)
      ;
  }

}
