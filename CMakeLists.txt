set(ECN_FINAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/traffic-generator/traffic-generator.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/vbr-generator/vbr-generator.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/ecn-monitor/ecn-monitor.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/ecn-controller/ecn-controller.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/bdp-monitor/bdp-monitor.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/bdp-controller/bdp-controller.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/simulation/main.cc
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/traffic-generator
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/vbr-generator
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/ecn-monitor
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/ecn-controller
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/bdp-monitor
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/bdp-controller
)

build_exec(
    EXECNAME ecn-final
    SOURCE_FILES ${ECN_FINAL_SOURCES}
    LIBRARIES_TO_LINK
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libapplications}
        ${libpoint-to-point}
        ${libtraffic-control}
        ${libflow-monitor}
        ${libmobility}
        ${libnetanim}
    EXECUTABLE_DIRECTORY_PATH
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scratch/ECN-Conjestion-Control_FINAL/
)