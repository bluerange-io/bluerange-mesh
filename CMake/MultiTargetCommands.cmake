# This file is a list of CMake functions that already exist in the CMake standard
# We are now defining a set of functions to apply these on our list of selected targets

function(target_compile_options_multi TARGETS op)
  foreach(target ${TARGETS})
    target_compile_options(${target} PRIVATE ${op})
  endforeach(target)
endfunction()

function(target_compile_options_multi_lang TARGETS lang op)
  foreach(target ${TARGETS})
    target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:${lang}>:${op}>)
  endforeach(target)
endfunction()

function(target_sources_multi TARGETS src)
  foreach(target ${TARGETS})
    target_sources(${target} PRIVATE ${src})
  endforeach(target)
endfunction()

function(target_include_directories_multi TARGETS path)
  foreach(target ${TARGETS})
    target_include_directories(${target} PRIVATE ${path})
  endforeach(target)
endfunction()

function(target_include_directories_system_multi TARGETS path)
  foreach(target ${TARGETS})
    target_include_directories(${target} SYSTEM PRIVATE ${path})
  endforeach(target)
endfunction()

function(target_compile_definitions_multi TARGETS def)
  foreach(target ${TARGETS})
    target_compile_definitions(${target} PRIVATE ${def})
  endforeach(target)
endfunction()

function(set_property_multi TARGETS key value)
  foreach(target ${TARGETS})
    set_property(TARGET ${target} PROPERTY ${key} ${value})
  endforeach(target)
endfunction()

function(target_link_options_multi TARGETS op)
  foreach(target ${TARGETS})
    target_link_options(${target} PRIVATE ${op})
  endforeach(target)
endfunction()