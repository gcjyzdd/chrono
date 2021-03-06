// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All right reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Radu Serban
// =============================================================================
//
// Chassis subsystem for the generic vehicle.
//
// =============================================================================

#include "chrono/assets/ChTriangleMeshShape.h"
#include "chrono/utils/ChUtilsInputOutput.h"

#include "chrono_vehicle/ChVehicleModelData.h"

#include "chrono_models/vehicle/generic/Generic_Chassis.h"

namespace chrono {
namespace vehicle {
namespace generic {

// -----------------------------------------------------------------------------
// Static variables
// -----------------------------------------------------------------------------
const double Generic_Chassis::m_mass = 995.0;
const ChVector<> Generic_Chassis::m_inertia(200.0, 500.0, 600.0);
const ChVector<> Generic_Chassis::m_COM_loc(0, 0, 0);
const ChCoordsys<> Generic_Chassis::m_driverCsys(ChVector<>(0.0, 0.5, 1.2), ChQuaternion<>(1, 0, 0, 0));

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
Generic_Chassis::Generic_Chassis(const std::string& name) : ChChassis(name) {}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
void Generic_Chassis::AddVisualizationAssets(VisualizationType vis) {
    ChChassis::AddVisualizationAssets(vis);
}

}  // end namespace generic
}  // end namespace vehicle
}  // end namespace chrono
