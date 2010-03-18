#include "bindings.hpp"
#include "Storage.hpp"
#include "DomainDecomposition.hpp"

namespace espresso {
  namespace storage {
    void registerPython() {
      Storage::registerPython();
      DomainDecomposition::registerPython();
    }
  }
}
