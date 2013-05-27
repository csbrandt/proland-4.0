# Copy the header files to the distribution directory

cd sources
find . -type d -exec mkdir -p ../../output/include/{} \;
find . -name "*.h" -exec cp {} ../../output/include/{} \;

# Copy the examples to the distribution directory

cd ../examples
find . -type d -exec mkdir -p ../../output/examples/graph/{} \;
find . ! \( -name "*.cbp" -o -name "*.depend" -o -name "*.layout" -o -name "*.raw" -o -name "log.html" \) -exec cp {} ../../output/examples/graph/{} \;

# Generate the Doxygen documentation

cd ../doc
mkdir -p ../../output/doc
doxygen
