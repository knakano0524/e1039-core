#ifndef PHG4CollimatorDetector_h
#define PHG4CollimatorDetector_h

#include <g4main/PHG4Detector.h>

class G4LogicalVolume;
class PHParameters;
class G4VPhysicalVolume;

class PHG4CollimatorDetector: public PHG4Detector
{

  public:

  //! constructor
  PHG4CollimatorDetector( PHCompositeNode *Node, PHParameters *parameters, const std::string &dnam="BLOCK", const int lyr = 0 );

  //! destructor
  virtual ~PHG4CollimatorDetector( void )
  {}

  //! construct
  virtual void Construct( G4LogicalVolume* world );

  //!@name volume accessors
  //@{
  bool IsInBlock(G4VPhysicalVolume*) const;
  //@}

  void SuperDetector(const std::string &name) {superdetector = name;}
  const std::string SuperDetector() const {return superdetector;}
  int get_Layer() const {return layer;}

  private:

  PHParameters *params;
 
  G4VPhysicalVolume* block_physi;


  int layer;
  std::string superdetector;
  
};

#endif
