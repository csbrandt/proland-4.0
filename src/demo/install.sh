# Copy the demo to the distribution directory

find . -type d -exec mkdir -p ../output/demo/{} \;
find . ! \( -name "*.cbp" -o -name "*.depend" -o -name "*.layout" -o -name "log.html" -o -name "*.sh" -o -name "inscatter.raw" -o -name "irradiance.raw" -o -name "transmittance.raw" -o -name "README.TXT" \) -exec cp {} ../output/demo/{} \;
