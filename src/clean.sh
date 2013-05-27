rm -rf build/*
rmdir build

rm -rf output/*
rmdir output

find . \( -name "*.depend" -o -name "*.layout" -o -name "log.html" -o -name "doxygen.log" -o -name "inscatter.raw" -o -name "irradiance.raw" -o -name "transmittance.raw" \) -exec rm {} \;
