import GoCartWasmModule from '../build/cart';

const prepareGeoJSONandCSV = (geojson, fieldName) => {
  if (!geojson || !fieldName || !geojson.features) {
    throw new Error('Invalid arguments');
  }
  const _geojson = JSON.parse(JSON.stringify(geojson));
  const values = [];
  let xmin = Infinity;
  let xmax = -Infinity;
  let ymin = Infinity;
  let ymax = -Infinity;

  const bboxFn = (point) => {
    xmin = Math.min(xmin, point[0]);
    xmax = Math.max(xmax, point[0]);
    ymin = Math.min(ymin, point[1]);
    ymax = Math.max(ymax, point[1]);
  };
  _geojson.features.forEach((feature, i) => {
    // eslint-disable-next-line no-param-reassign
    feature.properties.cartogram_id = `${i + 1}`;

    if (feature.geometry.type === 'Polygon') {
      feature.geometry.coordinates.forEach((ring) => {
        ring.forEach(bboxFn);
      });
    } else if (feature.geometry.type === 'MultiPolygon') {
      feature.geometry.coordinates.forEach((polygon) => {
        polygon.forEach((ring) => {
          ring.forEach(bboxFn);
        });
      });
    }

    // TODO: handle case where 'fieldName' is not present
    //       or is not a number...
    values.push({
      cartogram_id: i + 1,
      value: feature.properties[fieldName],
    });
  });
  _geojson.bbox = [xmin, ymin, xmax, ymax];
  const rawCsv = `Region Id, Region Data\n${values.map((v) => `${v.cartogram_id}, ${v.value}`).join('\n')}`;
  const rawGeoJSON = JSON.stringify(_geojson);
  return { rawGeoJSON, rawCsv };
};

const initGoCart = async (options = {}) => {
  const GoCartWasm = await GoCartWasmModule(options);

  GoCartWasm.makeCartogram = function makeCartogram(geojson, fieldName) {
    const { rawGeoJSON, rawCsv } = prepareGeoJSONandCSV(geojson, fieldName);
    const pathInputJsonFile = '/data/test.json';
    const pathInputCsvFile = '/data/test.csv';

    GoCartWasm.FS.mkdir('/data');
    GoCartWasm.FS.writeFile(pathInputJsonFile, rawGeoJSON);
    GoCartWasm.FS.writeFile(pathInputCsvFile, rawCsv);

    const retVal = GoCartWasm.ccall(
      'doCartogram',
      'number',
      ['string', 'string'],
      [pathInputJsonFile, pathInputCsvFile],
    );

    const data = GoCartWasm.FS.readFile('cartogram.json', { encoding: 'utf8' });

    GoCartWasm.FS.unlink(pathInputJsonFile);
    GoCartWasm.FS.unlink(pathInputCsvFile);
    GoCartWasm.FS.unlink('cartogram.json');

    return JSON.parse(data);
  };

  return GoCartWasm;
};

export default initGoCart;
