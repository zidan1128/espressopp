// ESPP_CLASS
#ifndef _INTERACTION_INTERACTION_HPP
#define _INTERACTION_INTERACTION_HPP

#include "types.hpp"
#include "logging.hpp"
#include "esutil/ESPPIterator.hpp"

namespace espresso {
  namespace interaction {

    enum bondTypes {unused, Nonbonded, Pair, Angular, Dihedral};

    /** Interaction base class. */

    class Interaction {

    public:
      virtual ~Interaction() {};
      virtual void addForces() = 0;
      virtual real computeEnergy() = 0;
      virtual real computeVirial() = 0;
      virtual void computeVirialTensor(Tensor& w) = 0;

      // this should compute the virial locally around a surface which crosses the box at
      // z (according to the method of Irving and Kirkwood)
      virtual void computeVirialTensor(Tensor& w, real z) = 0;
      // the same Irving - Kirkwood method, but Z direction is divided by n planes
      virtual void computeVirialTensor(Tensor *w, int n) = 0;

      /** This method returns the maximal cutoff defined for one type pair. */
      virtual real getMaxCutoff() = 0;
      virtual int bondType() = 0;

      static void registerPython();

    protected:
      /** Logger */
      static LOG4ESPP_DECL_LOGGER(theLogger);
    };

    struct InteractionList
      : public std::vector< shared_ptr< Interaction > > {
      typedef esutil::ESPPIterator< std::vector< Interaction > > Iterator;
    };


  }
}

#endif
