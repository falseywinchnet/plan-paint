# Pinned import-only dependencies. The application continues to export its native raster formats.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(AVIF_CODEC_DAV1D SYSTEM CACHE STRING "" FORCE)
foreach(codec AOM LIBGAV1 RAV1E SVT AVM)
  set(AVIF_CODEC_${codec} OFF CACHE STRING "" FORCE)
endforeach()
foreach(option AVIF_BUILD_APPS AVIF_BUILD_TESTS AVIF_BUILD_EXAMPLES AVIF_ENABLE_GTEST AVIF_LIBYUV AVIF_LIBSHARPYUV)
  set(${option} OFF CACHE STRING "" FORCE)
endforeach()
FetchContent_Declare(avif
  URL https://codeload.github.com/AOMediaCodec/libavif/tar.gz/bcfcd821dab042c83dbb83ef72121f59b3c00661
  URL_HASH SHA256=5f169f5e416786d7ab2d36fb533b118c34f48668b5ae455abe2c7701f494982b
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(avif)

find_program(RAINSTAR_CARGO cargo REQUIRED)
FetchContent_Declare(resvg
  URL https://codeload.github.com/linebender/resvg/tar.gz/898b377cb4f1b55f6f7b1dde4d0448277179812f
  URL_HASH SHA256=536805f37545c90981b818b45d5537d95668fa08696ca4f4226fca89845d8a74
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(resvg)
set(resvg_target "${CMAKE_CURRENT_BINARY_DIR}/rust-resvg")
set(resvg_environment "CARGO_PROFILE_RELEASE_LTO=true" "CARGO_PROFILE_RELEASE_CODEGEN_UNITS=1")
if(WIN32)
  set(resvg_library "${resvg_target}/release/resvg.lib")
  list(APPEND resvg_environment "RUSTFLAGS=-C target-feature=+crt-static")
else()
  set(resvg_library "${resvg_target}/release/libresvg.a")
endif()
add_custom_command(OUTPUT "${resvg_library}"
  COMMAND ${CMAKE_COMMAND} -E env ${resvg_environment} ${RAINSTAR_CARGO} build
    --manifest-path "${resvg_SOURCE_DIR}/crates/c-api/Cargo.toml" --release --locked
    --no-default-features --features text,system-fonts,raster-images --target-dir "${resvg_target}"
  DEPENDS "${resvg_SOURCE_DIR}/Cargo.lock" "${resvg_SOURCE_DIR}/crates/c-api/Cargo.toml"
  COMMENT "Building the pinned static SVG rasterizer" VERBATIM)
add_custom_target(rainstar_resvg_build DEPENDS "${resvg_library}")
add_library(rainstar_resvg STATIC IMPORTED GLOBAL)
set_target_properties(rainstar_resvg PROPERTIES IMPORTED_LOCATION "${resvg_library}"
  INTERFACE_INCLUDE_DIRECTORIES "${resvg_SOURCE_DIR}/crates/c-api")
add_dependencies(rainstar_resvg rainstar_resvg_build)
if(WIN32)
  set_property(TARGET rainstar_resvg PROPERTY INTERFACE_LINK_LIBRARIES "ntdll;userenv;ws2_32;bcrypt;advapi32")
elseif(UNIX AND NOT APPLE)
  set_property(TARGET rainstar_resvg PROPERTY INTERFACE_LINK_LIBRARIES "m;dl;pthread")
endif()
