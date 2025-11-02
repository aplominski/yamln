project('yamln', 'cpp',
    version : '0.1',
    default_options : [
        'cpp_std=c++17',
        'buildtype=release',
        'c_args=-O3 -flto -fno-rtti -fvisibility=hidden -march=native',
        'cpp_args=-O3 -flto -fexceptions -fvisibility=hidden -fno-rtti -march=native',
        'c_link_args=-flto',
        'cpp_link_args=-flto'
    ]
)

yamln_src = files(
    'src/yamln_serialize.cpp',
    'src/yamln_parser.cpp'
)

yamln_inc = include_directories('include')

yaln_lib = shared_library(
    'yamln',
    yamln_src,
    include_directories : yamln_inc,
    version : meson.project_version(),
    install : true,                        
    install_dir : get_option('libdir')     
)

install_headers(
    'include/yamln.h',              
    subdir : 'yamln'                       
)