#=============================================================================
# CMake configuration file for Chrono Vehicle Models
#
# Cannot be used stand-alone (it is loaded by parent CMake configuration file)
#=============================================================================

message(STATUS "Models for the VEHICLE module...")

# ----------------------------------------------------------------------------
# List the files in the Chrono_vehicle_models library
# ----------------------------------------------------------------------------

set(CVM_COMMON_FILES
    ../ChApiModels.h
)
source_group("" FILES ${CVM_COMMON_FILES})

set(CVM_HMMWV_FILES
    hmmwv/HMMWV.cpp
    hmmwv/HMMWV.h
    hmmwv/HMMWV_BrakeSimple.cpp
    hmmwv/HMMWV_BrakeSimple.h
    hmmwv/HMMWV_Chassis.h
    hmmwv/HMMWV_Chassis.cpp
    hmmwv/HMMWV_DoubleWishbone.cpp
    hmmwv/HMMWV_DoubleWishbone.h
    hmmwv/HMMWV_DoubleWishboneReduced.cpp
    hmmwv/HMMWV_DoubleWishboneReduced.h
    hmmwv/HMMWV_Driveline2WD.cpp
    hmmwv/HMMWV_Driveline2WD.h
    hmmwv/HMMWV_Driveline4WD.cpp
    hmmwv/HMMWV_Driveline4WD.h
    hmmwv/HMMWV_FialaTire.cpp
    hmmwv/HMMWV_FialaTire.h
    hmmwv/HMMWV_LugreTire.cpp
    hmmwv/HMMWV_LugreTire.h
    hmmwv/HMMWV_PitmanArm.cpp
    hmmwv/HMMWV_PitmanArm.h
    hmmwv/HMMWV_Powertrain.cpp
    hmmwv/HMMWV_Powertrain.h
    hmmwv/HMMWV_RackPinion.cpp
    hmmwv/HMMWV_RackPinion.h
    hmmwv/HMMWV_RigidTire.cpp
    hmmwv/HMMWV_RigidTire.h
    hmmwv/HMMWV_SimpleDriveline.cpp
    hmmwv/HMMWV_SimpleDriveline.h
    hmmwv/HMMWV_SimplePowertrain.cpp
    hmmwv/HMMWV_SimplePowertrain.h
    hmmwv/HMMWV_Vehicle.h
    hmmwv/HMMWV_VehicleFull.cpp
    hmmwv/HMMWV_VehicleFull.h
    hmmwv/HMMWV_VehicleReduced.cpp
    hmmwv/HMMWV_VehicleReduced.h
    hmmwv/HMMWV_Wheel.cpp
    hmmwv/HMMWV_Wheel.h
)
if(ENABLE_MODULE_FEA)
  set(CVM_HMMWV_FEA_FILES
      hmmwv/HMMWV_ANCFTire.cpp
      hmmwv/HMMWV_ANCFTire.h
	  hmmwv/HMMWV_ReissnerTire.cpp
      hmmwv/HMMWV_ReissnerTire.h
  )
else()
  set(CVM_HMMWV_FEA_FILES "")
endif()
source_group("hmmwv" FILES ${CVM_HMMWV_FILES} ${CVM_HMMWV_FEA_FILES})

set(CVM_GENERIC_FILES
    generic/Generic_AntirollBarRSD.h
    generic/Generic_AntirollBarRSD.cpp
    generic/Generic_BrakeSimple.h
    generic/Generic_BrakeSimple.cpp
    generic/Generic_Chassis.cpp
    generic/Generic_Chassis.h
    generic/Generic_DoubleWishbone.cpp
    generic/Generic_DoubleWishbone.h
    generic/Generic_Driveline2WD.h
    generic/Generic_Driveline2WD.cpp
    generic/Generic_FuncDriver.h
    generic/Generic_HendricksonPRIMAXX.cpp
    generic/Generic_HendricksonPRIMAXX.h
    generic/Generic_MacPhersonStrut.cpp
    generic/Generic_MacPhersonStrut.h
    generic/Generic_MultiLink.cpp
    generic/Generic_MultiLink.h
    generic/Generic_RackPinion.h
    generic/Generic_RackPinion.cpp
    generic/Generic_RigidTire.h
    generic/Generic_RigidTire.cpp
    generic/Generic_SimplePowertrain.h
    generic/Generic_SimplePowertrain.cpp
    generic/Generic_SolidAxle.cpp
    generic/Generic_SolidAxle.h
    generic/Generic_Vehicle.cpp
    generic/Generic_Vehicle.h
    generic/Generic_Wheel.h
    generic/Generic_Wheel.cpp
)
source_group("generic" FILES ${CVM_GENERIC_FILES})

