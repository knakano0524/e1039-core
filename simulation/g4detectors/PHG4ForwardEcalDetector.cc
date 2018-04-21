#include "PHG4ForwardEcalDetector.h"
#include "PHG4CylinderGeomContainer.h"
#include "PHG4CylinderGeomv3.h"

#include <g4main/PHG4Utils.h>

#include <phool/PHCompositeNode.h>
#include <phool/PHIODataNode.h>
#include <phool/getClass.h>

#include <Geant4/G4AssemblyVolume.hh>
#include <Geant4/G4IntersectionSolid.hh>
#include <Geant4/G4SubtractionSolid.hh>
#include <Geant4/G4Material.hh>
#include <Geant4/G4Box.hh>
#include <Geant4/G4ExtrudedSolid.hh>
#include <Geant4/G4LogicalVolume.hh>
#include <Geant4/G4PVPlacement.hh>
#include <Geant4/G4TwoVector.hh>
#include <Geant4/G4Trap.hh>
#include <Geant4/G4GenericTrap.hh>
#include <Geant4/G4Cons.hh>
#include <Geant4/G4Box.hh>
#include <Geant4/G4Trd.hh>
#include <Geant4/G4NistManager.hh>

#include <Geant4/G4VisAttributes.hh>
#include <Geant4/G4Colour.hh>

#include <cmath>
#include <sstream>

#include <iostream>
#include <fstream>
#include <cstdlib>


using namespace std;


//_______________________________________________________________________
PHG4ForwardEcalDetector::PHG4ForwardEcalDetector( PHCompositeNode *Node, const std::string &dnam ):
  PHG4Detector(Node, dnam),
  _tower0_dx(30*mm),
  _tower0_dy(30*mm),
  _tower0_dz(170.0*mm),
  _tower1_dx(30*mm),
  _tower1_dy(30*mm),
  _tower1_dz(170.0*mm),
  _tower2_dx(30*mm),
  _tower2_dy(30*mm),
  _tower2_dz(170.0*mm),
  _place_in_x(0.0*mm),
  _place_in_y(0.0*mm),
  _place_in_z(3150.0*mm),
  _rot_in_x(0.0),
  _rot_in_y(0.0),
  _rot_in_z(0.0),
  _rMin1(110*mm),
  _rMax1(2250*mm),
  _rMin2(120*mm),
  _rMax2(2460*mm),
  _dZ(170*mm),
  _sPhi(0),
  _dPhi(2*M_PI),
  _active(1),
  _absorberactive(0),
  _layer(0),
  _blackhole(0),
  _towerlogicnameprefix("hEcalTower"),
  _superdetector("NONE"),
  _mapping_tower_file("")
{

}


//_______________________________________________________________________
PHG4ForwardEcalDetector::~PHG4ForwardEcalDetector()
{}


//_______________________________________________________________________
int
PHG4ForwardEcalDetector::IsInForwardEcal(G4VPhysicalVolume * volume) const
{
  if (volume->GetName().find(_towerlogicnameprefix) != string::npos)
    {
      if (volume->GetName().find("scintillator") != string::npos)
	{
	  if(_active)
	    return 1;
	  else
	    return 0;
	}
      /* only record energy in actual absorber- drop energy lost in air gaps inside ecal envelope */
      else if (volume->GetName().find("absorber") != string::npos)
	{
	  if(_absorberactive)
	    return -1;
	  else
	    return 0; 
	}
      else if (volume->GetName().find("envelope") != string::npos)
	{
	  return 0;
	}
    }

  return 0;
}


