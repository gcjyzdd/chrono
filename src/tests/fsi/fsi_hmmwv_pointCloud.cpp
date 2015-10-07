
///////////////////////////////////////////////////////////////////////////////
//	main.cpp
//	Reads the initializes the particles, either from file or inside the code
//
//	Related Files: collideSphereSphere.cu, collideSphereSphere.cuh
//	Input File:		initializer.txt (optional: if initialize from file)
//					This file contains the sph particles specifications. The description
//					reads the number of particles first. The each line provides the
//					properties of one SPH particl:
//					position(x,y,z), radius, velocity(x,y,z), mass, \rho, pressure, mu,
// particle_type(rigid
// or fluid)
//
//	Created by Arman Pazouki
///////////////////////////////////////////////////////////////////////////////

// note: this is the original fsi_hmmwv model. uses RK2, an specific coupling, and density re_initializaiton.

// General Includes
#include <iostream>
#include <fstream>
#include <string>
#include <limits.h>
#include <vector>
#include <ctime>
#include <assert.h>
#include <stdlib.h>  // system

// SPH includes
#include "MyStructs.cuh"  //just for SimParams
#include "collideSphereSphere.cuh"
#include "printToFile.cuh"
#include "custom_cutil_math.h"
#include "SPHCudaUtils.h"
#include "checkPointReduced.h"

// Chrono Parallel Includes
#include "chrono_parallel/physics/ChSystemParallel.h"
#include "chrono_parallel/lcp/ChLcpSystemDescriptorParallel.h"

// Chrono Vehicle Include
#include "VehicleExtraProperties.h"
#include "chrono_vehicle/ChVehicleModelData.h"

//#include "chrono_utils/ChUtilsVehicle.h"
#include "utils/ChUtilsGeometry.h"
#include "utils/ChUtilsCreators.h"
#include "utils/ChUtilsGenerators.h"
#include "utils/ChUtilsInputOutput.h"

// Chrono general utils
#include "core/ChFileutils.h"
#include <core/ChTransform.h>  //transform acc from GF to LF for post process

// FSI Interface Includes
#include "fsi_hmmwv_params.h"  //SetupParamsH()
//#include "BallDropParams.h"
#include "SphInterface.h"
#include "InitializeSphMarkers.h"

// Chrono namespaces
using namespace chrono;
using namespace chrono::collision;

using std::cout;
using std::endl;

// Define General variables
SimParams paramsH;

#define haveFluid true
#define useWallBce true

#if haveFluid
#else
#undef useWallBce
#define useWallBce false
#endif
// =============================================================================
// Define Graphics
#define irrlichtVisualization false

#if irrlichtVisualization

// Irrlicht Include
#include "unit_IRRLICHT/ChIrrApp.h"

// Use the main namespaces of Irrlicht
using namespace irr;
using namespace core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;

std::shared_ptr<ChIrrApp> application;
#endif

//#ifdef CHRONO_OPENGL
//#undef CHRONO_OPENGL
//#endif

#ifdef CHRONO_OPENGL
#include "chrono_opengl/ChOpenGLWindow.h"
opengl::ChOpenGLWindow& gl_window = opengl::ChOpenGLWindow::getInstance();
#endif

// =============================================================================
void SetArgumentsForMbdFromInput(int argc,
                                 char* argv[],
                                 int& threads,
                                 int& max_iteration_sliding,
                                 int& max_iteration_bilateral) {
  if (argc > 1) {
    const char* text = argv[1];
    threads = atoi(text);
  }
  if (argc > 2) {
    const char* text = argv[2];
    max_iteration_sliding = atoi(text);
  }
  if (argc > 3) {
    const char* text = argv[3];
    max_iteration_bilateral = atoi(text);
  }
}
// =============================================================================
void SetArgumentsForMbdFromInput(int argc,
                                 char* argv[],
                                 int& threads,
                                 int& max_iteration_normal,
                                 int& max_iteration_sliding,
                                 int& max_iteration_spinning,
                                 int& max_iteration_bilateral) {
  if (argc > 1) {
    const char* text = argv[1];
    threads = atoi(text);
  }
  if (argc > 2) {
    const char* text = argv[2];
    max_iteration_normal = atoi(text);
  }
  if (argc > 3) {
    const char* text = argv[3];
    max_iteration_sliding = atoi(text);
  }
  if (argc > 4) {
    const char* text = argv[4];
    max_iteration_spinning = atoi(text);
  }
  if (argc > 5) {
    const char* text = argv[5];
    max_iteration_bilateral = atoi(text);
  }
}
// =============================================================================

void InitializeMbdPhysicalSystem(ChSystemParallelDVI& mphysicalSystem, int argc, char* argv[]) {
  // Desired number of OpenMP threads (will be clamped to maximum available)
  int threads = 4;
  // Perform dynamic tuning of number of threads?
  bool thread_tuning = true;

  //	uint max_iteration = 20;//10000;
  int max_iteration_normal = 0;
  int max_iteration_sliding = 200;
  int max_iteration_spinning = 0;
  int max_iteration_bilateral = 100;

  // ----------------------
  // Set params from input
  // ----------------------

  SetArgumentsForMbdFromInput(argc, argv, threads, max_iteration_sliding, max_iteration_bilateral);

  // ----------------------
  // Set number of threads.
  // ----------------------

  //  omp_get_num_procs();
  int max_threads = omp_get_num_procs();
  if (threads > max_threads)
    threads = max_threads;
  mphysicalSystem.SetParallelThreadNumber(threads);
  omp_set_num_threads(threads);
  cout << "Using " << threads << " threads" << endl;

  mphysicalSystem.GetSettings()->perform_thread_tuning = thread_tuning;
  mphysicalSystem.GetSettings()->min_threads = max(1, threads / 2);
  mphysicalSystem.GetSettings()->max_threads = int(3.0 * threads / 2);

  // ---------------------
  // Print the rest of parameters
  // ---------------------

  simParams << endl << " number of threads: " << threads << endl << " max_iteration_normal: " << max_iteration_normal
            << endl << " max_iteration_sliding: " << max_iteration_sliding << endl
            << " max_iteration_spinning: " << max_iteration_spinning << endl
            << " max_iteration_bilateral: " << max_iteration_bilateral << endl << endl;

  // ---------------------
  // Edit mphysicalSystem settings.
  // ---------------------

  double tolerance = 0.1;  // 1e-3;  // Arman, move it to paramsH
  // double collisionEnvelop = 0.04 * paramsH.HSML;
  mphysicalSystem.Set_G_acc(ChVector<>(paramsH.gravity.x, paramsH.gravity.y, paramsH.gravity.z));

  mphysicalSystem.GetSettings()->solver.solver_mode = SLIDING;                              // NORMAL, SPINNING
  mphysicalSystem.GetSettings()->solver.max_iteration_normal = max_iteration_normal;        // max_iteration / 3
  mphysicalSystem.GetSettings()->solver.max_iteration_sliding = max_iteration_sliding;      // max_iteration / 3
  mphysicalSystem.GetSettings()->solver.max_iteration_spinning = max_iteration_spinning;    // 0
  mphysicalSystem.GetSettings()->solver.max_iteration_bilateral = max_iteration_bilateral;  // max_iteration / 3
  mphysicalSystem.GetSettings()->solver.use_full_inertia_tensor = true;
  mphysicalSystem.GetSettings()->solver.tolerance = tolerance;
  mphysicalSystem.GetSettings()->solver.alpha = 0;  // Arman, find out what is this
  mphysicalSystem.GetSettings()->solver.contact_recovery_speed = contact_recovery_speed;
  mphysicalSystem.ChangeSolverType(APGD);  // Arman check this APGD APGDBLAZE
  //  mphysicalSystem.GetSettings()->collision.narrowphase_algorithm = NARROWPHASE_HYBRID_MPR;

  //    mphysicalSystem.GetSettings()->collision.collision_envelope = collisionEnvelop;   // global collisionEnvelop
  //    does not work. Maybe due to sph-tire size mismatch
  mphysicalSystem.GetSettings()->collision.bins_per_axis = _make_int3(40, 40, 40);  // Arman check
}


