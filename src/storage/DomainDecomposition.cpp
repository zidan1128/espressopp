#define LOG4ESPP_LEVEL_DEBUG
#include <algorithm>
#include <cmath>
#include "log4espp.hpp"
#include "System.hpp"
#include "python.hpp"

#include "Real3D.hpp"
#include "DomainDecomposition.hpp"
#include "bc/BC.hpp"
#include "Int3D.hpp"

using namespace boost;

namespace espresso { 
  namespace storage {

    const int DD_COMM_TAG = 0xab;

    LOG4ESPP_LOGGER(DomainDecomposition::logger, "DomainDecomposition");

    NodeGridMismatch::NodeGridMismatch()
      : std::runtime_error("specified node grid does not match number of nodes in the communicator") {
    }

    DomainDecomposition::DomainDecomposition(shared_ptr< System > _system,
					     const mpi::communicator &_comm,
					     const ConstInt3DRef _nodeGrid,
					     const ConstInt3DRef _cellGrid)
      : Storage(_system, _comm), exchangeBufferSize(0) {
      LOG4ESPP_INFO(logger, "node grid = "
		    << _nodeGrid[0] << "x" << _nodeGrid[1] << "x" << _nodeGrid[2]
		    << " cell grid = "
		    << _cellGrid[0] << "x" << _cellGrid[1] << "x" << _cellGrid[2]);

      createCellGrid(_nodeGrid, _cellGrid);
      initCellInteractions();
      prepareGhostCommunication();
      LOG4ESPP_DEBUG(logger, "done");
    }

    void DomainDecomposition::createCellGrid(const ConstInt3DRef _nodeGrid, const ConstInt3DRef _cellGrid) {
      real myLeft[3];
      real myRight[3];

      nodeGrid = NodeGrid(_nodeGrid, comm.rank(), system.lock()->bc->getBoxL());

      if (nodeGrid.getNumberOfCells() != comm.size()) {
	throw NodeGridMismatch();
      }

      LOG4ESPP_INFO(logger, "my node grid position: "
		    << nodeGrid.getNodePosition(0) << " "
		    << nodeGrid.getNodePosition(1) << " "
		    << nodeGrid.getNodePosition(2) << " -> "
		    << comm.rank());

      LOG4ESPP_DEBUG(logger, "my neighbors: "
		     << nodeGrid.getNodeNeighbor(0) << "<->"
		     << nodeGrid.getNodeNeighbor(1) << ", "
		     << nodeGrid.getNodeNeighbor(2) << "<->"
		     << nodeGrid.getNodeNeighbor(3) << ", "
		     << nodeGrid.getNodeNeighbor(4) << "<->"
		     << nodeGrid.getNodeNeighbor(5));

      for (int i = 0; i < 3; ++i) {
	myLeft[i] = nodeGrid.getMyLeft(i);
	myRight[i] = nodeGrid.getMyRight(i);
      }

      cellGrid = CellGrid(_cellGrid, myLeft, myRight, 1);

      LOG4ESPP_INFO(logger, "local box "
		    << myLeft[0] << "-" << myRight[0] << ", "
		    << myLeft[1] << "-" << myRight[1] << ", "
		    << myLeft[2] << "-" << myRight[2]);

      longint nLocalCells = 1;
      longint nRealCells = 1;
      for (int i = 0; i < 3; ++i) {
	nRealCells *= cellGrid.getGridSize(i);
	nLocalCells *= cellGrid.getFrameGridSize(i);
      }

      resizeCells(nLocalCells);

      realCells.reserve(nRealCells);
      ghostCells.reserve(nLocalCells - nRealCells);

      markCells();

      LOG4ESPP_DEBUG(logger, "total # cells=" << nLocalCells
		     << ", # real cells=" << nRealCells
		     << ", frame cell grid = (" << cellGrid.getFrameGridSize(0)
		     << ", " << cellGrid.getFrameGridSize(1)
		     << ", " << cellGrid.getFrameGridSize(2)
		     << ")");
    }

