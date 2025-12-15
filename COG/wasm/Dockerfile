FROM emscripten/emsdk:latest

WORKDIR /src

# Pre-compile cache for faster builds
RUN emcc --clear-cache

CMD ["emcc", \
     "-O3", \
     "-flto", \
     "--bind", \
     "-I.", \
     "-o", "output/genesis.js", \
     "COG/genesis_cpp/Universe.cpp", \
     "COG/genesis_cpp/Bindings.cpp", \
     "-s", "ALLOW_MEMORY_GROWTH=1", \
     "-s", "MODULARIZE=1", \
     "-s", "EXPORT_NAME='createGenesisModule'", \
     "-s", "ENVIRONMENT='web'"]