// =============================================================================
Real CreateOne3DRigidCylinder(
		thrust::host_vector<Real3> & posRadH,
		thrust::host_vector<Real4> & velMasH,
		thrust::host_vector<Real4> & rhoPresMuH,

		thrust::device_vector<Real3> & posRigidD,
		thrust::device_vector<Real4> & qD,
		thrust::device_vector<Real4> & velMassRigidD,
		thrust::device_vector<Real3> & omegaLRF_D,
	    ChBody* body,
	    Real cyl_rad,
	    Real cyl_h,
		Real rigidMass,
		Real sphMarkerMass,
		int type) {
	int num_BCEMarkers = 0;
	Real3 vel = ConvertChVectorToR3(body->GetPos_dt());
	Real3 omega = ConvertChVectorToR3(body->GetWacc_loc());
	ChVector<> chPa3 = body->GetPos() - body->GetRot().Rotate( ChVector<>(cyl_h / 2, 0, 0) );
	ChVector<> chPb3 = body->GetPos() + body->GetRot().Rotate(ChVector<>(cyl_h / 2, 0, 0));
	Real3 pa3 = ConvertChVectorToR3(chPa3);
	Real3 pb3 = ConvertChVectorToR3(chPb3);
	Real3 n3 = normalize(pb3 - pa3);
	PushBackR3(posRigidD, ConvertChVectorToR3(body->GetPos()));
	PushBackR4(qD, ConvertChQuaternionToR4(body->GetRot()));
	PushBackR4(velMassRigidD, mR4(vel.x, vel.y, vel.z, rigidMass));
	PushBackR3(omegaLRF_D, omega);

	Real4 q4 = ConvertChQuaternionToR4(body->GetRot());


	Real spacing = paramsH.MULT_INITSPACE * paramsH.HSML;
	for (Real s = 0; s <= cyl_h; s += spacing) {
		Real3 centerPoint = pa3 + s * n3;
		posRadH.push_back(centerPoint);
		velMasH.push_back(mR4(0, 0, 0, sphMarkerMass));
		rhoPresMuH.push_back(
				mR4(paramsH.rho0, paramsH.BASEPRES, paramsH.mu0, type)); //take care of type			 /// type needs to be unique, to differentiate flex from other flex as well as other rigids
		num_BCEMarkers++;
		for (Real r = spacing; r < cyl_rad - paramsH.solidSurfaceAdjust; r += spacing) {
			Real deltaTeta = spacing / r;
			for (Real teta = .1 * deltaTeta; teta < 2 * PI - .1 * deltaTeta;
					teta += deltaTeta) {
				Real3 BCE_Pos_local = mR3(r * cos(teta), r * sin(teta), 0);
				Real3 BCE_Pos_Global = Rotate_By_Quaternion(q4, BCE_Pos_local)
						+ centerPoint;
				posRadH.push_back(BCE_Pos_Global);
				velMasH.push_back(mR4(0, 0, 0, sphMarkerMass));
				rhoPresMuH.push_back(
						mR4(paramsH.rho0, paramsH.BASEPRES, paramsH.mu0, type)); //take care of type
				num_BCEMarkers++;
			}
		}
	}
	return num_BCEMarkers;
}


// =============================================================================
void AddBoxBceToChSystemAndSPH(
    ChBody* body,
    const ChVector<>& size,
    const ChVector<>& pos,
    const ChQuaternion<>& rot,
    bool visualization,

    thrust::host_vector<Real3>& posRadH,  // do not set the size here since you are using push back later
    thrust::host_vector<Real4>& velMasH,
    thrust::host_vector<Real4>& rhoPresMuH,
    thrust::host_vector<uint>& bodyIndex,
    thrust::host_vector< ::int3>& referenceArray,
    NumberOfObjects& numObjects,
    const SimParams& paramsH,
    Real sphMarkerMass) {
  utils::AddBoxGeometry(body, size, pos, rot, visualization);

  if (!initializeFluidFromFile) {
#if haveFluid
#if useWallBce
    assert(referenceArray.size() > 1 &&
           "error: fluid need to be initialized before boundary. Reference array should have two components");

    thrust::host_vector<Real3> posRadBCE;
    thrust::host_vector<Real4> velMasBCE;
    thrust::host_vector<Real4> rhoPresMuBCE;

    CreateBCE_On_Box(posRadBCE, velMasBCE, rhoPresMuBCE, paramsH, sphMarkerMass, size, pos, rot, 12);
    int numBCE = posRadBCE.size();
    int numSaved = posRadH.size();
    for (int i = 0; i < numBCE; i++) {
      posRadH.push_back(posRadBCE[i]);
      velMasH.push_back(velMasBCE[i]);
      rhoPresMuH.push_back(rhoPresMuBCE[i]);
      bodyIndex.push_back(i + numSaved);
    }

    ::int3 ref3 = referenceArray[1];
    ref3.y = ref3.y + numBCE;
    referenceArray[1] = ref3;

    int numAllMarkers = numBCE + numSaved;
    SetNumObjects(numObjects, referenceArray, numAllMarkers);

    posRadBCE.clear();
    velMasBCE.clear();
    rhoPresMuBCE.clear();
#endif
#endif
  }
}


