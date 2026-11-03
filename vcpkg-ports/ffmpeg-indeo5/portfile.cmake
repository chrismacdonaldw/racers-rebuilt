vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ffmpeg/ffmpeg
    REF "n${VERSION}"
    SHA512 21bf3fbcdfd2f41ea6edeab40433fecfe362b09ecaa177a652471b54f9357011b4f74dab18245ab1c26f1a473b94205490baecb42289afd2effd603cd0b22b59
    HEAD_REF master
    PATCHES
        0046-fix-msvc-detection.patch
        0047-fix-msvc-utf8.patch
        0051-fix-msvc-undef-flags.patch
)

find_program(compiler cl.exe REQUIRED)
get_filename_component(compiler_dir "${compiler}" DIRECTORY)

vcpkg_acquire_msys(MSYS_ROOT PACKAGES make diffutils)
vcpkg_add_to_path(PREPEND "${MSYS_ROOT}/usr/bin")
vcpkg_add_to_path(PREPEND "${compiler_dir}")

set(crt "-MT")
if(VCPKG_CRT_LINKAGE STREQUAL "dynamic")
    set(crt "-MD")
endif()

set(options
    --toolchain=msvc
    --arch=x86_64
    --target-os=win32
    --enable-static
    --disable-shared
    --disable-autodetect
    --disable-everything
    --disable-programs
    --disable-doc
    --disable-avdevice
    --disable-avformat
    --disable-avfilter
    --disable-swscale
    --disable-swresample
    --disable-network
    --disable-x86asm
    --enable-w32threads
    --enable-decoder=indeo5
    --extra-cflags=-DHAVE_UNISTD_H=0
)

function(build_ffmpeg config prefix crt_flag)
    set(build_dir "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-${config}")
    file(REMOVE_RECURSE "${build_dir}")
    file(MAKE_DIRECTORY "${build_dir}")
    vcpkg_execute_required_process(
        COMMAND "${MSYS_ROOT}/usr/bin/bash.exe" --noprofile --norc -c
            "\"$0\" \"$@\" && make -j${VCPKG_CONCURRENCY} && make install"
            "${SOURCE_PATH}/configure" "--prefix=${prefix}" ${options} "--extra-cflags=${crt_flag}"
        WORKING_DIRECTORY "${build_dir}"
        LOGNAME "build-${TARGET_TRIPLET}-${config}"
        SAVE_LOG_FILES ffbuild/config.log
    )
    file(REMOVE_RECURSE "${prefix}/lib/pkgconfig" "${prefix}/share")
endfunction()

if(NOT VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "release")
    build_ffmpeg(rel "${CURRENT_PACKAGES_DIR}" "${crt}")
endif()
if(NOT VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "debug")
    build_ffmpeg(dbg "${CURRENT_PACKAGES_DIR}/debug" "${crt}d")
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
endif()

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/ffmpeg-indeo5-config.cmake"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING.LGPLv2.1" "${SOURCE_PATH}/LICENSE.md")
