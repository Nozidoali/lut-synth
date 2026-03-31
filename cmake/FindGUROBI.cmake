find_path(GUROBI_INCLUDE_DIRS
  NAMES gurobi_c.h
  HINTS
    ENV GUROBI_HOME
    /Library/gurobi1203/macos_universal2
    /opt/gurobi*/linux64
    /opt/gurobi*/macos_universal2
  PATH_SUFFIXES include
)

find_library(GUROBI_LIBRARY
  NAMES gurobi120 gurobi110 gurobi100 gurobi95 gurobi91
  HINTS
    ENV GUROBI_HOME
    /Library/gurobi1203/macos_universal2
    /opt/gurobi*/linux64
    /opt/gurobi*/macos_universal2
  PATH_SUFFIXES lib
)

find_library(GUROBI_CXX_LIBRARY
  NAMES gurobi_c++
  HINTS
    ENV GUROBI_HOME
    /Library/gurobi1203/macos_universal2
    /opt/gurobi*/linux64
    /opt/gurobi*/macos_universal2
  PATH_SUFFIXES lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GUROBI DEFAULT_MSG
  GUROBI_LIBRARY
  GUROBI_CXX_LIBRARY
  GUROBI_INCLUDE_DIRS
)

if(GUROBI_FOUND)
  set(GUROBI_LIBRARIES ${GUROBI_CXX_LIBRARY} ${GUROBI_LIBRARY})
endif()