// =============================================================================
void AddCylinderBceToChSystemAndSPH(
		ChSystemParallelDVI& mphysicalSystem,
    Real radius,
    Real height,
    const ChVector<>& pos,
    const ChQuaternion<>& rot,
    bool visualization,

    thrust::host_vector<Real3>& posRadH,  // do not set the size here since you are using push back later
    thrust::host_vector<Real4>& velMasH,
    thrust::host_vector<Real4>& rhoPresMuH,
    thrust::host_vector< ::int3>& referenceArray,

	thrust::device_vector<Real3> & posRigidD,
	thrust::device_vector<Real4> & qD,
	thrust::device_vector<Real4> & velMassRigidD,
	thrust::device_vector<Real3> & omegaLRF_D,
	thrust::host_vector<int>& mapIndex_H,

    NumberOfObjects& numObjects,
    Real sphMarkerMass
) {
	int numMarkers = posRadH.size();
	int numRigidObjects = mphysicalSystem.Get_bodylist()->size();
	int type = 1;
 ChSharedPtr<ChBody> body = ChSharedPtr<ChBody>(new ChBody(new collision::ChCollisionModelParallel));
// body->SetIdentifier(-1);
 body->SetBodyFixed(false);
 body->SetCollide(true);
 body->GetMaterialSurface()->SetFriction(mu_g);
 body->GetCollisionModel()->ClearModel();
  utils::AddCylinderGeometry(body.get_ptr(), radius, height, pos, rot, visualization);
  body->GetCollisionModel()->BuildModel();
  mphysicalSystem.AddBody(body);


  int numBce = CreateOne3DRigidCylinder(posRadH,velMasH, rhoPresMuH,
		  posRigidD, qD, velMassRigidD, omegaLRF_D,
		  body.get_ptr(), radius, height, body->GetMass(), sphMarkerMass, type);

  referenceArray.push_back(mI3(numMarkers, numMarkers + numBce, type));
  numObjects.numRigidBodies += 1;
  numObjects.startRigidMarkers = numMarkers; // Arman : not sure if you need to set startFlexMarkers
  numObjects.numRigid_SphMarkers += numBce;
  numObjects.numAllMarkers = posRadH.size();
  mapIndex_H.push_back(numRigidObjects);
}
// =============================================================================