    void DomainDecomposition::markCells() {
      realCells.resize(0);
      ghostCells.resize(0);

      for (int o = 0; o < cellGrid.getFrameGridSize(2); ++o) {
	for (int n = 0; n < cellGrid.getFrameGridSize(1); ++n) {
	  for (int m = 0; m < cellGrid.getFrameGridSize(0); ++m) {
	    Cell *cur = &cells[cellGrid.mapPositionToIndex(m, n, o)];
	    if (cellGrid.isInnerCell(m, n, o)) {
	      LOG4ESPP_TRACE(logger, "cell " << (cur - &cells[0]) << " is inner cell (" << m << ", " << n << ", " << o << ")");
	      realCells.push_back(cur);
	    } else {
	      LOG4ESPP_TRACE(logger, "cell " << (cur - &cells[0]) << " is ghost cell (" << m << ", " << n << ", " << o << ")");
	      ghostCells.push_back(cur);
	    }
	  }
	}
      }
    }

    void DomainDecomposition::initCellInteractions() {
      LOG4ESPP_DEBUG(logger, "setting up neighbors for " << cells.size() << " cells");

      for (int o = cellGrid.getInnerCellsBegin(2); o < cellGrid.getInnerCellsEnd(2); ++o) {
	for (int n = cellGrid.getInnerCellsBegin(1); n < cellGrid.getInnerCellsEnd(1); ++n) {
	  for (int m = cellGrid.getInnerCellsBegin(0); m < cellGrid.getInnerCellsEnd(0); ++m) {
	    longint cellIdx = cellGrid.mapPositionToIndex(m, n, o);
	    Cell *cell = &cells[cellIdx];

	    LOG4ESPP_TRACE(logger, "setting up neighbors for cell " << cell - getFirstCell()
			   << " @ " << m << " " << n << " " << o);
	    
	    // there should be always 26 neighbors
	    cell->neighborCells.reserve(26);

	    // loop all neighbor cells
	    for (int p = o - 1; p <= o + 1; ++p) {
	      for (int q = n - 1; q <= n + 1; ++q) {
		for (int r = m - 1; r <= m + 1; ++r) {
		  if (p != o || q != n || r != m) {
		    longint cell2Idx = cellGrid.mapPositionToIndex(r, q, p);
		    Cell *cell2 = &cells[cell2Idx];
		    cell->neighborCells.push_back(NeighborCellInfo(cell2, (cell2Idx<cellIdx)));

		    LOG4ESPP_TRACE(logger, "neighbor cell " << cell2 - getFirstCell()
				   << " @ " << r << " " << q << " " << p << ((cell2Idx<cellIdx) ? " is" : " is not") << " taken" );
		  }
		}
	      }
	    }
	  }
	}
      }

      LOG4ESPP_DEBUG(logger, "done");
    }

    Cell *DomainDecomposition::mapPositionToCellClipped(const ConstReal3DRef pos) {
      return &cells[cellGrid.mapPositionToCellClipped(pos)];
    }

    Cell *DomainDecomposition::mapPositionToCellChecked(const ConstReal3DRef pos) {
      longint c = cellGrid.mapPositionToCellChecked(pos);
      if (c == CellGrid::noCell) {
	return 0;
      } else {
	return &cells[c];
      }
    }

    bool DomainDecomposition::appendParticles(ParticleList &l, int dir) {
      bool outlier = false;

      LOG4ESPP_DEBUG(logger, "got " << l.size() << " particles");

      for (ParticleList::iterator it = l.begin(),
	     end = l.end(); it != end; ++it) {

	if (nodeGrid.getBoundary(dir) != 0) {
	  system.lock()->bc->foldCoordinate(it->r.p, it->l.i, nodeGrid.convertDirToCoord(dir));
	  LOG4ESPP_TRACE(logger, "folded coordinate " << nodeGrid.convertDirToCoord(dir) << " of particle " << it->p.id);
	}

	longint cell;
	if (cellGrid.mapPositionToCellCheckedAndClipped(cell, it->r.p)) {
	  LOG4ESPP_TRACE(logger, "particle " << it->p.id
			 << " @ " << it->r.p[0] << ", " << it->r.p[1] << ", "
			 << it->r.p[2] << " is not inside node domain");
	  outlier = true;
	}

	LOG4ESPP_TRACE(logger, "append part " << it->p.id << " to cell "
		       << cell);

	appendIndexedParticle(cells[cell].particles, *it);
      }
      return outlier;
    }

