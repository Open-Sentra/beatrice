#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Beatrice::beatrice_core" for configuration "Release"
set_property(TARGET Beatrice::beatrice_core APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Beatrice::beatrice_core PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "spdlog::spdlog"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib64/libbeatrice_core.so.1.0.0"
  IMPORTED_SONAME_RELEASE "libbeatrice_core.so.1"
  )

list(APPEND _cmake_import_check_targets Beatrice::beatrice_core )
list(APPEND _cmake_import_check_files_for_Beatrice::beatrice_core "${_IMPORT_PREFIX}/lib64/libbeatrice_core.so.1.0.0" )

# Import target "Beatrice::beatrice" for configuration "Release"
set_property(TARGET Beatrice::beatrice APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Beatrice::beatrice PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/beatrice"
  )

list(APPEND _cmake_import_check_targets Beatrice::beatrice )
list(APPEND _cmake_import_check_files_for_Beatrice::beatrice "${_IMPORT_PREFIX}/bin/beatrice" )

# Import target "Beatrice::beatrice_cli" for configuration "Release"
set_property(TARGET Beatrice::beatrice_cli APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Beatrice::beatrice_cli PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/beatrice_cli"
  )

list(APPEND _cmake_import_check_targets Beatrice::beatrice_cli )
list(APPEND _cmake_import_check_files_for_Beatrice::beatrice_cli "${_IMPORT_PREFIX}/bin/beatrice_cli" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
