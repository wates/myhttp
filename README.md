
overview
====

for use Route53 HTTP health check at mysql

require
====
g++ > 4.8 or VisualStudio > 2013
libmysql-client

how to build
====
git submodule init --update --recuresive
mkdir build
cd build
cmake ..

download mysql-connector-c-6.1.6-win32 if use windows
