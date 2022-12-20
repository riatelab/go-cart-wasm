set shell := ["bash", "-uc"]
set dotenv-load

FFTW_PACKAGE := "fftw-3.3.3"
CJSON_VERSION := "1.7.15"

default: build

clean-all: clean
    rm -rf cJSON-{{CJSON_VERSION}} {{FFTW_PACKAGE}}

clean:
    rm -rf build/
    rm -f fftw-3.3.3/.libs/*.*
    rm -f fftw-3.3.3/api/*.o

fftw3:
    if [[ ! -d {{FFTW_PACKAGE}} ]]; then \
        wget http://www.fftw.org/{{FFTW_PACKAGE}}.tar.gz; \
        tar -xzf {{FFTW_PACKAGE}}.tar.gz; \
        rm {{FFTW_PACKAGE}}.tar.gz; \
    fi
    cd {{FFTW_PACKAGE}} && \
    emconfigure ./configure --disable-fortran && \
    emmake make -j4

cjson:
    if [[ ! -d cJSON-{{CJSON_VERSION}} ]]; then \
        wget https://github.com/DaveGamble/cJSON/archive/refs/tags/v{{CJSON_VERSION}}.zip; \
        unzip v{{CJSON_VERSION}}.zip; \
        rm v{{CJSON_VERSION}}.zip; \
    fi
    cp cJSON-{{CJSON_VERSION}}/cJSON.h go_cart/cartogram_generator/
    cp cJSON-{{CJSON_VERSION}}/cJSON.c go_cart/cartogram_generator/

build: clean fftw3 cjson
    mkdir -p build
    cd go_cart/cartogram_generator && \
    emcc --bind -I../../{{FFTW_PACKAGE}}/api -L../../{{FFTW_PACKAGE}}/.libs -DUSE_FFTW -lfftw3 main.c cartogram.c ffb_integrate.c fill_with_density.c ps_figure.c read_map.c process_json.c cJSON.c \
        -o ../../build/cart.js \
        -O3 \
        -s FORCE_FILESYSTEM \
        -s EXPORT_ES6=1 \
        -s MODULARIZE=1 \
        -s EXPORTED_FUNCTIONS="['_doCartogram']" \
        -s EXPORTED_RUNTIME_METHODS="['ccall', 'FS']" \
        -s EXPORT_NAME="GoCart" \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s MAXIMUM_MEMORY=4096mb
