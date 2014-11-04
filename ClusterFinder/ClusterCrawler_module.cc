////////////////////////////////////////////////////////////////////////
// Class:       ClusterCrawler
// Module Type: producer
// File:        ClusterCrawler_module.cc
//
// Generated at Fri Jun  7 09:44:09 2013 by Bruce Baller using artmod 
// from cetpkgsupport v1_02_00.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "art/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"

#include <memory>

//LArSoft includes
#include "SimpleTypesAndConstants/geo_types.h"
#include "Geometry/Geometry.h"
#include "Geometry/CryostatGeo.h"
#include "Geometry/TPCGeo.h"
#include "Geometry/PlaneGeo.h"
#include "RecoBase/Cluster.h"
#include "RecoBase/Hit.h"
#include "RecoBase/EndPoint2D.h"
#include "RecoBase/Vertex.h"
#include "Utilities/AssociationUtil.h"
#include "RecoAlg/CCHitFinderAlg.h"
#include "RecoAlg/ClusterCrawlerAlg.h"
//#include "RecoAlg/CCHitRefinerAlg.h"


namespace cluster {
  class ClusterCrawler;
}

class cluster::ClusterCrawler : public art::EDProducer {

  public:
    explicit ClusterCrawler(fhicl::ParameterSet const & pset);
    virtual ~ClusterCrawler();

    void reconfigure(fhicl::ParameterSet const & pset) override;
    void produce(art::Event & evt) override;
    void beginJob();

  private:
    CCHitFinderAlg fCCHFAlg; // define CCHitFinderAlg object
    ClusterCrawlerAlg fCCAlg; // define ClusterCrawlerAlg object
//    CCHitRefinerAlg fCCHRAlg; // define CCHitRefinerAlg object    
};


namespace cluster {

  ClusterCrawler::ClusterCrawler(fhicl::ParameterSet const& pset) :
    fCCHFAlg(pset.get< fhicl::ParameterSet >("CCHitFinderAlg" )),
    fCCAlg(  pset.get< fhicl::ParameterSet >("ClusterCrawlerAlg"))
//    fCCHRAlg(pset.get< fhicl::ParameterSet >("CCHitRefinerAlg" ))
  {  
    this->reconfigure(pset);
    produces< std::vector<recob::Hit> >();
    produces< std::vector<recob::Cluster> >();  
    produces< art::Assns<recob::Cluster, recob::Hit> >();
    produces< std::vector<recob::EndPoint2D> >();
    produces< std::vector<recob::Vertex> >();
  }

  ClusterCrawler::~ClusterCrawler()
  {
  }

  void ClusterCrawler::reconfigure(fhicl::ParameterSet const & pset)
  {
    fCCAlg.reconfigure(pset.get< fhicl::ParameterSet >("ClusterCrawlerAlg"));
    fCCHFAlg.reconfigure(pset.get< fhicl::ParameterSet >("CCHitFinderAlg"));
//    fCCHRAlg.reconfigure(pset.get< fhicl::ParameterSet >("CCHitRefinerAlg"));
  }
  
  void ClusterCrawler::beginJob(){
  }
  
  // used for sorting clusters by length
  typedef std::pair<unsigned int, unsigned int> mypair;
  bool SortByLen(const mypair& L, const mypair& R) {return (L.first > R.first);}
  