//_______________________________________________________________________
void
PHG4ForwardEcalDetector::Construct( G4LogicalVolume* logicWorld )
{
  if ( verbosity > 0 )
    {
      cout << "PHG4ForwardEcalDetector: Begin Construction" << endl;
    }


  if ( _mapping_tower_file.empty() )
    {
      cout << "ERROR in PHG4ForwardEcalDetector: No tower mapping file specified. Abort detector construction." << endl;
      cout << "Please run SetTowerMappingFile( std::string filename ) first." << endl;
      exit(1);
    }

  /* Read parameters for detector construction and mappign from file */
  ParseParametersFromTable();

  /* Create the cone envelope = 'world volume' for the crystal calorimeter */
  G4Material* Air = G4Material::GetMaterial("G4_AIR");

  G4VSolid* ecal_envelope_solid = new G4Cons("hEcal_envelope_solid",
					    _rMin1, _rMax1,
					    _rMin2, _rMax2,
					    _dZ/2.,
					    _sPhi, _dPhi );

  G4LogicalVolume* ecal_envelope_log =  new G4LogicalVolume(ecal_envelope_solid, Air, G4String("hEcal_envelope"), 0, 0, 0);

  /* Define visualization attributes for envelope cone */
  G4VisAttributes* ecalVisAtt = new G4VisAttributes();
  ecalVisAtt->SetVisibility(false);
  ecalVisAtt->SetForceSolid(false);
  ecalVisAtt->SetColour(G4Colour::Magenta());
  ecal_envelope_log->SetVisAttributes(ecalVisAtt);

  /* Define rotation attributes for envelope cone */
  G4RotationMatrix ecal_rotm;
  ecal_rotm.rotateX(_rot_in_x);
  ecal_rotm.rotateY(_rot_in_y);
  ecal_rotm.rotateZ(_rot_in_z);

  /* Place envelope cone in simulation */
  ostringstream name_envelope;
  name_envelope.str("");
  name_envelope << _towerlogicnameprefix << "_envelope" << endl;

  new G4PVPlacement( G4Transform3D(ecal_rotm, G4ThreeVector(_place_in_x, _place_in_y, _place_in_z) ),
		     ecal_envelope_log, name_envelope.str().c_str(), logicWorld, 0, false, overlapcheck);

  /* Construct single calorimeter tower */
  G4LogicalVolume* singletower0 = ConstructTower(0);
  G4LogicalVolume* singletower1 = ConstructTower(1);
  G4LogicalVolume* singletower2 = ConstructTower(2);

  /* Place calorimeter towers within envelope */
  PlaceTower( ecal_envelope_log , singletower0, singletower1, singletower2 );

  return;
}


//_______________________________________________________________________
G4LogicalVolume*
PHG4ForwardEcalDetector::ConstructTower( int type )
{
  if ( verbosity > 0 )
    {
      cout << "PHG4ForwardEcalDetector: Build logical volume for single tower, type = " << type << endl;
    }

  // This method allows construction of Type 0,1 tower (PbGl or PbW04). 
  // Call a separate routine to generate Type 2 towers (PbSc)

  if(type==2) return ConstructTowerType2(); 

  /* create logical volume for single tower */
  G4Material* material_air = G4Material::GetMaterial( "G4_AIR" );

  G4double _tower_dx = 0.0;
  G4double _tower_dy = 0.0;
  G4double _tower_dz = 0.0;

  G4NistManager* manager = G4NistManager::Instance();
  G4Material* material_scintillator;

  if(type==0){
    _tower_dx = _tower0_dx; 
    _tower_dy = _tower0_dy; 
    _tower_dz = _tower0_dz;
    material_scintillator = manager->FindOrBuildMaterial("G4_LEAD_OXIDE"); 
  }
  else if(type==1){
    _tower_dx = _tower1_dx; 
    _tower_dy = _tower1_dy; 
    _tower_dz = _tower1_dz; 
    material_scintillator = manager->FindOrBuildMaterial("G4_PbWO4"); 
  }
  else{
    cout << "PHG4ForwardEcalDetector::ConstructTower invalid type = " << type << endl;
    material_scintillator = NULL; 
  }

  ostringstream single_tower_solid_name;
  single_tower_solid_name.str("");
  single_tower_solid_name << _towerlogicnameprefix << "_single_scintillator_type" << type << endl;

  G4VSolid* single_tower_solid = new G4Box( G4String(single_tower_solid_name.str().c_str()),
                                       _tower_dx / 2.0,
                                       _tower_dy / 2.0,
                                       _tower_dz / 2.0 );

  ostringstream single_tower_logic_name;
  single_tower_logic_name.str("");
  single_tower_logic_name << "single_tower_logic_type" << type << endl;

  G4LogicalVolume *single_tower_logic = new G4LogicalVolume( single_tower_solid,
							     material_air,
							     single_tower_logic_name.str().c_str(),
							     0, 0, 0);

  ostringstream single_scintillator_name;
  single_scintillator_name.str("");
  single_scintillator_name << "single_scintillator_type" << type << endl;

  G4VSolid* solid_scintillator = new G4Box( G4String(single_scintillator_name.str().c_str()),
				   _tower_dx / 2.0,
				   _tower_dy / 2.0,
				   _tower_dz / 2.0 );

  ostringstream hEcal_scintillator_plate_logic_name;
  hEcal_scintillator_plate_logic_name.str("");
  hEcal_scintillator_plate_logic_name << "hEcal_scintillator_plate_logic_type" << type << endl;

  G4LogicalVolume *logic_scint = new G4LogicalVolume( solid_scintillator,
						      material_scintillator,
						      hEcal_scintillator_plate_logic_name.str().c_str(),
						      0, 0, 0);

  G4VisAttributes *visattscint = new G4VisAttributes();
  visattscint->SetVisibility(true);
  visattscint->SetForceSolid(true);
  visattscint->SetColour(G4Colour::Cyan());
  logic_scint->SetVisAttributes(visattscint);

  /* place physical volumes for scintillator */

  ostringstream name_scintillator;
  name_scintillator.str("");
  name_scintillator << _towerlogicnameprefix << "_single_plate_scintillator" << endl;

  new G4PVPlacement( 0, G4ThreeVector(0.0, 0.0, 0.0),
		     logic_scint,
		     name_scintillator.str().c_str(),
		     single_tower_logic,
		     0, 0, overlapcheck);

  G4VisAttributes *visattchk = new G4VisAttributes();
  visattchk->SetVisibility(true);
  visattchk->SetForceSolid(true);
  visattchk->SetColour(G4Colour::Cyan());
  single_tower_logic->SetVisAttributes(visattchk);

  if ( verbosity > 0 )
    {
      cout << "PHG4ForwardEcalDetector: Building logical volume for single tower done, type = " << type << endl;
    }

  return single_tower_logic;
}