    void DomainDecomposition::resortRealParticles() {
      LOG4ESPP_DEBUG(logger, "starting, expected comm buffer size " << exchangeBufferSize);

      // allocate send/recv buffers. We use the size as we need maximally so far, to avoid reallocation
      ParticleList sendBufL;
      sendBufL.reserve(exchangeBufferSize);
      ParticleList sendBufR;
      sendBufR.reserve(exchangeBufferSize);
      ParticleList recvBufL;
      recvBufL.reserve(exchangeBufferSize);
      ParticleList recvBufR;
      recvBufR.reserve(exchangeBufferSize);

      int nNodesFinished;
      do {
	int finished = 1;

	for (int coord = 0; coord < 3; ++coord) {
	  LOG4ESPP_DEBUG(logger, "starting with direction " << coord);

	  if (nodeGrid.getGridSize(coord) > 1) {
	    for (std::vector<Cell*>::iterator it = realCells.begin(),
		   end = realCells.end(); it != end; ++it) {
	      Cell &cell = **it;
	      // do not use an iterator here, since we have need to take out particles during the loop
	      for (size_t p = 0; p < cell.particles.size(); ++p) {
		Particle &part = cell.particles[p];
		if (part.r.p[coord] - cellGrid.getMyLeft(coord) < -ROUND_ERROR_PREC) {
		  LOG4ESPP_TRACE(logger, "send particle left " << part.p.id);
		  moveIndexedParticle(sendBufL, cell.particles, p);
		  localParticles.erase(part.p.id);
		  // redo same particle since we took one out here, so it's a new one
		  --p;
		} else if (part.r.p[coord] - cellGrid.getMyRight(coord) >= ROUND_ERROR_PREC) {
		  LOG4ESPP_TRACE(logger, "send particle right " << part.p.id);
		  moveIndexedParticle(sendBufR, cell.particles, p);
		  localParticles.erase(part.p.id);
		  --p;
		}                                // Sort particles in cells of this node during last direction
		else if (coord == 2) {
		  Cell *sortCell = mapPositionToCellChecked(part.r.p);
		  if (sortCell != &cell) {
		    if (sortCell == 0) {
		      LOG4ESPP_DEBUG(logger, "take another loop: particle " << part.p.id
				     << " @ " << part.r.p[0] << ", " << part.r.p[1] << ", "
				     << part.r.p[2] << " is not inside node domain after neighbor exchange");
		      if (isnan(part.r.p[0]) || isnan(part.r.p[1]) || isnan(part.r.p[2])) {
			LOG4ESPP_ERROR(logger, "particle " << part.p.id << " has moved to outer space (one or more coordinates are nan)");
		      } else {
			// particle stays where it is, and will be sorted in the next round
			finished = 0;
		      }
		    } else {
		      moveIndexedParticle(sortCell->particles, cell.particles, p);
		      --p;
		    }
		  }
		}
	      }
	    }

	    // Exchange particles, odd-even rule
	    if (nodeGrid.getNodePosition(coord) % 2 == 0) {
	      sendParticles(sendBufL, nodeGrid.getNodeNeighbor(2 * coord));
	      recvParticles(recvBufR, nodeGrid.getNodeNeighbor(2 * coord + 1));
	      sendParticles(sendBufR, nodeGrid.getNodeNeighbor(2 * coord + 1));
	      recvParticles(recvBufL, nodeGrid.getNodeNeighbor(2 * coord));
	    } else {
	      recvParticles(recvBufR, nodeGrid.getNodeNeighbor(2 * coord + 1));
	      sendParticles(sendBufL, nodeGrid.getNodeNeighbor(2 * coord));
	      recvParticles(recvBufL, nodeGrid.getNodeNeighbor(2 * coord));
	      sendParticles(sendBufR, nodeGrid.getNodeNeighbor(2 * coord + 1));
	    }

	    // sort received particles to cells
	    if (appendParticles(recvBufL, 2 * coord) && coord == 2) finished = 0;
	    if (appendParticles(recvBufR, 2 * coord + 1) && coord == 2) finished = 0;

	    // reset send/recv buffers
	    sendBufL.resize(0);
	    sendBufR.resize(0);
	    recvBufL.resize(0);
	    recvBufR.resize(0);
	  } else {
	    /* Single node direction case (no communication)
	       Fold particles that have left the box */
	    for (std::vector<Cell*>::iterator it = realCells.begin(),
		   end = realCells.end(); it != end; ++it) {
	      Cell &cell = **it;
	      // do not use an iterator here, since we have need to take out particles during the loop
	      for (size_t p = 0; p < cell.particles.size(); ++p) {
		Particle &part = cell.particles[p];
		system.lock()->bc->foldCoordinate(part.r.p, part.l.i, coord);
		LOG4ESPP_TRACE(logger, "folded coordinate " << coord << " of particle " << part.p.id);

		if (coord == 2) {
		  Cell *sortCell = mapPositionToCellChecked(part.r.p);

		  if (sortCell != &cell) {
		    if (sortCell == 0) {
		      LOG4ESPP_DEBUG(logger, "take another loop: particle " << part.p.id
				     << " @ " << part.r.p[0] << ", " << part.r.p[1] << ", "
				     << part.r.p[2] << " is not inside node domain after neighbor exchange");
		      if (isnan(part.r.p[0]) || isnan(part.r.p[1]) || isnan(part.r.p[2])) {
			LOG4ESPP_ERROR(logger, "particle " << part.p.id << " has moved to outer space (one or more coordinates are nan)");
		      } else {
			// particle stays where it is, and will be sorted in the next round
			finished = 0;
		      }
		    } else {
		      moveIndexedParticle(sortCell->particles, cell.particles, p);
		      --p;
		    }
		  }
		}
	      }
	    }
	  }

	  LOG4ESPP_DEBUG(logger, "done with direction " << coord);
	}

	// Communicate wether particle exchange is finished
	mpi::all_reduce(comm, finished, nNodesFinished, std::plus<int>());
      } while (nNodesFinished < comm.size());

      exchangeBufferSize = std::max(exchangeBufferSize,
				    std::max(sendBufL.capacity(),
					     std::max(sendBufR.capacity(),
						      std::max(recvBufL.capacity(),
							       recvBufR.capacity()))));

      LOG4ESPP_DEBUG(logger, "finished exchanging particles, new send/recv buffer size " << exchangeBufferSize);

      LOG4ESPP_DEBUG(logger, "starting to exchange full ghost information");

      LOG4ESPP_DEBUG(logger, "done");
    }

