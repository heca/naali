configure_protobuf ()
configure_openssl ()
configure_mumbleclient ()
configure_celt ()

# Define target name and output directory
init_target (MumbleVoipModule OUTPUT plugins)

# Define source files
file (GLOB CPP_FILES *.cpp)
file (GLOB H_FILES *.h )
file (GLOB XML_FILES *.xml)
file (GLOB UI_FILES ui/*.ui)
file (GLOB RESOURCE_FILES resource/*.qrc)

# Qt4 Moc files to subgroup "CMake Moc"
# and ui_*.h generated .h files to "Generated UI" subgroup
MocFolder ()
UiFolder ()

set (SOURCE_FILES ${SOURCE_FILES} ${CPP_FILES} ${H_FILES})

# Qt4 Wrap 
QT4_WRAP_CPP(MOC_SRCS ${H_FILES})
QT4_WRAP_UI(UI_SRCS ${UI_FILES})
QT4_ADD_RESOURCES(RESOURCE_SRCS ${RESOURCE_FILES})

add_definitions (-DMUMBLE_VOIP_MODULE_EXPORTS)

use_package (PROTOBUF) # NEEDED BY MUMBLECLIENT
use_package (OPENSSL) # NEEDED BY MUMBLECLIENT
use_package (MUMBLECLIENT)
use_package (CELT)

use_core_modules (Framework OgreRenderingModule Scene Audio TundraProtocolModule Console)

build_library (${TARGET_NAME} SHARED ${SOURCE_FILES} ${MOC_SRCS} ${UI_SRCS} ${RESOURCE_SRCS})

link_modules (Framework OgreRenderingModule Scene Audio TundraProtocolModule Console)

link_package (PROTOBUF) # NEEDED BY MUMBLECLIENT
link_package (OPENSSL) # NEEDED BY MUMBLECLIENT
link_package (MUMBLECLIENT)
link_package (CELT)

SetupCompileFlagsWithPCH()

final_target ()