G4LogicalVolume*
PHG4ForwardEcalDetector::ConstructTowerType2()
{
  if ( verbosity > 0 )
    {
      cout << "PHG4ForwardEcalDetector: Build logical volume for single tower type 2..." << endl;
    }

  /* create logical volume for single tower */
  G4Material* material_air = G4Material::GetMaterial( "G4_AIR" );

  G4VSolid* single_tower_solid = new G4Box( G4String("single_tower_solid2"),
                                       _tower2_dx / 2.0,
                                       _tower2_dy / 2.0,
                                       _tower2_dz / 2.0 );

  G4LogicalVolume *single_tower_logic = new G4LogicalVolume( single_tower_solid,
						      material_air,
						      "single_tower_logic2",
						      0, 0, 0);

  /* create geometry volumes for scintillator and absorber plates to place inside single_tower */
  // PHENIX EMCal JGL 3/27/2016
  G4int nlayers = 66;
  G4double thickness_layer = _tower2_dz/(float)nlayers;
  G4double thickness_absorber = thickness_layer*0.23; // 27% absorber by length
  G4double thickness_scintillator = thickness_layer*0.73; // 73% scintillator by length

  G4VSolid* solid_absorber = new G4Box( G4String("single_plate_absorber_solid2"),
				   _tower2_dx / 2.0,
				   _tower2_dy / 2.0,
				   thickness_absorber / 2.0 );

  G4VSolid* solid_scintillator = new G4Box( G4String("single_plate_scintillator2"),
				   _tower2_dx / 2.0,
				   _tower2_dy / 2.0,
				   thickness_scintillator / 2.0 );

  /* create logical volumes for scintillator and absorber plates to place inside single_tower */
  G4Material* material_scintillator = G4Material::GetMaterial( "G4_POLYSTYRENE" );
  G4Material* material_absorber = G4Material::GetMaterial( "G4_Pb" );

  G4LogicalVolume *logic_absorber = new G4LogicalVolume( solid_absorber,
						    material_absorber,
						    "single_plate_absorber_logic2",
						    0, 0, 0);

  G4LogicalVolume *logic_scint = new G4LogicalVolume( solid_scintillator,
						    material_scintillator,
						    "hEcal_scintillator_plate_logic2",
						    0, 0, 0);

  G4VisAttributes *visattscint = new G4VisAttributes();
  visattscint->SetVisibility(true);
  visattscint->SetForceSolid(true);
  visattscint->SetColour(G4Colour::Cyan());
  logic_absorber->SetVisAttributes(visattscint);
  logic_scint->SetVisAttributes(visattscint);

  /* place physical volumes for absorber and scintillator plates */
  G4double xpos_i = 0;
  G4double ypos_i = 0;
  G4double zpos_i = ( -1 * _tower2_dz / 2.0 ) + thickness_absorber / 2.0;

  ostringstream name_absorber;
  name_absorber.str("");
  name_absorber << _towerlogicnameprefix << "_single_plate_absorber2" << endl;

  ostringstream name_scintillator;
  name_scintillator.str("");
  name_scintillator << _towerlogicnameprefix << "_single_plate_scintillator2" << endl;

  for (int i = 1; i <= nlayers; i++)
    {
      new G4PVPlacement( 0, G4ThreeVector(xpos_i , ypos_i, zpos_i),
			 logic_absorber,
			 name_absorber.str().c_str(),
			 single_tower_logic,
			 0, 0, overlapcheck);

      zpos_i += ( thickness_absorber/2. + thickness_scintillator/2. );

      new G4PVPlacement( 0, G4ThreeVector(xpos_i , ypos_i, zpos_i),
			 logic_scint,
			 name_scintillator.str().c_str(),
			 single_tower_logic,
			 0, 0, overlapcheck);

      zpos_i += ( thickness_absorber/2. + thickness_scintillator/2. );
    }


  G4VisAttributes *visattchk = new G4VisAttributes();
  visattchk->SetVisibility(true);
  visattchk->SetForceSolid(true);
  visattchk->SetColour(G4Colour::Cyan());
  single_tower_logic->SetVisAttributes(visattchk);

  if ( verbosity > 0 )
    {
      cout << "PHG4EICForwardEcalDetector: Building logical volume for single tower done." << endl;
    }

  return single_tower_logic;
}