    void DomainDecomposition::exchangeGhosts() {
      doGhostCommunication(true, true, dataOfExchangeGhosts);
    }

    void DomainDecomposition::updateGhosts() {
      doGhostCommunication(false, true, dataOfUpdateGhosts);
    }

    void DomainDecomposition::collectGhostForces() {
      doGhostCommunication(false, false);
    }

    void DomainDecomposition::fillCells(std::vector<Cell *> &cv,
					const int leftBoundary[3],
					const int rightBoundary[3]) {
      LOG4ESPP_DEBUG(logger, "filling: "
		     << leftBoundary[0] << "-" << (rightBoundary[0] - 1) << " "
		     << leftBoundary[1] << "-" << (rightBoundary[1] - 1) << " "
		     << leftBoundary[2] << "-" << (rightBoundary[2] - 1));

      longint total = 1;
      for (int i = 0; i < 3; ++i) {
	if (leftBoundary[i] < 0 || leftBoundary[i] > cellGrid.getFrameGridSize(i) ||
	    rightBoundary[i] < 0 || rightBoundary[i] > cellGrid.getFrameGridSize(i) ||
	    leftBoundary[i] >= rightBoundary[i]) {
	  throw std::runtime_error("DomainDecomposition::fillCells: wrong cell grid specified internally");
	}
	total *= (rightBoundary[i] - leftBoundary[i]);
      }
      cv.reserve(total);

      for (int o = leftBoundary[0]; o < rightBoundary[0]; ++o) {
	for (int n = leftBoundary[1]; n < rightBoundary[1]; ++n) {
	  for (int m = leftBoundary[2]; m < rightBoundary[2]; ++m) {
	    int i = cellGrid.mapPositionToIndex(o, n, m);
	    LOG4ESPP_TRACE(logger, "add cell " << i);
	    cv.push_back(&cells[i]);
	  }
	}
      }

      LOG4ESPP_DEBUG(logger, "expected " << total << " cells, filled with " << cv.size());
    }

