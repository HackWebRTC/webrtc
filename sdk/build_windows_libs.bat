@echo off

if "%~1"=="" (
    echo "Please set valid target output path"
    exit /b 1
)

set "OUTPUT_PATH=%~1"

mkdir %OUTPUT_PATH%\libs\windows\x64
mkdir %OUTPUT_PATH%\libs\windows_linux\include
mkdir %OUTPUT_PATH%\libs\symbols\win_x64\

gn gen out\x64_clang_vs2022 --args="target_os=\"win\" target_cpu=\"x64\" is_clang=true use_custom_libcxx=true use_rtti=true is_debug=false treat_warnings_as_errors=false is_component_build=false enable_stripping=true rtc_include_tests=false rtc_libvpx_build_vp9=false rtc_use_h264=true rtc_include_internal_audio_device=true" --ide=vs2022 && ^
autoninja -C out\x64_clang_vs2022 win_pc_client && ^
copy /Y out\x64_clang_vs2022\win_pc_client.dll %OUTPUT_PATH%\libs\windows\x64\ && ^
copy /Y out\x64_clang_vs2022\win_pc_client.dll.lib %OUTPUT_PATH%\libs\windows\x64\ && ^
copy /Y sdk\desktop\lib_pc_client.h %OUTPUT_PATH%\libs\windows_linux\include\ && ^
copy /Y out\x64_clang_vs2022\win_pc_client.dll.pdb %OUTPUT_PATH%\libs\symbols\win_x64\
