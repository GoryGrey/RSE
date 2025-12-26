{
  "targets": [
    {
      "target_name": "betti_rdl",
      "sources": [ "betti_rdl.cpp" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "../src/cpp_kernel"
      ],
      "libraries": [
        "-L/home/gorygrey/Apps/RSE/build/shared/lib",
        "-lbetti_rdl_c",
        "-lstdc++",
        "-latomic",
        "-Wl,-rpath,/home/gorygrey/Apps/RSE/build/shared/lib"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
      "msvs_settings": {
        "VCCLCompilerTool": {
          "ExceptionHandling": 1,
          "AdditionalOptions": [ "/std:c++17" ]
        }
      },
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.15",
        "OTHER_CFLAGS": [ "-std=c++17" ]
      }
    }
  ]
}