int
PHG4ForwardEcalDetector::PlaceTower(G4LogicalVolume* ecalenvelope, G4LogicalVolume* singletower0, 
				    G4LogicalVolume* singletower1, G4LogicalVolume* singletower2)
{
  /* Loop over all tower positions in vector and place tower */
  typedef std::map< std::string, towerposition>::iterator it_type;

  for(it_type iterator = _map_tower.begin(); iterator != _map_tower.end(); iterator++) {

      if ( verbosity > 0 )
	{
	  cout << "PHG4ForwardEcalDetector: Place tower " << iterator->first
	       << " at x = " << iterator->second.x << " , y = " << iterator->second.y << " , z = " << iterator->second.z << endl;
	}

      G4LogicalVolume* singletower = NULL;
      if(iterator->second.type==0)
	singletower = singletower0; 
      else if(iterator->second.type==1)
	singletower = singletower1; 
      else if(iterator->second.type==2)
	singletower = singletower2; 
      else
	cout << "PHG4ForwardEcalDetector::PlaceTower invalid type =  " << iterator->second.type << endl; 

      new G4PVPlacement( 0, G4ThreeVector(iterator->second.x, iterator->second.y, iterator->second.z),
			 singletower,
			 iterator->first.c_str(),
			 ecalenvelope,
			 0, 0, overlapcheck);

  }

  return 0;
}

