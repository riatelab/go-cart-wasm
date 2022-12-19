// eslint-disable-next-line import/no-named-as-default, import/no-named-as-default-member
import GoCartWasmModule from '../build/cart';

// eslint-disable-next-line no-restricted-globals
const isNumber = (value) => value != null && value !== '' && isFinite(value);

/**
 * Prepare GeoJSON and CSV data for GoCart.
 *
 * @param geojson
 * @param fieldName
 * @return {{rawGeoJSON: string, rawCsv: string}}
 */
const prepareGeoJSONandCSV = (geojson, fieldName) => {
  const newGeojson = JSON.parse(JSON.stringify(geojson));
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
  newGeojson.features.forEach((feature, i) => {
    const cartogramId = i + 1;
    // eslint-disable-next-line no-param-reassign
    feature.properties.cartogram_id = `${cartogramId}`;

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

    // Replace inexistent values and not number values by 0
    const valueIsNumber = isNumber(feature.properties[fieldName]);
    const value = valueIsNumber ? feature.properties[fieldName] : 0;
    values.push({ cartogramId, value });
  });

  newGeojson.bbox = [xmin, ymin, xmax, ymax];
  const rawCsv = `Region Id, Region Data\n${values.map((v) => `${v.cartogramId}, ${v.value}`).join('\n')}`;
  const rawGeoJSON = JSON.stringify(newGeojson);
  return { rawGeoJSON, rawCsv };
};

const initGoCart = async (options = {}) => {
  const GoCartWasm = await GoCartWasmModule(options);

  /**
   * Make a cartogram according to Gastner, Seguy and More (2018) algorithm.
   * This function expects a GeoJSON object (FeatureCollection) and a field name.
   *
   * @param geojson {Object} - The FeatureCollection to be handled.
   * @param fieldName {String} - The name of the field to be used as data.
   * @return {Object}
   */
  GoCartWasm.makeCartogram = function makeCartogram(geojson, fieldName) {
    // Check the arguments
    if (
      !geojson
      || !fieldName
      || typeof geojson !== 'object'
      || !Object.prototype.hasOwnProperty.call(geojson, 'features')
    ) {
      throw new Error('Invalid arguments : first argument must be a GeoJSON FeatureCollection and second argument must be a field name');
    }
    // Prepare the data
    const { rawGeoJSON, rawCsv } = prepareGeoJSONandCSV(geojson, fieldName);

    // Save the data in GoCart memory / file system
    const pathInputJsonFile = '/data/test.json';
    const pathInputCsvFile = '/data/test.csv';

    GoCartWasm.FS.mkdir('/data');
    GoCartWasm.FS.writeFile(pathInputJsonFile, rawGeoJSON);
    GoCartWasm.FS.writeFile(pathInputCsvFile, rawCsv);

    const cleanUp = () => {
      GoCartWasm.FS.unlink(pathInputJsonFile);
      GoCartWasm.FS.unlink(pathInputCsvFile);
      GoCartWasm.FS.unlink('cartogram.json');
    };

    // Actually run the algorithm
    const retVal = GoCartWasm.ccall(
      'doCartogram',
      'number',
      ['string', 'string'],
      [pathInputJsonFile, pathInputCsvFile],
    );

    if (retVal !== 0) {
      cleanUp();
      throw new Error('Error while running the cartogram algorithm');
    }

    // Read the result
    const data = GoCartWasm.FS.readFile('cartogram.json', { encoding: 'utf8' });

    // Read the log file about area errors
    const areaErrors = GoCartWasm.FS.readFile('area_error.dat', { encoding: 'utf8' });
    const t = {};
    areaErrors.split('\n').forEach((line) => {
      const id = line.substring(7, line.indexOf(': '));
      const errorValue = line.substring(line.indexOf('relative error = ') + 'relative error = '.length);
      t[id] = +errorValue;
    });

    // Store the area error in each feature properties
    const result = JSON.parse(data);
    result.features.forEach((feature) => {
      const id = feature.properties.cartogram_id;
      // eslint-disable-next-line no-param-reassign
      feature.properties.area_error = t[id];
    });

    // Clean the memory / file system
    cleanUp();

    // Return the result
    return result;
  };

  return GoCartWasm;
};

export default initGoCart;
