
# All executables are somewhat optional ; testbench is the one which is
# used for tests.

# Would compile, but implies a dependency on gmp-ecm, or so it seems.
# add_executable(find_primes_hit find_primes_hit.c)
# target_link_libraries(find_primes_hit utils m)


# macro(variant_facul base typewidth)
# string(TOUPPER ${typewidth} TW)
#     SET(src ${CMAKE_CURRENT_SOURCE_DIR}/${base}.c)
#     SET(dst ${CMAKE_CURRENT_BINARY_DIR}/${base}_${typewidth}.c)
# add_custom_command(OUTPUT  ${dst}
#     COMMAND ln -s ${src} ${dst}
#     DEPENDS  ${src})
# set_source_files_properties(${dst} PROPERTIES COMPILE_FLAGS -DMODREDC${TW})
# endmacro()
# 
# variant_facul(pm1 15ul)
# variant_facul(pp1 15ul)
# variant_facul(ecm 15ul)
# variant_facul(facul_doit 15ul)
# variant_facul(pm1 ul)
# variant_facul(pp1 ul)
# variant_facul(ecm ul)
# variant_facul(facul_doit ul)

# Useful for debugging: disables optimization for facul() and facul_doit_*()
# which makes tracing program flow easier, but does not slow down the
# arithmetic in pm1_*(), pp1_*(), ecm_*()
# set_property(SOURCE facul.c PROPERTY COMPILE_FLAGS "-O0 -g")
# set_property(SOURCE facul_doit_ul.c PROPERTY COMPILE_FLAGS "-O0 -g")
# set_property(SOURCE facul_doit_15ul.c PROPERTY COMPILE_FLAGS "-O0 -g")
# set_property(SOURCE facul_doit_2ul2.c PROPERTY COMPILE_FLAGS "-O0 -g")
# set_property(SOURCE facul_doit_mpz.c PROPERTY COMPILE_FLAGS "-O0 -g")

# We now have wrappers, so it's very easy from the cmake perspective.
add_library(facul
        ecm_ul.c
        pm1_ul.c
        pp1_ul.c
        facul_doit_ul.c
        ecm_15ul.c
        pm1_15ul.c
        pp1_15ul.c
        facul_doit_15ul.c
        ecm_2ul2.c
        pm1_2ul2.c
        pp1_2ul2.c
        facul_doit_2ul2.c
        ecm_mpz.c
        pm1_mpz.c
        pp1_mpz.c
        facul_doit_mpz.c
        stage2.c bytecoder.c prac_bc.c addchain_bc.c
	makeplan.c facul.cpp
        mpqs_doit.c mpqs_ul.c mpqs_15ul.c mpqs_2ul2.c mpqs_mpz.c
	batch.c)

set_target_properties(facul PROPERTIES COMPILE_FLAGS "${OpenMP_C_FLAGS}")

if(MINGW)
target_link_libraries(facul gnurx)
endif()

add_executable(testbench testbench.c)

target_link_libraries(testbench facul utils m)

include(CMakeLists-nodist.txt OPTIONAL)