int
PHG4ForwardEcalDetector::ParseParametersFromTable()
{

  /* Open the datafile, if it won't open return an error */
  ifstream istream_mapping;
  if (!istream_mapping.is_open())
    {
      istream_mapping.open( _mapping_tower_file.c_str() );
      if(!istream_mapping)
	{
	  cerr << "ERROR in PHG4ForwardEcalDetector: Failed to open mapping file " << _mapping_tower_file << endl;
	  exit(1);
	}
    }

  /* loop over lines in file */
  string line_mapping;
  while ( getline( istream_mapping, line_mapping ) )
    {
      /* Skip lines starting with / including a '#' */
      if ( line_mapping.find("#") != string::npos )
	{
	  if ( verbosity > 0 )
	    {
	      cout << "PHG4ForwardEcalDetector: SKIPPING line in mapping file: " << line_mapping << endl;
	    }
	  continue;
	}

      istringstream iss(line_mapping);

      /* If line starts with keyword Tower, add to tower positions */
      if ( line_mapping.find("Tower ") != string::npos )
	{
	  unsigned idx_j, idx_k, idx_l;
	  G4double pos_x, pos_y, pos_z;
	  G4double size_x, size_y, size_z;
	  G4double rot_x, rot_y, rot_z;
	  G4double type;
	  string dummys;

	  /* read string- break if error */
	  if ( !( iss >> dummys >> type >> idx_j >> idx_k >> idx_l >> pos_x >> pos_y >> pos_z >> size_x >> size_y >> size_z >> rot_x >> rot_y >> rot_z ) )
	    {
	      cerr << "ERROR in PHG4ForwardEcalDetector: Failed to read line in mapping file " << _mapping_tower_file << endl;
	      exit(1);
	    }

	  /* Construct unique name for tower */
	  /* Mapping file uses cm, this class uses mm for length */
	  ostringstream towername;
	  towername.str("");
	  towername << _towerlogicnameprefix << "_t_" << type << "_j_" << idx_j << "_k_" << idx_k;

	  /* Add Geant4 units */
	  pos_x = pos_x * cm;
	  pos_y = pos_y * cm;
	  pos_z = pos_z * cm;

	  /* insert tower into tower map */
	  towerposition tower_new;
	  tower_new.x = pos_x;
	  tower_new.y = pos_y;
	  tower_new.z = pos_z;
	  tower_new.type = type; 
	  _map_tower.insert( make_pair( towername.str() , tower_new ) );

	}
      else
	{
	  /* If this line is not a comment and not a tower, save parameter as string / value. */
	  string parname;
	  G4double parval;

	  /* read string- break if error */
	  if ( !( iss >> parname >> parval ) )
	    {
	      cerr << "ERROR in PHG4ForwardEcalDetector: Failed to read line in mapping file " << _mapping_tower_file << endl;
	      exit(1);
	    }

	  _map_global_parameter.insert( make_pair( parname , parval ) );

	}
    }

  /* Update member variables for global parameters based on parsed parameter file */
  std::map<string,G4double>::iterator parit;

  parit = _map_global_parameter.find("Gtower0_dx");
  if (parit != _map_global_parameter.end())
    _tower0_dx = parit->second * cm;

  parit = _map_global_parameter.find("Gtower0_dy");
  if (parit != _map_global_parameter.end())
    _tower0_dy = parit->second * cm;

  parit = _map_global_parameter.find("Gtower0_dz");
  if (parit != _map_global_parameter.end())
    _tower0_dz = parit->second * cm;

  parit = _map_global_parameter.find("Gtower1_dx");
  if (parit != _map_global_parameter.end())
    _tower1_dx = parit->second * cm;

  parit = _map_global_parameter.find("Gtower1_dy");
  if (parit != _map_global_parameter.end())
    _tower1_dy = parit->second * cm;

  parit = _map_global_parameter.find("Gtower1_dz");
  if (parit != _map_global_parameter.end())
    _tower1_dz = parit->second * cm;

  parit = _map_global_parameter.find("Gtower2_dx");
  if (parit != _map_global_parameter.end())
    _tower2_dx = parit->second * cm;

  parit = _map_global_parameter.find("Gtower2_dy");
  if (parit != _map_global_parameter.end())
    _tower2_dy = parit->second * cm;

  parit = _map_global_parameter.find("Gtower2_dz");
  if (parit != _map_global_parameter.end())
    _tower2_dz = parit->second * cm;

  parit = _map_global_parameter.find("Gr1_inner");
  if (parit != _map_global_parameter.end())
    _rMin1 = parit->second * cm;

  parit = _map_global_parameter.find("Gr1_outer");
  if (parit != _map_global_parameter.end())
    _rMax1 = parit->second * cm;

  parit = _map_global_parameter.find("Gr2_inner");
  if (parit != _map_global_parameter.end())
    _rMin2 = parit->second * cm;

  parit = _map_global_parameter.find("Gr2_outer");
  if (parit != _map_global_parameter.end())
    _rMax2 = parit->second * cm;

  parit = _map_global_parameter.find("Gdz");
  if (parit != _map_global_parameter.end())
    _dZ = parit->second * cm;

  parit = _map_global_parameter.find("Gx0");
  if (parit != _map_global_parameter.end())
    _place_in_x = parit->second * cm;

  parit = _map_global_parameter.find("Gy0");
  if (parit != _map_global_parameter.end())
    _place_in_y = parit->second * cm;

  parit = _map_global_parameter.find("Gz0");
  if (parit != _map_global_parameter.end())
    _place_in_z = parit->second * cm;

  parit = _map_global_parameter.find("Grot_x");
  if (parit != _map_global_parameter.end())
    _rot_in_x = parit->second;

  parit = _map_global_parameter.find("Grot_y");
  if (parit != _map_global_parameter.end())
    _rot_in_y = parit->second;

  parit = _map_global_parameter.find("Grot_z");
  if (parit != _map_global_parameter.end())
    _rot_in_z = parit->second;

  return 0;
}