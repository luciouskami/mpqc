# Add MPQC library
# add_mpqc_library(LIBRARY SUBDIRS)
# LIBRARY - name of the library
# OBJECTS - library subdirs to include in LIBRARY
# add_mpqc_library automatically adds dependencies on External target
# Example: add_mpqc_library(util class ref render)

macro(add_mpqc_library LIBRARY )
  #message("Library \"${LIBRARY}\": ${ARGN}")
  set(${LIBRARY}/OBJECTS)
  foreach(s ${ARGN})
    add_subdirectory(${s})
    #message("${s}/OBJECTS: ${${s}/OBJECTS}")
    list(APPEND ${LIBRARY}/OBJECTS ${${s}/OBJECTS})
  endforeach()
  set(${LIBRARY}/OBJECTS ${${LIBRARY}/OBJECTS} PARENT_SCOPE)
  add_library(${LIBRARY} ${${LIBRARY}/OBJECTS})
  add_dependencies(${LIBRARY} External)
endmacro()

# Add MPQC object library
# add_mpqc_object_library(LIBRARY SOURCES)
# LIBRARY - name of the library
# SOURCES - sources to include in LIBRARY
# automatically adds dependencies on External target
# Example: add_mpqc_object_library(class class.cc)

macro(add_mpqc_object_library LIBRARY )
  #message("Object library \"${LIBRARY}\": ${ARGN}")
  add_library(${LIBRARY} OBJECT ${ARGN})
  add_dependencies(${LIBRARY} External)
  set(${LIBRARY}/OBJECTS $<TARGET_OBJECTS:${LIBRARY}> PARENT_SCOPE)
endmacro()

# Add MPQC object target
# add_mpqc_objects_target(TARGET SUBDIRS)
macro(add_mpqc_objects_target TARGET )
  #message("Objects target \"${TARGET}\": ${ARGN}")
  add_custom_target(${TARGET})
  foreach(s ${ARGN})
    add_subdirectory(${s})
    add_dependencies(${TARGET} ${s})
    #message("${s}/OBJECTS: ${${s}/OBJECTS}")
    list(APPEND ${TARGET}/OBJECTS ${${s}/OBJECTS})
  endforeach()
  add_dependencies(${TARGET} External)
  set(${TARGET}/OBJECTS ${${TARGET}/OBJECTS} PARENT_SCOPE)
endmacro()