// Arman you still need local position of bce markers
void CreateMbdPhysicalSystemObjects(
    ChSystemParallelDVI& mphysicalSystem,
    thrust::host_vector<Real3>& posRadH,  // do not set the size here since you are using push back later
    thrust::host_vector<Real4>& velMasH,
    thrust::host_vector<Real4>& rhoPresMuH,
    thrust::host_vector<uint>& bodyIndex,
    		thrust::device_vector<Real3>& posRigidD,
    		thrust::device_vector<Real4>& qD,
    		thrust::device_vector<Real4>& velMassRigidD,
    		thrust::device_vector<Real3>& omegaLRF_D,
    		thrust::host_vector<int>& mapIndex_H,
    thrust::host_vector< ::int3>& referenceArray,
    NumberOfObjects& numObjects,
    const SimParams& paramsH,
    Real sphMarkerMass) {
  // Ground body
  ChSharedPtr<ChBody> ground = ChSharedPtr<ChBody>(new ChBody(new collision::ChCollisionModelParallel));
  ground->SetIdentifier(-1);
  ground->SetBodyFixed(true);
  ground->SetCollide(true);

  ground->GetMaterialSurface()->SetFriction(mu_g);

  ground->GetCollisionModel()->ClearModel();

  // Bottom box
  double hdimSide = hdimX / 4.0;
  double midSecDim = hdimX - 2 * hdimSide;

  // basin info
  double phi = CH_C_PI / 9;
  double bottomWidth = midSecDim - basinDepth / tan(phi);  // for a 45 degree slope
  double bottomBuffer = .4 * bottomWidth;

  double inclinedWidth = 0.5 * basinDepth / sin(phi);  // for a 45 degree slope

  double smallBuffer = .7 * hthick;
  double x1I = -midSecDim + inclinedWidth * cos(phi) - hthick * sin(phi) - smallBuffer;
  double zI = -inclinedWidth * sin(phi) - hthick * cos(phi);
  double x2I = midSecDim - inclinedWidth * cos(phi) + hthick * sin(phi) + smallBuffer;

  // beginning third
  AddBoxBceToChSystemAndSPH(ground.get_ptr(),
                            ChVector<>(hdimSide, hdimY, hthick),
                            ChVector<>(-midSecDim - hdimSide, 0, -hthick),
                            ChQuaternion<>(1, 0, 0, 0),
                            true,
                            posRadH,
                            velMasH,
                            rhoPresMuH,
                            bodyIndex,
                            referenceArray,
                            numObjects,
                            paramsH,
                            sphMarkerMass);

  // end third
  AddBoxBceToChSystemAndSPH(ground.get_ptr(),
                            ChVector<>(hdimSide, hdimY, hthick),
                            ChVector<>(midSecDim + hdimSide, 0, -hthick),
                            ChQuaternion<>(1, 0, 0, 0),
                            true,
                            posRadH,
                            velMasH,
                            rhoPresMuH,
                            bodyIndex,
                            referenceArray,
                            numObjects,
                            paramsH,
                            sphMarkerMass);
  // basin
  AddBoxBceToChSystemAndSPH(ground.get_ptr(),
                            ChVector<>(bottomWidth + bottomBuffer, hdimY, hthick),
                            ChVector<>(0, 0, -basinDepth - hthick),
                            ChQuaternion<>(1, 0, 0, 0),
                            true,
                            posRadH,
                            velMasH,
                            rhoPresMuH,
                            bodyIndex,
                            referenceArray,
                            numObjects,
                            paramsH,
                            sphMarkerMass);
  // slope 1
  AddBoxBceToChSystemAndSPH(ground.get_ptr(),
                            ChVector<>(inclinedWidth, hdimY, hthick),
                            ChVector<>(x1I, 0, zI),
                            Q_from_AngAxis(phi, ChVector<>(0, 1, 0)),
                            true,
                            posRadH,
                            velMasH,
                            rhoPresMuH,
                            bodyIndex,
                            referenceArray,
                            numObjects,
                            paramsH,
                            sphMarkerMass);

  // slope 2
  AddBoxBceToChSystemAndSPH(ground.get_ptr(),
                            ChVector<>(inclinedWidth, hdimY, hthick),
                            ChVector<>(x2I, 0, zI),
                            Q_from_AngAxis(-phi, ChVector<>(0, 1, 0)),
                            true,
                            posRadH,
                            velMasH,
                            rhoPresMuH,
                            bodyIndex,
                            referenceArray,
                            numObjects,
                            paramsH,
                            sphMarkerMass);

  // a flat surface altogether
  //  utils::AddBoxGeometry(
  //      ground.get_ptr(), ChVector<>(hdimX, hdimY, hthick), ChVector<>(0, 0, -hthick), ChQuaternion<>(1, 0, 0, 0),
  //      true);

  if (initializeFluidFromFile) {
    if (numObjects.numBoundaryMarkers > 0) {
      ground->GetCollisionModel()->SetFamily(fluidCollisionFamily);
      ground->GetCollisionModel()->SetFamilyMaskNoCollisionWithFamily(fluidCollisionFamily);
    }
  } else {
#if haveFluid
#if useWallBce
    ground->GetCollisionModel()->SetFamily(fluidCollisionFamily);
    ground->GetCollisionModel()->SetFamilyMaskNoCollisionWithFamily(fluidCollisionFamily);
#endif
#endif
  }

  ground->GetCollisionModel()->BuildModel();

  mphysicalSystem.AddBody(ground);


  double cyl_len = bottomWidth / 5;
  double cyl_rad = bottomWidth / 10;
  ChVector<> cyl_pos = ChVector<>(0, 0, 0);
  ChQuaternion<> cyl_rot = chrono::Q_from_AngAxis(CH_C_PI/3, VECT_Y);

  // version 0, create one cylinder // note: rigid body initialization should come after boundary initialization

  AddCylinderBceToChSystemAndSPH(mphysicalSystem, cyl_rad, cyl_len, cyl_pos, cyl_rot, true,
		  posRadH, velMasH, rhoPresMuH, referenceArray,
		  posRigidD, qD, velMassRigidD, omegaLRF_D, mapIndex_H,
		  numObjects, sphMarkerMass);

//						  // version 1
//						  // -----------------------------------------
//						  // Create and initialize the vehicle system.
//						  // -----------------------------------------
//						  // Create the vehicle assembly and the callback object for tire contact
//						  // according to the specified type of tire/wheel.
//						  switch (wheel_type) {
//							case CYLINDRICAL: {
//							  mVehicle = new ChWheeledVehicleAssembly(&mphysicalSystem, vehicle_file_cyl, simplepowertrain_file);
//							  tire_cb = new MyCylindricalTire();
//							} break;
//							case LUGGED: {
//							  mVehicle = new ChWheeledVehicleAssembly(&mphysicalSystem, vehicle_file_lug, simplepowertrain_file);
//							  tire_cb = new MyLuggedTire();
//							} break;
//						  }
//						  mVehicle->SetTireContactCallback(tire_cb);
//						  // Set the callback object for chassis.
//						  switch (chassis_type) {
//							case CBOX: {
//							  chassis_cb = new MyChassisBoxModel_vis();  //(mVehicle->GetVehicle()->GetChassis(), ChVector<>(1, .5, .4));
//							  ChVector<> boxSize(1, .5, .2);
//							  ((MyChassisBoxModel_vis*)chassis_cb)->SetAttributes(boxSize);
//							  mVehicle->SetChassisContactCallback(chassis_cb);
//							} break;
//
//							case CSPHERE: {
//							  chassis_cb = new MyChassisSphereModel_vis();  //(mVehicle->GetVehicle()->GetChassis(), ChVector<>(1, .5, .4));
//							  Real radius = 1;
//							  ((MyChassisSphereModel_vis*)chassis_cb)->SetAttributes(radius);
//							  mVehicle->SetChassisContactCallback(chassis_cb);
//							} break;
//
//							case C_SIMPLE_CONVEX_MESH: {
//							  chassis_cb = new MyChassisSimpleConvexMesh();  //(mVehicle->GetVehicle()->GetChassis(), ChVector<>(1, .5, .4));
//							  mVehicle->SetChassisContactCallback(chassis_cb);
//							} break;
//
//							case C_SIMPLE_TRI_MESH: {
//							  chassis_cb = new MyChassisSimpleTriMesh_vis();  //(mVehicle->GetVehicle()->GetChassis(), ChVector<>(1, .5, .4));
//							  mVehicle->SetChassisContactCallback(chassis_cb);
//							} break;
//						  }
//
//						  // Set the callback object for driver inputs. Pass the hold time as a delay in
//						  // generating driver inputs.
//						  driver_cb = new MyDriverInputs(time_hold_vehicle);
//						  mVehicle->SetDriverInputsCallback(driver_cb);
//
//						  // Initialize the vehicle at a height above the terrain.
//						  mVehicle->Initialize(initLoc + ChVector<>(0, 0, vertical_offset), initRot);
//
//						  // Initially, fix the chassis (will be released after time_hold_vehicle).
//						  mVehicle->GetVehicle()->GetChassis()->SetBodyFixed(true);
//						  // Initially, fix the wheels (will be released after time_hold_vehicle).
//						  for (int i = 0; i < 2 * mVehicle->GetVehicle()->GetNumberAxles(); i++) {
//							mVehicle->GetVehicle()->GetWheelBody(i)->SetBodyFixed(true);
//						  }

  //  // version 2
  //  // -----------------------------------------
  //  // Create and initialize the vehicle system.
  //  // -----------------------------------------
  //  std::string vehicle_file_cyl1("hmmwv/vehicle/HMMWV_Vehicle_simple.json");
  //  std::string vehicle_file_lug1("hmmwv/vehicle/HMMWV_Vehicle_simple_lugged.json");
  //
  //  // JSON files for powertrain (simple)
  //  std::string simplepowertrain_file1("hmmwv/powertrain/HMMWV_SimplePowertrain.json");
  //
  //  // Create the vehicle assembly and the callback object for tire contact
  //  // according to the specified type of tire/wheel.
  //  switch (wheel_type) {
  //      case CYLINDRICAL: {
  //    	  mVehicle = new ChWheeledVehicleAssembly(&mphysicalSystem, vehicle_file_cyl1, simplepowertrain_file1);
  //          tire_cb = new MyCylindricalTire();
  //      } break;
  //      case LUGGED: {
  //    	  mVehicle = new ChWheeledVehicleAssembly(&mphysicalSystem, vehicle_file_lug1, simplepowertrain_file1);
  //          tire_cb = new MyLuggedTire();
  //      } break;
  //  }
  //
  //  mVehicle->SetTireContactCallback(tire_cb);
  //
  //  // Set the callback object for driver inputs. Pass the hold time as a delay in
  //  // generating driver inputs.
  //  driver_cb = new MyDriverInputs(time_hold_vehicle);
  //  mVehicle->SetDriverInputsCallback(driver_cb);
  //
  //  // Initialize the vehicle at a height above the terrain.
  //  mVehicle->Initialize(initLoc + ChVector<>(0, 0, vertical_offset), initRot);
  //
  //  // Initially, fix the chassis and wheel bodies (will be released after time_hold).
  //  mVehicle->GetVehicle()->GetChassis()->SetBodyFixed(true);
  //  for (int i = 0; i < 2 * mVehicle->GetVehicle()->GetNumberAxles(); i++) {
  //	  mVehicle->GetVehicle()->GetWheelBody(i)->SetBodyFixed(true);
  //  }

  // extra objects
  // -----------------------------------------
  // Add extra collision body to test the collision shape
  // -----------------------------------------
  //
  //  Real rad = 0.1;
  //  // NOTE: mass properties and shapes are all for sphere
  //  double volume = utils::CalcSphereVolume(rad);
  //  ChVector<> gyration = utils::CalcSphereGyration(rad).Get_Diag();
  //  double density = paramsH.rho0;
  //  double mass = density * volume;
  //  double muFriction = 0;
  //
  //  // Create a common material
  //  ChSharedPtr<ChMaterialSurface> mat_g(new ChMaterialSurface);
  //  mat_g->SetFriction(muFriction);
  //  mat_g->SetCohesion(0);
  //  mat_g->SetCompliance(0.0);
  //  mat_g->SetComplianceT(0.0);
  //  mat_g->SetDampingF(0.2);
  //
  //  for (Real x = -4; x < 2; x += 0.25) {
  //    for (Real y = -1; y < 1; y += 0.25) {
  //      ChSharedPtr<ChBody> mball = ChSharedPtr<ChBody>(new ChBody(new collision::ChCollisionModelParallel));
  //      ChVector<> pos = ChVector<>(-8.5, .20, 3) + ChVector<>(x, y, 0);
  //      mball->SetMaterialSurface(mat_g);
  //      // body->SetIdentifier(fId);
  //      mball->SetPos(pos);
  //      mball->SetCollide(true);
  //      mball->SetBodyFixed(false);
  //      mball->SetMass(mass);
  //      mball->SetInertiaXX(mass * gyration);
  //
  //      mball->GetCollisionModel()->ClearModel();
  //      utils::AddSphereGeometry(mball.get_ptr(), rad);  // O
  //                                                       //	utils::AddEllipsoidGeometry(body.get_ptr(), size);
  //                                                       // X
  //
  //      mball->GetCollisionModel()->SetFamily(100);
  //      mball->GetCollisionModel()->SetFamilyMaskNoCollisionWithFamily(100);
  //
  //      mball->GetCollisionModel()->BuildModel();
  //      mphysicalSystem.AddBody(mball);
  //    }
  //  }
}
// =============================================================================

