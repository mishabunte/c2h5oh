Build
=====

cmake 3.2 is required for build 
postgresql 9.5 is required for testing

git submodule update --init
make debug
make test

Install
================

make deb
./deploy/c2h5oh-nginx_0.0.0_amd64.sh
vi /etc/c2h5oh/c2h5oh_nginx.conf
service c2h5oh_nginx restart