    void DomainDecomposition::prepareGhostCommunication() {
      // direction loop: x, y, z
      for (int coord = 0; coord < 3; ++coord) {
	// boundaries of area to send
	int leftBoundary[3], rightBoundary[3];
	/* boundaries perpendicular directions are the same for left/right send.
	   We also send the ghost frame that we have already, so the data amount
	   increase with each cycle.

	   For a direction that was done already, i.e. is smaller than dir,
	   we take the full ghost frame, otherwise only the inner frame.
	*/
	for (int offset = 1; offset <= 2; ++offset) {
	  int otherCoord = (coord + offset) % 3;
	  if (otherCoord < coord) {
	    leftBoundary[otherCoord] = 0;
	    rightBoundary[otherCoord] = cellGrid.getFrameGridSize(otherCoord);
	  } else {
	    leftBoundary[otherCoord] = cellGrid.getInnerCellsBegin(otherCoord);
	    rightBoundary[otherCoord] = cellGrid.getInnerCellsEnd(otherCoord);
	  }
	}

	//  lr loop: left right - loop
	for (int lr = 0; lr < 2; ++lr) {
	  int dir = 2 * coord + lr;

	  /* participating real particles from this node */
	  LOG4ESPP_DEBUG(logger, "direction " << dir << " reals");

	  if (lr == 0) {
	    leftBoundary[coord] = cellGrid.getInnerCellsBegin(coord);
	    rightBoundary[coord] = cellGrid.getInnerCellsBegin(coord) + cellGrid.getFrameWidth();
	  } else {
	    leftBoundary[coord] = cellGrid.getInnerCellsEnd(coord) - cellGrid.getFrameWidth();
	    rightBoundary[coord] = cellGrid.getInnerCellsEnd(coord);
	  }
	  fillCells(commCells[dir].reals, leftBoundary, rightBoundary);

	  /* participating ghosts from this node */
	  LOG4ESPP_DEBUG(logger, "direction " << dir << " ghosts");

	  if (lr == 0) {
	    leftBoundary[coord] = cellGrid.getInnerCellsEnd(coord);
	    rightBoundary[coord] = cellGrid.getInnerCellsEnd(coord) + cellGrid.getFrameWidth();
	  } else {
	    leftBoundary[coord] = cellGrid.getInnerCellsBegin(coord) - cellGrid.getFrameWidth();
	    rightBoundary[coord] = cellGrid.getInnerCellsBegin(coord);
	  }
	  fillCells(commCells[dir].ghosts, leftBoundary, rightBoundary);
	}
      }
    }