void InitializeChronoGraphics(ChSystemParallelDVI& mphysicalSystem) {
  //	Real3 domainCenter = 0.5 * (paramsH.cMin + paramsH.cMax);
  //	ChVector<> CameraLocation = ChVector<>(2 * paramsH.cMax.x, 2 * paramsH.cMax.y, 2 * paramsH.cMax.z);
  //	ChVector<> CameraLookAt = ChVector<>(domainCenter.x, domainCenter.y, domainCenter.z);
  ChVector<> CameraLocation = ChVector<>(0, -10, 0);
  ChVector<> CameraLookAt = ChVector<>(0, 0, 0);

#ifdef CHRONO_OPENGL
  gl_window.Initialize(1280, 720, "HMMWV", &mphysicalSystem);
  gl_window.SetCamera(CameraLocation, CameraLookAt, ChVector<>(0, 0, 1));
  gl_window.SetRenderMode(opengl::WIREFRAME);

// Uncomment the following two lines for the OpenGL manager to automatically un the simulation in an infinite loop.

// gl_window.StartDrawLoop(paramsH.dT);
// return 0;
#endif

#if irrlichtVisualization
  // Create the Irrlicht visualization (open the Irrlicht device,
  // bind a simple user interface, etc. etc.)
  application = std::shared_ptr<ChIrrApp>(
      new ChIrrApp(&mphysicalSystem, L"Bricks test", core::dimension2d<u32>(800, 600), false, true));
  //	ChIrrApp application(&mphysicalSystem, L"Bricks test",core::dimension2d<u32>(800,600),false, true);

  // Easy shortcuts to add camera, lights, logo and sky in Irrlicht scene:
  ChIrrWizard::add_typical_Logo(application->GetDevice());
  //		ChIrrWizard::add_typical_Sky   (application->GetDevice());
  ChIrrWizard::add_typical_Lights(
      application->GetDevice(), core::vector3df(14.0f, 44.0f, -18.0f), core::vector3df(-3.0f, 8.0f, 6.0f), 59, 40);
  ChIrrWizard::add_typical_Camera(
      application->GetDevice(),
      core::vector3df(CameraLocation.x, CameraLocation.y, CameraLocation.z),
      core::vector3df(CameraLookAt.x, CameraLookAt.y, CameraLookAt.z));  //   (7.2,30,0) :  (-3,12,-8)
  // Use this function for adding a ChIrrNodeAsset to all items
  // If you need a finer control on which item really needs a visualization proxy in
  // Irrlicht, just use application->AssetBind(myitem); on a per-item basis.
  application->AssetBindAll();
  // Use this function for 'converting' into Irrlicht meshes the assets
  // into Irrlicht-visualizable meshes
  application->AssetUpdateAll();

  application->SetStepManage(true);
#endif
}
// =============================================================================

void SavePovFilesMBD(ChSystemParallelDVI& mphysicalSystem,
                     int tStep,
                     double mTime,
                     int& num_contacts,
                     double exec_time) {
  int out_steps = std::ceil((1.0 / paramsH.dT) / out_fps);

  static int out_frame = 0;

  // If enabled, output data for PovRay postprocessing.
  if (povray_output && tStep % out_steps == 0) {
    if (tStep / out_steps == 0) {
      const std::string rmCmd = std::string("rm ") + pov_dir_mbd + std::string("/*.dat");
      system(rmCmd.c_str());
    }

    char filename[100];
    sprintf(filename, "%s/data_%03d.dat", pov_dir_mbd.c_str(), out_frame + 1);
    utils::WriteShapesPovray(&mphysicalSystem, filename);

    cout << "------------ Output frame:   " << out_frame + 1 << endl;
    cout << "             Sim frame:      " << tStep << endl;
    cout << "             Time:           " << mTime << endl;
    cout << "             Avg. contacts:  " << num_contacts / out_steps << endl;
    cout << "             Execution time: " << exec_time << endl;

    out_frame++;
    num_contacts = 0;
  }
}

// =============================================================================

