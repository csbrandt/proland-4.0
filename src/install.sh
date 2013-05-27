cd core
install.sh

cd ../terrain
install.sh

cd ../atmo
install.sh

cd ../ocean
install.sh

cd ../forest
install.sh

cd ../graph
install.sh

cd ../river
install.sh

cd ../edit
install.sh

cd ../demo
install.sh

cd ..
echo '<html>
<body>
<h1>Proland documentation</h1>
<ul>
<li><a href="core/html/index.html">core</a></li>
<li><a href="terrain/html/index.html">terrain</a></li>
<li><a href="atmo/html/index.html">atmo</a></li>
<li><a href="ocean/html/index.html">ocean</a></li>
<li><a href="forest/html/index.html">forest</a></li>
<li><a href="graph/html/index.html">graph</a></li>
<li><a href="river/html/index.html">river</a></li>
<li><a href="edit/html/index.html">edit</a></li>
</ul>
<h1>Proland examples</h1>
<ul>
<li><a href="core/html/page-examples.html">core</a></li>
<li><a href="terrain/html/page-examples.html">terrain</a></li>
<li><a href="atmo/html/page-examples.html">atmo</a></li>
<li><a href="ocean/html/page-examples.html">ocean</a></li>
<li><a href="forest/html/page-examples.html">forest</a></li>
<li><a href="graph/html/page-examples.html">graph</a></li>
<li><a href="river/html/page-examples.html">river</a></li>
<li><a href="edit/html/page-examples.html">edit</a></li>
</ul>
</body>
</html>' > output/doc/index.html

cp README-DISTRIB.TXT output/README.TXT