set(CVM_M113_FILES
    m113/M113_BrakeSimple.h
    m113/M113_Chassis.h
    m113/M113_Chassis.cpp
    m113/M113_DrivelineBDS.cpp
    m113/M113_DrivelineBDS.h
    m113/M113_Idler.cpp
    m113/M113_Idler.h
    m113/M113_RoadWheel.cpp
    m113/M113_RoadWheel.h
    m113/M113_ShaftsPowertrain.cpp
    m113/M113_ShaftsPowertrain.h
    m113/M113_SimpleDriveline.cpp
    m113/M113_SimpleDriveline.h
    m113/M113_SimplePowertrain.cpp
    m113/M113_SimplePowertrain.h
    m113/M113_SprocketSinglePin.cpp
    m113/M113_SprocketSinglePin.h
    m113/M113_SprocketDoublePin.cpp
    m113/M113_SprocketDoublePin.h
    m113/M113_Suspension.cpp
    m113/M113_Suspension.h
    m113/M113_TrackAssemblySinglePin.cpp
    m113/M113_TrackAssemblySinglePin.h
    m113/M113_TrackAssemblyDoublePin.cpp
    m113/M113_TrackAssemblyDoublePin.h
    m113/M113_TrackShoeSinglePin.cpp
    m113/M113_TrackShoeSinglePin.h
    m113/M113_TrackShoeDoublePin.cpp
    m113/M113_TrackShoeDoublePin.h
    m113/M113_Vehicle.cpp
    m113/M113_Vehicle.h
)
source_group("m113" FILES ${CVM_M113_FILES})


# ----------------------------------------------------------------------------
# Add the Chrono_vehicle_models library
# ----------------------------------------------------------------------------

set(CXX_FLAGS ${CH_CXX_FLAGS})
set(LIBRARIES ChronoEngine ChronoEngine_vehicle)
if(ENABLE_MODULE_IRRLICHT)
    set(CXX_FLAGS "${CXX_FLAGS} ${CH_IRRLICHT_CXX_FLAGS}")
    list(APPEND LIBRARIES ChronoEngine_irrlicht)
    list(APPEND LIBRARIES ${CH_IRRKLANGLIB})
    include_directories(${CH_IRRLICHTINC})
endif()

if(ENABLE_MODULE_FEA)
    list(APPEND LIBRARIES ChronoEngine_fea)
endif()

add_library(ChronoModels_vehicle SHARED
    ${CVM_COMMON_FILES}
    ${CVM_HMMWV_FILES}
    ${CVM_HMMWV_FEA_FILES}
    ${CVM_GENERIC_FILES}
    ${CVM_M113_FILES}
 )

set_target_properties(ChronoModels_vehicle PROPERTIES
                      COMPILE_FLAGS "${CXX_FLAGS}"
                      LINK_FLAGS "${CH_LINKERFLAG_SHARED}"
                      COMPILE_DEFINITIONS "CH_API_COMPILE_MODELS")

target_link_libraries(ChronoModels_vehicle ${LIBRARIES})

install(TARGETS ChronoModels_vehicle
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib64
        ARCHIVE DESTINATION lib64)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/chrono_vehicle/models/vehicle
        FILES_MATCHING PATTERN "*.h")
