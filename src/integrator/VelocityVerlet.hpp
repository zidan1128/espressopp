#ifndef _VELOCITY_VERLET
#define _VELOCITY_VERLET

#include <boost/signals2.hpp>

#include "types.hpp"
#include "MDIntegrator.hpp"

namespace espresso {
  namespace integrator {

    class VelocityVerlet: public MDIntegrator {

    private:

      /* on change:

      struct ForceEvaluation {
        boost::shared_ptr<interaction::Interaction> interaction;
        boost::shared_ptr<pairs::Set> pairs;
        ForceEvaluation(boost::shared_ptr<interaction::Interaction> _interaction,
                        boost::shared_ptr<pairs::Set> _pairs)
          : interaction(_interaction), pairs(_pairs) {}
      };

      std::vector<ForceEvaluation> forceEvaluations;

      */

    protected:

      void runSingleStep();

    public:

      VelocityVerlet(particles::PSet particles,
		     PReal3DProperty position,
		     PReal3DProperty velocity,
		     PReal3DProperty force);

      static void registerPython();

      /** The following signals are specific for VelocityVerlet */

      typedef boost::signals2::signal1<void, const VelocityVerlet&> VelocityVerletSignal;

      VelocityVerletSignal updateVelocity1;

      VelocityVerletSignal updateVelocity2;

      virtual ~VelocityVerlet();

   };

    typedef boost::shared_ptr< VelocityVerlet > PVelocityVerlet;

  }
}

#endif