  void ClusterCrawler::produce(art::Event & evt)
  {

    // find hits in all planes
    fCCHFAlg.RunCCHitFinder(evt);

    // look for clusters in all planes
    fCCAlg.RunCrawler(fCCHFAlg.allhits);
  
/*
    // refine the hits, set cluster Begin/End
    fCCHRAlg.RunCCHitRefiner(fCCHFAlg.allhits, fCCHFAlg.hitcuts, 
                               fCCAlg.tcl, fCCAlg.vtx, fCCAlg);
*/

    art::ServiceHandle<geo::Geometry> geo;
    
    std::unique_ptr<art::Assns<recob::Cluster, recob::Hit> > hc_assn(new art::Assns<recob::Cluster, recob::Hit>);
    std::vector<recob::Hit> shcol;
    std::vector<recob::Cluster> sccol;

    // put clusters and hits into std::vectors
    unsigned short nclus = 0;
    unsigned short hitcnt = 0;
    double qtot;
    for(unsigned short icl = 0; icl < fCCAlg.tcl.size(); ++icl) {
      ClusterCrawlerAlg::ClusterStore clstr = fCCAlg.tcl[icl];
      if(clstr.ID < 0) continue;
      // start cluster numbering at 1
      ++nclus;
      qtot = 0;
      // make the hits on this cluster
      unsigned short firsthit = hitcnt;
      for(unsigned short itt = 0; itt < clstr.tclhits.size(); ++itt) {
        unsigned short iht = clstr.tclhits[itt];
        if(iht > fCCHFAlg.allhits.size() - 1) {
          mf::LogError("ClusterCrawler")<<"Bad hit index "<<iht;
          return;
        }
        CCHitFinderAlg::CCHit& theHit = fCCHFAlg.allhits[iht];
        // consistency check
        if(theHit.InClus != clstr.ID) {
          mf::LogError("ClusterCrawler")<<"Using bad hit in cluster "<<clstr.ID
            <<" hit ID "<<iht<<" InClus "<<theHit.InClus;
          return;
        }
        art::Ptr<recob::Wire> theWire = theHit.Wire;
        uint32_t channel = theWire->Channel();
        // get the Wire ID from the channel
        std::vector<geo::WireID> wids = geo->ChannelToWire(channel);
        if(!wids[0].isValid) {
          mf::LogError("ClusterCrawler")<<"Invalid Wire ID "<<theWire<<" "<<channel;
          return;
        }
// NOTE: hit RMS in SigmaStartTime
        recob::Hit hit(theHit.Wire,  wids[0],
              (double) theHit.LoTime, theHit.RMS,
              (double) theHit.HiTime, 0.,
              (double) theHit.Time, theHit.TimeErr,
              (double) theHit.Charge, theHit.ChargeErr,
              (double) theHit.Amplitude, theHit.AmplitudeErr,
              (int)    theHit.numHits, 
              (double) theHit.ChiDOF);
        shcol.push_back(hit);
        ++hitcnt;
        qtot += theHit.Charge;
      } // itt
      // get the view from a hit on the cluster
      CCHitFinderAlg::CCHit& theHit = fCCHFAlg.allhits[clstr.tclhits[0]];
      art::Ptr<recob::Wire> theWire = theHit.Wire;
      uint32_t channel = theWire->Channel();
      
      // Stuff 2D vertex info into unused Cluster variables to help
      // associate the Begin cluster - vertex 2D and End cluster - vertex 2D
      double clBeginEP2Index = -1;
      double clBeginVtxIndex = -1;
      double clEndEP2Index = -1;
      double clEndVtxIndex = -1;
      short iv2 = 0;
      geo::PlaneID planeID = ClusterCrawlerAlg::DecodeCTP(clstr.CTP);
      unsigned short cPlane = planeID.Plane;
      if(clstr.BeginVtx >= 0) {
        iv2 = clstr.BeginVtx;
        clBeginEP2Index = iv2;
        // See if this 2D vertex is associated with a 3D vertex
        for(unsigned short iv3 = 0; iv3 < fCCAlg.vtx3.size(); ++iv3) {
          // ignore incomplete vertices
          if(fCCAlg.vtx3[iv3].Ptr2D[0] < 0) continue;
          if(fCCAlg.vtx3[iv3].Ptr2D[1] < 0) continue;
          if(fCCAlg.vtx3[iv3].Ptr2D[2] < 0) continue;
          if(fCCAlg.vtx3[iv3].Ptr2D[cPlane] == iv2) {
            clBeginVtxIndex = iv3;
            break;
          }
        } // iv3
      } // clstr.BeginVtx >= 0
      if(clstr.EndVtx >= 0) {
        iv2 = clstr.EndVtx;
        clEndEP2Index = iv2;
        for(unsigned short iv3 = 0; iv3 < fCCAlg.vtx3.size(); ++iv3) {
          // ignore incomplete vertices
          if(fCCAlg.vtx3[iv3].Ptr2D[0] < 0) continue;
          if(fCCAlg.vtx3[iv3].Ptr2D[1] < 0) continue;
          if(fCCAlg.vtx3[iv3].Ptr2D[2] < 0) continue;
          if(fCCAlg.vtx3[iv3].Ptr2D[cPlane] == iv2) {
            clEndVtxIndex = iv3;
            break;
          }
        } // iv3
      } // clstr.EndVtx >= 0
      
      // create the recob::Cluster directly in the vector
      sccol.emplace_back((double)clstr.BeginWir, clBeginEP2Index,
                         (double)clstr.BeginTim, clBeginVtxIndex,
                         (double)clstr.EndWir, clEndEP2Index,
                         (double)clstr.EndTim, clEndVtxIndex,
                         (double)clstr.EndSlp, (double)clstr.BeginSlp,
                         (double)clstr.BeginChg, (double)clstr.EndChg,
                         qtot,
                         geo->View(channel),
                         nclus,
                         ClusterCrawlerAlg::DecodeCTP(clstr.CTP)
                         );
      
      // associate the hits to this cluster
      util::CreateAssn(*this, evt, sccol, shcol, *hc_assn, firsthit, hitcnt);
    } // cluster iterator
    
    // make hits that are not associated with any cluster
    hitcnt = 0;
    for(unsigned short iht = 0; iht < fCCHFAlg.allhits.size(); ++iht) {
      CCHitFinderAlg::CCHit& theHit = fCCHFAlg.allhits[iht];
      // obsolete or used hit?
      if(theHit.InClus != 0) continue;
      ++hitcnt;
      art::Ptr<recob::Wire> theWire = theHit.Wire;
      uint32_t channel = theWire->Channel();
      // get the Wire ID from the channel
      std::vector<geo::WireID> wids = geo->ChannelToWire(channel);
      if(!wids[0].isValid) {
        mf::LogError("ClusterCrawler")<<"Invalid Wire ID "<<theWire<<" "<<channel;
      }
// NOTE: hit RMS in SigmaStartTime
      recob::Hit hit(theHit.Wire,  wids[0],
            (double) theHit.LoTime, theHit.RMS,
            (double) theHit.HiTime, 0.,
            (double) theHit.Time , theHit.TimeErr,
            (double) theHit.Charge , theHit.ChargeErr,
            (double) theHit.Amplitude , theHit.AmplitudeErr,
            (int)    theHit.numHits, 
            (double) theHit.ChiDOF);
      shcol.push_back(hit);
    }
    
    // convert to unique_ptrs
    std::unique_ptr<std::vector<recob::Hit> > hcol(new std::vector<recob::Hit>);
    for(unsigned short iht = 0; iht < shcol.size(); ++iht) {
      hcol->push_back(shcol[iht]);
    }
    std::unique_ptr<std::vector<recob::Cluster> > ccol(new std::vector<recob::Cluster>(std::move(sccol)));

    // 2D and 3D vertex collections
    std::unique_ptr<std::vector<recob::EndPoint2D> > v2col(new std::vector<recob::EndPoint2D>);
    std::unique_ptr<std::vector<recob::Vertex> > v3col(new std::vector<recob::Vertex>);

    // make the 2D vertex collection
    for(unsigned short iv = 0; iv < fCCAlg.vtx.size(); iv++) {
      ClusterCrawlerAlg::VtxStore aVtx = fCCAlg.vtx[iv];
      if(aVtx.Wght <= 0) continue;
      geo::PlaneID planeID = ClusterCrawlerAlg::DecodeCTP(aVtx.CTP);
      unsigned int wire = (0.5 + aVtx.Wire);
      if(wire > geo->Nwires(planeID.Plane) - 1) {
        mf::LogError("ClusterCrawler")<<"Bad vtx wire "<<wire<<" plane "
          <<planeID.Plane<<" vtx # "<<iv;
        continue;
      }
      uint32_t channel = geo->PlaneWireToChannel(planeID.Plane, wire, planeID.TPC, planeID.Cryostat);
      // get the Wire ID from the channel
      std::vector<geo::WireID> wids = geo->ChannelToWire(channel);
      if(!wids[0].isValid) {
        mf::LogError("ClusterCrawler")<<"Invalid Wire ID "<<planeID.Plane<<" "<<wire<<" "<<planeID.TPC<<" "<<planeID.Cryostat;
        continue;
      }
      // stuff the (float) wire coordinate into the (not-useful) endpoint charge
      recob::EndPoint2D myvtx((double)aVtx.Time, wids[0], (double)aVtx.Wght,
        (int)iv, geo->View(channel), aVtx.Wire);
      v2col->push_back(myvtx);
    } // iv
    
    // make the 3D vertex collection
    double xyz[3] = {0, 0, 0};
    for(unsigned short iv = 0; iv < fCCAlg.vtx3.size(); iv++) {
      ClusterCrawlerAlg::Vtx3Store vtx3 = fCCAlg.vtx3[iv];
      // ignore incomplete vertices
      if(vtx3.Ptr2D[0] < 0) continue;
      if(vtx3.Ptr2D[1] < 0) continue;
      if(vtx3.Ptr2D[2] < 0) continue;
      xyz[0] = vtx3.X;
      xyz[1] = vtx3.Y;
      xyz[2] = vtx3.Z;
      recob::Vertex myvtx(xyz, iv);
      v3col->push_back(myvtx);
    }

    // clean up
    fCCHFAlg.allhits.clear();
    fCCAlg.tcl.clear();
    fCCAlg.vtx.clear();
    fCCAlg.vtx3.clear();

    evt.put(std::move(hcol));
    evt.put(std::move(ccol));
    evt.put(std::move(hc_assn));
    evt.put(std::move(v2col));
    evt.put(std::move(v3col));

  } // produce
} // namespace


namespace cluster{

  DEFINE_ART_MODULE(ClusterCrawler)
  
} 