void OutputVehicleData(ChSystemParallelDVI& mphysicalSystem, int tStep) {
  std::ofstream outVehicleData;
  const std::string vehicleDataFile = out_dir + "/vehicleData.txt";

  if (tStep == 0) {
    outVehicleData.open(vehicleDataFile);
  } else {
    outVehicleData.open(vehicleDataFile, std::ios::app);
  }
  ChVector<> accLF = ChTransform<>::TransformParentToLocal(mVehicle->GetVehicle()->GetChassis()->GetPos_dtdt(),
                                                           ChVector<>(0),
                                                           mVehicle->GetVehicle()->GetChassis()->GetRot());

  outVehicleData << mphysicalSystem.GetChTime() << ", " <<

      mVehicle->GetVehicle()->GetChassis()->GetPos().x << ", " << mVehicle->GetVehicle()->GetChassis()->GetPos().y
                 << ", " << mVehicle->GetVehicle()->GetChassis()->GetPos().z << ", " <<

      mVehicle->GetVehicle()->GetChassis()->GetPos_dt().x << ", " << mVehicle->GetVehicle()->GetChassis()->GetPos_dt().y
                 << ", " << mVehicle->GetVehicle()->GetChassis()->GetPos_dt().z << ", " <<

      mVehicle->GetVehicle()->GetChassis()->GetPos_dt().Length() << ", " <<

      mVehicle->GetVehicle()->GetChassis()->GetPos_dtdt().x << ", "
                 << mVehicle->GetVehicle()->GetChassis()->GetPos_dtdt().y << ", "
                 << mVehicle->GetVehicle()->GetChassis()->GetPos_dtdt().z << ", " <<

      accLF.x << ", " << accLF.y << ", " << accLF.z << ", " <<

      mVehicle->GetPowertrain()->GetMotorTorque() << ", " << mVehicle->GetPowertrain()->GetMotorSpeed() << ", "
                 << mVehicle->GetPowertrain()->GetOutputTorque() << ", "
                 << mVehicle->GetPowertrain()->GetCurrentTransmissionGear() << ", "
                 << mVehicle->GetPowertrain()->GetMaxTorque() << ", " << mVehicle->GetPowertrain()->GetMaxSpeed()
                 << ", " <<

      std::endl;
}
// =============================================================================
void FreezeSPH(thrust::device_vector<Real4>& velMasD, thrust::host_vector<Real4>& velMasH) {
  for (int i = 0; i < velMasH.size(); i++) {
    Real4 vM = velMasH[i];
    velMasH[i] = mR4(0, 0, 0, vM.w);
    velMasD[i] = mR4(0, 0, 0, vM.w);
  }
}
// =============================================================================
void printSimulationParameters() {
  simParams << " time_hold_vehicle: " << time_hold_vehicle << endl
            << " time_pause_fluid_external_force: " << time_pause_fluid_external_force << endl
            << " contact_recovery_speed: " << contact_recovery_speed << endl << " maxFlowVelocity " << maxFlowVelocity
            << endl << " time_step (paramsH.dT): " << paramsH.dT << endl << " time_end: " << time_end << endl;
}
// =============================================================================

int DoStepChronoSystem(ChSystemParallelDVI& mphysicalSystem, Real dT, double mTime) {
  // Release the vehicle chassis at the end of the hold time.
  if (mVehicle->GetVehicle()->GetChassis()->GetBodyFixed() && mTime > time_hold_vehicle) {
    mVehicle->GetVehicle()->GetChassis()->SetBodyFixed(false);
    for (int i = 0; i < 2 * mVehicle->GetVehicle()->GetNumberAxles(); i++) {
      mVehicle->GetVehicle()->GetWheelBody(i)->SetBodyFixed(false);
    }
  }

  // Update vehicle
  mVehicle->Update(mTime);

#if irrlichtVisualization
  Real3 domainCenter = 0.5 * (paramsH.cMin + paramsH.cMax);
  if (!(application->GetDevice()->run()))
    return 0;
  application->SetTimestep(dT);
  application->GetVideoDriver()->beginScene(true, true, SColor(255, 140, 161, 192));
  ChIrrTools::drawGrid(application->GetVideoDriver(),
                       2 * paramsH.HSML,
                       2 * paramsH.HSML,
                       50,
                       50,
                       ChCoordsys<>(ChVector<>(domainCenter.x, paramsH.worldOrigin.y, domainCenter.z),
                                    Q_from_AngAxis(CH_C_PI / 2, VECT_X)),
                       video::SColor(50, 90, 90, 150),
                       true);
  application->DrawAll();
  application->DoStep();
  application->GetVideoDriver()->endScene();
#else
#ifdef CHRONO_OPENGL
  if (gl_window.Active()) {
    gl_window.DoStepDynamics(dT);
    gl_window.Render();
  }
#else
  mphysicalSystem.DoStepDynamics(dT);
#endif
#endif
  return 1;
}
// =============================================================================

