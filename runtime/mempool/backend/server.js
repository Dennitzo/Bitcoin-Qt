process.chdir(__dirname);
process.argv = [process.argv[0], require.resolve('./dist/index.js')];
require('./dist/index.js');