    void DomainDecomposition::doGhostCommunication(bool sizesFirst,
						   bool realToGhosts,
						   int extradata) {
      LOG4ESPP_DEBUG(logger, "do ghost communication " << (sizesFirst ? "with sizes " : "") << (realToGhosts ? "reals to ghosts " : "ghosts to reals ") << extradata);
      
      /* direction loop: x, y, z.
	 Here we could in principle build in a one sided ghost
	 communication, simply by taking the lr loop only over one
	 value. */
      for (int _coord = 0; _coord < 3; ++_coord) {
	/* inverted processing order for ghost force communication,
	   since the corner ghosts have to be collected via several
	   nodes. We now add back the corner ghost forces first again
	   to ghost forces, which only eventually go back to the real
	   particle.
	*/
	int coord = realToGhosts ? _coord : (2 - _coord);
	real curCoordBoxL = system.lock()->bc->getBoxL(coord);

	// lr loop: left right
	for (int lr = 0; lr < 2; ++lr) {
	  int dir         = 2 * coord + lr;
	  int oppositeDir = 2 * coord + (1 - lr);

	  real shift[3] = {0, 0, 0};
	  shift[coord] = nodeGrid.getBoundary(dir) * curCoordBoxL;

	  LOG4ESPP_DEBUG(logger, "direction " << dir);

	  if (nodeGrid.getGridSize(coord) == 1) {
	    LOG4ESPP_DEBUG(logger, "local communication");

	    // copy operation, we have to receive as many cells as we send
	    if (commCells[dir].ghosts.size() != commCells[dir].reals.size()) {
	      throw std::runtime_error("DomainDecomposition::doGhostCommunication: send/recv cell structure mismatch during local copy");
	    }

	    for (int i = 0, end = commCells[dir].ghosts.size(); i < end; ++i) {
	      if (realToGhosts) {
		copyRealsToGhosts(*commCells[dir].reals[i], *commCells[dir].ghosts[i], extradata, shift);
	      } else {
		addGhostForcesToReals(*commCells[dir].ghosts[i], *commCells[dir].reals[i]);
	      }
	    }
	  } else {
	    // exchange size information, if necessary
	    if (sizesFirst) {
	      LOG4ESPP_DEBUG(logger, "exchanging ghost cell sizes");

	      // prepare buffers
	      std::vector<longint> sendSizes, recvSizes;
	      sendSizes.reserve(commCells[dir].reals.size());
	      for (int i = 0, end = commCells[dir].reals.size(); i < end; ++i) {
		sendSizes.push_back(commCells[dir].reals[i]->particles.size());
	      }
	      recvSizes.resize(commCells[dir].ghosts.size());

	      // exchange sizes, odd-even rule
	      if (nodeGrid.getNodePosition(coord) % 2 == 0) {
		LOG4ESPP_DEBUG(logger, "sending to node " << nodeGrid.getNodeNeighbor(dir)
			       << ", then receiving from node " << nodeGrid.getNodeNeighbor(oppositeDir));
		comm.send(nodeGrid.getNodeNeighbor(dir), DD_COMM_TAG, &(sendSizes[0]), sendSizes.size());
		comm.recv(nodeGrid.getNodeNeighbor(oppositeDir), DD_COMM_TAG, &(recvSizes[0]), recvSizes.size());
	      }
	      else {
		LOG4ESPP_DEBUG(logger, "receiving from node " << nodeGrid.getNodeNeighbor(oppositeDir)
			       << ", then sending to node " << nodeGrid.getNodeNeighbor(dir));
		comm.recv(nodeGrid.getNodeNeighbor(oppositeDir), DD_COMM_TAG, &(recvSizes[0]), recvSizes.size());
		comm.send(nodeGrid.getNodeNeighbor(dir), DD_COMM_TAG, &(sendSizes[0]), sendSizes.size());
	      }

	      // resize according to received information
	      for (int i = 0, end = commCells[dir].ghosts.size(); i < end; ++i) {
		commCells[dir].ghosts[i]->particles.resize(recvSizes[i]);
	      }
	      LOG4ESPP_DEBUG(logger, "exchanging ghost cell sizes done");
	    }

	    // prepare send and receive buffers
	    longint oarRecver, iarSender;
	    boost::mpi::packed_oarchive oar(comm);
	    boost::mpi::packed_iarchive iar(comm);
	    if (realToGhosts) {
	      oarRecver = nodeGrid.getNodeNeighbor(dir);
	      iarSender = nodeGrid.getNodeNeighbor(oppositeDir);
	      for (int i = 0, end = commCells[dir].reals.size(); i < end; ++i) {
		packPositionsEtc(oar, *commCells[dir].reals[i], extradata, shift);
	      }
	    }
	    else {
	      oarRecver = nodeGrid.getNodeNeighbor(oppositeDir);
	      iarSender = nodeGrid.getNodeNeighbor(dir);
	      for (int i = 0, end = commCells[dir].ghosts.size(); i < end; ++i) {
		packForces(oar, *commCells[dir].ghosts[i]);
	      }
	    }

	    // exchange particles, odd-even rule
	    if (nodeGrid.getNodePosition(coord) % 2 == 0) {
	      comm.send(oarRecver, DD_COMM_TAG, oar);
	      comm.recv(iarSender, DD_COMM_TAG, iar);
	    } else {
	      comm.recv(iarSender, DD_COMM_TAG, iar);
	      comm.send(oarRecver, DD_COMM_TAG, oar);
	    }

	    // unpack received data
	    if (realToGhosts) {
	      for (int i = 0, end = commCells[dir].reals.size(); i < end; ++i) {
		unpackPositionsEtc(*commCells[dir].ghosts[i], iar, extradata);
	      }
	    }
	    else {
	      for (int i = 0, end = commCells[dir].reals.size(); i < end; ++i) {
		unpackAndAddForces(*commCells[dir].reals[i], iar);
	      }
	    }
	  }
	}
      }
      LOG4ESPP_DEBUG(logger, "ghost communication finished");
    }
 


    //////////////////////////////////////////////////
    // REGISTRATION WITH PYTHON
    //////////////////////////////////////////////////

    void
    DomainDecomposition::registerPython() {
      using namespace espresso::python;
      class_< DomainDecomposition, boost::noncopyable >("storage_DomainDecomposition", no_init)
      ;
    }
  }
}