int main(int argc, char* argv[]) {
  //****************************************************************************************
  time_t rawtime;
  struct tm* timeinfo;

  GpuTimer myGpuTimerHalfStep;
  ChTimer<double> myCpuTimerHalfStep;
  //(void) cudaSetDevice(0);

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  //****************************************************************************************
  // Arman take care of this block.
  // Set path to ChronoVehicle data files
  //  vehicle::SetDataPath(CHRONO_VEHICLE_DATA_DIR);
  //  vehicle::SetDataPath("/home/arman/Repos/GitBeta/chrono/src/demos/data/");
  SetChronoDataPath(CHRONO_DATA_DIR);

  // --------------------------
  // Create output directories.
  // --------------------------

  if (ChFileutils::MakeDirectory(out_dir.c_str()) < 0) {
    cout << "Error creating directory " << out_dir << endl;
    return 1;
  }

  if (povray_output) {
    if (ChFileutils::MakeDirectory(pov_dir_mbd.c_str()) < 0) {
      cout << "Error creating directory " << pov_dir_mbd << endl;
      return 1;
    }
  }

  if (ChFileutils::MakeDirectory(pov_dir_fluid.c_str()) < 0) {
    cout << "Error creating directory " << pov_dir_fluid << endl;
    return 1;
  }

  //****************************************************************************************
  const std::string simulationParams = out_dir + "/simulation_specific_parameters.txt";
  simParams.open(simulationParams);
  simParams << " Job was submitted at date/time: " << asctime(timeinfo) << endl;
  printSimulationParameters();
  // ***************************** Create Fluid ********************************************
  thrust::host_vector< ::int3> referenceArray;
  thrust::host_vector<Real3> posRadH;  // do not set the size here since you are using push back later
  thrust::host_vector<Real4> velMasH;
  thrust::host_vector<Real4> rhoPresMuH;
  thrust::host_vector<uint> bodyIndex;

	thrust::device_vector<Real3> posRigidD, posRigidD2;
	thrust::device_vector<Real4> qD, qD2;
	thrust::device_vector<Real4> velMassRigidD, velMassRigidD2;
	thrust::device_vector<Real3> omegaLRF_D, omegaLRF_D2;
	thrust::host_vector<int> mapIndex_H;

  Real sphMarkerMass = 0;  // To be initialized in CreateFluidMarkers, and used in other places

  SetupParamsH(paramsH);

  if (initializeFluidFromFile) {
    // call to CheckPointMarkers_Read should be as close to the top as possible
    CheckPointMarkers_Read(
        initializeFluidFromFile, posRadH, velMasH, rhoPresMuH, bodyIndex, referenceArray, paramsH, numObjects);
    if (numObjects.numAllMarkers == 0) {
      ClearArraysH(posRadH, velMasH, rhoPresMuH, bodyIndex, referenceArray);
      return 0;
    }
#if haveFluid
#else
    printf("Error! Initialized from file But haveFluid is false! \n");
    return -1;
#endif
  } else {
#if haveFluid

    //*** default num markers

    int numAllMarkers = 0;

    //*** initialize fluid particles
    ::int2 num_fluidOrBoundaryMarkers =
        CreateFluidMarkers(posRadH, velMasH, rhoPresMuH, bodyIndex, paramsH, sphMarkerMass);
    printf("num_fluidOrBoundaryMarkers %d %d \n", num_fluidOrBoundaryMarkers.x, num_fluidOrBoundaryMarkers.y);
    referenceArray.push_back(mI3(0, num_fluidOrBoundaryMarkers.x, -1));  // map fluid -1
    numAllMarkers += num_fluidOrBoundaryMarkers.x;
    referenceArray.push_back(mI3(numAllMarkers, numAllMarkers + num_fluidOrBoundaryMarkers.y, 0));
    numAllMarkers += num_fluidOrBoundaryMarkers.y;

    //*** set num objects

    SetNumObjects(numObjects, referenceArray, numAllMarkers);
    assert(posRadH.size() == numObjects.numAllMarkers && "numObjects is not set correctly");
    if (numObjects.numAllMarkers == 0) {
      ClearArraysH(posRadH, velMasH, rhoPresMuH, bodyIndex, referenceArray);
      return 0;
    }
#endif
  }
  // ***************************** Create Rigid ********************************************
  ChSystemParallelDVI mphysicalSystem;
  InitializeMbdPhysicalSystem(mphysicalSystem, argc, argv);

  // This needs to be called after fluid initialization because I am using "numObjects.numBoundaryMarkers" inside it



  CreateMbdPhysicalSystemObjects(
      mphysicalSystem, posRadH, velMasH, rhoPresMuH, bodyIndex,
      posRigidD,qD,velMassRigidD,omegaLRF_D, mapIndex_H,
      referenceArray, numObjects, paramsH, sphMarkerMass);
    // ***************************** Create Interface ********************************************

  //*** Add sph data to the physics system

  int startIndexSph = 0;
#if haveFluid
  thrust::device_vector<Real3> posRadD = posRadH;
  thrust::device_vector<Real4> velMasD = velMasH;
  thrust::device_vector<Real4> rhoPresMuD = rhoPresMuH;
  thrust::device_vector<uint> bodyIndexD = bodyIndex;
  thrust::device_vector<Real4> derivVelRhoD;
  ResizeR4(derivVelRhoD, numObjects.numAllMarkers);




  thrust::device_vector<uint> rigidIdentifierD;
  ResizeU1(rigidIdentifierD, numObjects.numRigid_SphMarkers);
  thrust::device_vector<Real3> rigidSPH_MeshPos_LRF_D;
  ResizeR3(rigidSPH_MeshPos_LRF_D, numObjects.numRigid_SphMarkers);
  Populate_RigidSPH_MeshPos_LRF(rigidIdentifierD, rigidSPH_MeshPos_LRF_D, posRadD, posRigidD, qD, referenceArray, numObjects);




  // ** initialize device mid step data
  thrust::device_vector<Real3> posRadD2 = posRadD;
  thrust::device_vector<Real4> velMasD2 = velMasD;
  thrust::device_vector<Real4> rhoPresMuD2 = rhoPresMuD;
  thrust::device_vector<Real3> vel_XSPH_D;



  thrust::device_vector<Real3> rigid_FSI_ForcesD;
  thrust::device_vector<Real3> rigid_FSI_TorquesD;
  ResizeR3(rigid_FSI_ForcesD, numObjects.numRigidBodies);
  ResizeR3(rigid_FSI_TorquesD, numObjects.numRigidBodies);

#endif
  cout << " -- ChSystem size : " << mphysicalSystem.Get_bodylist()->size() << endl;

  // ***************************** System Initialize ********************************************

  InitializeChronoGraphics(mphysicalSystem);

  double mTime = 0;
  double exec_time = 0;
  int num_contacts = 0;

  DOUBLEPRECISION ? printf("Double Precision\n") : printf("Single Precision\n");

  int stepEnd = int(paramsH.tFinal / paramsH.dT);  // 1.0e6;//2.4e6;//600000;//2.4e6 * (.02 * paramsH.sizeScale) /
                                                   // currentParamsH.dT ; //1.4e6 * (.02 * paramsH.sizeScale) /
                                                   // currentParamsH.dT ;//0.7e6 * (.02 * paramsH.sizeScale) /
                                                   // currentParamsH.dT ;//0.7e6;//2.5e6;
                                                   // //200000;//10000;//50000;//100000;
  printf("stepEnd %d\n", stepEnd);
  Real realTime = 0;

  SimParams paramsH_B = paramsH;
  paramsH_B.bodyForce3 = mR3(0);
  paramsH_B.gravity = mR3(0);
  paramsH_B.dT = paramsH.dT;

  printf("\ntimePause %f, numPause %d\n", paramsH.timePause, int(paramsH.timePause / paramsH_B.dT));
  printf("paramsH.timePauseRigidFlex %f, numPauseRigidFlex %d\n\n",
         paramsH.timePauseRigidFlex,
         int((paramsH.timePauseRigidFlex - paramsH.timePause) / paramsH.dT + paramsH.timePause / paramsH_B.dT));
  //  InitSystem(paramsH, numObjects);
  SimParams currentParamsH = paramsH;

  simParams.close();

  // ******************************************************************************************
  // ******************************************************************************************
  // ******************************************************************************************
  // ******************************************************************************************
  // ***************************** Simulation loop ********************************************

  for (int tStep = 0; tStep < stepEnd + 1; tStep++) {
    // -------------------
    // SPH Block
    // -------------------
    myCpuTimerHalfStep.start();
    myGpuTimerHalfStep.Start();
    fsi_timer.Reset();

#if haveFluid
    CpuTimer mCpuTimer;
    mCpuTimer.Start();
    GpuTimer myGpuTimer;
    myGpuTimer.Start();

    //		CopySys2D(posRadD, mphysicalSystem, numObjects, startIndexSph);

    fsi_timer.start("half_step_dynamic_fsi_12");
    fsi_timer.start("fluid_initialization");

    int out_steps = std::ceil((1.0 / paramsH.dT) / out_fps);
    PrintToFile(
        posRadD, velMasD, rhoPresMuD, referenceArray, currentParamsH, realTime, tStep, out_steps, pov_dir_fluid);

    // ******* slow down the sys.Check point the sys.
    CheckPointMarkers_Write(
        posRadH, velMasH, rhoPresMuH, bodyIndex, referenceArray, paramsH, numObjects, tStep, tStepsCheckPoint);

//    // freeze sph. check it later
//    if (fmod(realTime, 0.6) < paramsH.dT && realTime < 1.3) {
//      FreezeSPH(velMasD, velMasH);
//    }
    // *******

    if (realTime <= paramsH.timePause) {
      currentParamsH = paramsH_B;
    } else {
      currentParamsH = paramsH;
    }
    InitSystem(currentParamsH, numObjects);
    mphysicalSystem.Set_G_acc(ChVector<>(currentParamsH.gravity.x, currentParamsH.gravity.y, currentParamsH.gravity.z));

    // ** initialize host mid step data
    thrust::copy(posRadD.begin(), posRadD.end(), posRadD2.begin());
    thrust::copy(velMasD.begin(), velMasD.end(), velMasD2.begin());
    thrust::copy(rhoPresMuD2.begin(), rhoPresMuD2.end(), rhoPresMuD.begin());

    FillMyThrust4(derivVelRhoD, mR4(0));

    fsi_timer.stop("fluid_initialization");

#endif
    // -------------------
    // End SPH Block
    // -------------------

    // If enabled, output data for PovRay postprocessing.
    SavePovFilesMBD(mphysicalSystem, tStep, mTime, num_contacts, exec_time);
// ******************
// ******************
// ******************
// ******************
// ****************** RK2: 1/2
#if haveFluid


    fsi_timer.start("integrate_sph");
    // //assumes ...D2 is a copy of ...D


    IntegrateSPH(derivVelRhoD, posRadD2, velMasD2, rhoPresMuD2,
                      posRadD, velMasD, vel_XSPH_D, rhoPresMuD,
                      bodyIndexD, referenceArray, numObjects, currentParamsH,  0.5 * currentParamsH.dT); // Arman vel_XSPH_D does not need to be sent essentially
    fsi_timer.start("integrate_sph");


    Rigid_Forces_Torques(rigid_FSI_ForcesD, rigid_FSI_TorquesD,
    		posRadD, posRigidD, derivVelRhoD, rigidIdentifierD, numObjects, sphMarkerMass);
    Add_Rigid_ForceTorques_To_ChSystem(mphysicalSystem, rigid_FSI_ForcesD, rigid_FSI_TorquesD, mapIndex_H);


#endif

    fsi_timer.start("stepDynamic_mbd");
    mTime += 0.5 * currentParamsH.dT;
    DoStepChronoSystem(
        mphysicalSystem, 0.5 * currentParamsH.dT, mTime);  // Keep only this if you are just interested in the rigid sys

    fsi_timer.stop("stepDynamic_mbd");

#if haveFluid


    Update_RigidPosVel_from_ChSystem_H2D(posRigidD2, qD2, velMassRigidD2, omegaLRF_D2, mapIndex_H, mphysicalSystem);
    UpdateRigidMarkersPosition(posRadD2, velMasD2,
    		rigidSPH_MeshPos_LRF_D, rigidIdentifierD, posRigidD2, qD2, velMassRigidD2, omegaLRF_D2, numObjects); // Arman rigidSPH_MeshPos_LRF_D, rigidIdentifierD, numObjects

#endif


// ******************
// ******************
// ******************
// ******************
// ****************** RK2: 2/2
    FillMyThrust4(derivVelRhoD, mR4(0));

#if haveFluid

    // //assumes ...D2 is a copy of ...D
    IntegrateSPH(derivVelRhoD, posRadD, velMasD, rhoPresMuD,
                      posRadD2, velMasD2, vel_XSPH_D, rhoPresMuD2,
                      bodyIndexD, referenceArray, numObjects, currentParamsH,  currentParamsH.dT);
    fsi_timer.start("integrate_sph");

    Rigid_Forces_Torques(rigid_FSI_ForcesD, rigid_FSI_TorquesD,
    		posRadD2, posRigidD2, derivVelRhoD, rigidIdentifierD, numObjects, sphMarkerMass);
    Add_Rigid_ForceTorques_To_ChSystem(mphysicalSystem, rigid_FSI_ForcesD, rigid_FSI_TorquesD, mapIndex_H); // Arman: take care of this


#endif
    mTime -= 0.5 * currentParamsH.dT;

    // Arman: do it so that you don't need gpu when you don't have fluid
    HardSet_PosRot_In_ChSystem_D2H(mphysicalSystem, posRigidD, qD, velMassRigidD, omegaLRF_D, mapIndex_H);

    fsi_timer.start("stepDynamic_mbd");



    mTime += currentParamsH.dT;
    DoStepChronoSystem(
        mphysicalSystem, currentParamsH.dT, mTime);  // Keep only this if you are just interested in the rigid sys

    fsi_timer.stop("stepDynamic_mbd");

#if haveFluid

    Update_RigidPosVel_from_ChSystem_H2D(posRigidD, qD, velMassRigidD, omegaLRF_D, mapIndex_H, mphysicalSystem);
    UpdateRigidMarkersPosition(posRadD, velMasD,
    		rigidSPH_MeshPos_LRF_D, rigidIdentifierD, posRigidD, qD, velMassRigidD, omegaLRF_D, numObjects);


    if ((tStep % 10 == 0) && (paramsH.densityReinit != 0)) {
      DensityReinitialization(posRadD, velMasD, rhoPresMuD, numObjects.numAllMarkers, paramsH.gridSize);
    }

#endif



// ****************** End RK2

//    OutputVehicleData(mphysicalSystem, tStep);

    // Update counters.
    exec_time += mphysicalSystem.GetTimerStep();
    num_contacts += mphysicalSystem.GetNcontacts();

// -------------------
// SPH Block
// -------------------

#if haveFluid
    mCpuTimer.Stop();
    myGpuTimer.Stop();
    if (tStep % 2 == 0) {
      printf("step: %d, realTime: %f, step Time (CUDA): %f, step Time (CPU): %f\n ",
             tStep,
             realTime,
             (Real)myGpuTimer.Elapsed(),
             1000 * mCpuTimer.Elapsed());

      fsi_timer.stop("half_step_dynamic_fsi_22");
      fsi_timer.PrintReport();
    }
#endif
    // -------------------
    // End SPH Block
    // -------------------

    fflush(stdout);
    realTime += currentParamsH.dT;

    mphysicalSystem.data_manager->system_timer.PrintReport();
  }
  ClearArraysH(posRadH, velMasH, rhoPresMuH, bodyIndex, referenceArray);

  ClearMyThrustR3(posRigidD);
  ClearMyThrustR4(qD);
  ClearMyThrustR4(velMassRigidD);
  ClearMyThrustR3(omegaLRF_D);
  ClearMyThrustR3(posRigidD2);
  ClearMyThrustR4(qD2);
  ClearMyThrustR4(velMassRigidD2);
  ClearMyThrustR3(omegaLRF_D2);
  mapIndex_H.clear();
#if haveFluid
  ClearMyThrustR3(posRadD);
  ClearMyThrustR4(velMasD);
  ClearMyThrustR4(rhoPresMuD);
  ClearMyThrustU1(bodyIndexD);
  ClearMyThrustR4(derivVelRhoD);
  ClearMyThrustU1(rigidIdentifierD);
  ClearMyThrustR3(rigidSPH_MeshPos_LRF_D);


  ClearMyThrustR3(posRadD2);
  ClearMyThrustR4(velMasD2);
  ClearMyThrustR4(rhoPresMuD2);
  ClearMyThrustR3(vel_XSPH_D);

  ClearMyThrustR3(rigid_FSI_ForcesD);
  ClearMyThrustR3(rigid_FSI_TorquesD);
#endif
  delete mVehicle;
  delete tire_cb;
  delete chassis_cb;
  delete driver_cb;

  return 0;
}