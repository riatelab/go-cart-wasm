## Go-cart-wasm

`Go-cart-wasm` is a JS/WASM library for making flow-based cartograms (as described in "*Gastner, Seguy, and More (2018). Fast flow-based algorithm for creating density-equalizing map projections. Proceedings of the National Academy of Sciences USA, 115:E2156-E2164*"), purely in the browser.

This is a port of the [reference implementation](https://github.com/Flow-Based-Cartograms/go_cart) provided by the authors (however we needed to slightly modify some minor aspects of the code, and our modified version is located [here](https://github.com/Flow-Based-Cartograms/go_cart/tree/wasm-mod)).

### Usage

The library is available as a JS module, and can be used as follows:

```js
import initGoCart from 'go-cart-wasm';

initGoCart()
  .then((GoCart) => {
    // The GeoJSON containing the data to be transformed
    const data = {
      type: 'FeatureCollection',
      features: ...
    };
    
    // The name of the field that contains the data on which the cartogram will be based
    const fieldName = 'POP2021';

    // Call the function that creates the cartogram
    const dataCartogram = GoCart.makeCartogram(data, fieldName);
    
    console.log(dataCartogram); // The resulting GeoJSON
  });
```

Optionally, and depending on how you import the library, you may also need to pass a `config` object as argument to the `initGoCart` function, which can contain the `locateFile` property: if set, it will be used to locate the WASM file (which is needed by the library). For example:

```js
initGoCart = require("https://unpkg.com/go-cart-wasm@0.1.0/dist/go-cart.js");

GoCart = await initGoCart({
  locateFile: (path) => 'https://unpkg.com/go-cart-wasm@0.1.0/dist/cart.wasm',
});

// Use GoCart as in the previous example
// ...
```

Note that the data that is passed to the `makeCartogram` function must be a valid GeoJSON FeatureCollection, its features must be of type Polygon or MultiPolygon and the field name must be a valid property of its features.
Moreover, the data have to be in a projected reference coordinate system that maintains the areas of the polygons (e.g. Lambert-93 / EPSG:2154 for Metropolitan France, EPSG:3035 for the European Union, etc.). Performing the calculation in geographic coordinates (e.g. EPSG:4326) would give erroneous results.

Note also that by default the calculation is done in the main thread. The freedom is left to the user to import go-cart.js from a webworker and use it that way (nevertheless you will have to implement a basic dialogue between the main thread and the webworker).

### Installation for development

**Requirements:**

- The **Just** command runner (https://github.com/casey/just)
- Emscripten SDK (https://emscripten.org/docs/getting_started/downloads.html)
- Node.js (https://nodejs.org/en/download/)
- npm (https://www.npmjs.com/get-npm)

**Install and compile dependencies:**

1) Install Just, install Emscripten SDK, install node.js / npm.
2) Install node dependencies with `npm install`.
3) Activate the various environment variables of Emscripten SDK in the current terminal (`source ./emsdk_env.sh` in emsdk directory).
4) Build the C dependencies with `npm run build-deps`.

**Build the WASM/JS code:**

1) Run `npm run build` to build the WASM module and the JS wrapper. If you change stuff in the JS wrapper (in `src`) or in the C code of go_cart (in `go_cart`), you can resume from here.
   If you want to see debug information about the progress of the cartogram creation in the console (grid size used, number of iterations, max area error, correction factor, etc.) you can compile with the DEBUG flag with `npm run build-debug`.
2) Get the built files from the `dist` directory.

Note that this has only been tested on GNU/Linux and that these instructions may need to be modified to work on Mac OS X and Windows.

### License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
Underlying code by Gastner, Seguy, and More (2018) is licensed under a modified MIT License - see their [LICENSE](go_cart/LICENSE) file for details.
