{
  "variables": {
    "betti_rdl_shared_lib_dir%": "<!(node -e \"const path=require('path'); const dir=process.env.BETTI_RDL_SHARED_LIB_DIR || path.resolve(process.cwd(), '../build/shared/lib'); console.log(dir);\")"
  },
  "targets": [
    {
      "target_name": "betti_rdl",
      "sources": ["betti_rdl.cpp"],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "../src/cpp_kernel"
      ],
      "libraries": [
        "-L<(betti_rdl_shared_lib_dir)",
        "-lbetti_rdl_c",
        "-lstdc++",
        "-Wl,-rpath,<(betti_rdl_shared_lib_dir)"
      ],
      "conditions": [
        [
          "OS=='linux'",
          {
            "libraries": [
              "-latomic",
              "-Wl,-rpath,\\$$ORIGIN/../../../build/shared/lib"
            ]
          }
        ]
      ],
      "dependencies": ["<!(node -p \"require('node-addon-api').gyp\")"],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"],
      "msvs_settings": {
        "VCCLCompilerTool": {
          "ExceptionHandling": 1,
          "AdditionalOptions": ["/std:c++17"]
        }
      },
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.15",
        "OTHER_CFLAGS": ["-std=c++17"]
      }
    }
  ]
}
