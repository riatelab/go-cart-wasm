import { nodeResolve } from '@rollup/plugin-node-resolve';
import commonjs from '@rollup/plugin-commonjs';
import { babel } from '@rollup/plugin-babel';
import virtual from '@rollup/plugin-virtual';
import copy from 'rollup-plugin-copy';

const nodeLibs = {
  fs: `export default {};`,
  path: `export default {};`,
  string_decoder: `export default {};`,
  buffer: `export default {};`,
  crypto: `export default {};`,
  stream: `export default {};`
};

export default [
  {
    plugins: [
      virtual(nodeLibs),
      nodeResolve(),
      commonjs({ transformMixedEsModules: true }),
      babel({ babelHelpers: 'bundled' }),
      copy({
        targets: [
          { src: 'build/cart.wasm', dest: 'dist/' },
        ]
      })
    ],
    input: 'src/index.js',
    output: [
      {
        file: 'dist/go-cart.cjs',
        format: 'cjs',
        exports: 'auto',
      },
      {
        file: 'dist/go-cart.mjs',
        format: 'esm',
      },
      {
        file: 'dist/go-cart.js',
        format: 'umd',
        name: 'initGoCart',
      },
    ]
  },
];
