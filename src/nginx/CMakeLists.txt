project(nginx_c2h5oh)
cmake_minimum_required(VERSION 3.2)

find_package(ZLIB REQUIRED)

set(NX_VER    nginx-1.8.0)
set(NX_URL    http://nginx.org/download/${NX_VER}.tar.gz)
set(NX_DBUILD ${CMAKE_SOURCE_DIR}/.build)
set(NX_TAR    ${CMAKE_SOURCE_DIR}/deps/third-party/${NX_VER}.tar.gz)
set(NX_C2H5OH_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/nginx_c2h5oh/config)

set(NX_PREFIX     /etc/c2h5oh_nginx)
set(NX_DRUN       /var/run)
set(NX_DLIB       /var/lib/c2h5oh)
set(NX_PID        ${NX_DRUN}/c2h5oh_nginx.pid)
set(NX_LOG        /var/log/c2h5oh)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(NX_CONFIGURE  "--with-debug")
endif()


include(ExternalProject)

ExternalProject_Add(jsmn                                                        
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/deps/jsmn                                   
  HG_REPOSITORY http://bitbucket.org/zserge/jsmn
  BUILD_IN_SOURCE 1                                                             
  UPDATE_COMMAND ""                                                             
  CONFIGURE_COMMAND ""                                                          
  BUILD_COMMAND make -e CFLAGS=-DJSMN_PARENT_LINKS
  INSTALL_COMMAND ""                                                            
)                      

ExternalProject_Add(nginx
  DEPENDS jsmn 
  PREFIX ${NX_DBUILD}/nginx_trash
#--Download step--------------
  URL ${NX_TAR}
#--Configure step-------------
  SOURCE_DIR ${NX_DBUILD}/nginx        
  CONFIGURE_COMMAND ./configure
  ${NX_CONFIGURE}
  --build=c2h5oh/${C2H5OH_VERSION}
  --prefix=${NX_PREFIX}
  --conf-path=${NX_PREFIX}/c2h5oh_nginx.conf
  --sbin-path=/sbin/c2h5oh_nginx
  --with-cc-opt=-Wno-deprecated-declarations  
  --error-log-path=${NX_LOG}/error.log
  #--error-log-path=error.log
  --http-log-path=${NX_LOG}/access.log
  --http-client-body-temp-path=${NX_DLIB}/nginx_body
  --http-fastcgi-temp-path=${NX_DLIB}/nginx_fastcgi
  --http-proxy-temp-path=${NX_DLIB}/nginx_proxy
  --http-scgi-temp-path=${NX_DLIB}/nginx_scgi
  --http-uwsgi-temp-path=${NX_DLIB}/nginx_uwsgi
  --lock-path=/var/lock/c2h5oh_nginx.lock
  --pid-path=${NX_PID}
  --add-module=${CMAKE_CURRENT_SOURCE_DIR}/nginx_c2h5oh
  --with-http_stub_status_module
  --with-http_auth_request_module
  --with-ipv6  
  --with-http_ssl_module
  #--with-file-aio
  --without-http_userid_module
  --without-http_auth_basic_module
  --without-http_autoindex_module
  --without-http_geo_module
  --without-http_referer_module
  --without-http_fastcgi_module
  --without-http_uwsgi_module
  --without-http_scgi_module
  --without-http_memcached_module
  --without-http_browser_module
  --without-mail_pop3_module
  --without-mail_imap_module
  --without-mail_smtp_module
#--Build step-----------------
  BUILD_COMMAND "make"
  BUILD_ALWAYS 1
  BUILD_IN_SOURCE 1
#--Install step---------------
  INSTALL_COMMAND ""  
)

ExternalProject_Add_Step(nginx nginx_download
 COMMAND test -e ${NX_TAR} || curl ${NX_URL} --create-dirs -o ${NX_TAR}
 COMMAND cp ${NX_C2H5OH_CONFIG}.${CMAKE_BUILD_TYPE} ${NX_C2H5OH_CONFIG}
 DEPENDERS build
 WORKING_DIRECTORY ${NX_DBUILD}
 DEPENDS c2h5oh ${NX_C2H5OH_CONFIG}.${CMAKE_BUILD_TYPE}
)

#enable_testing()
#add_test(NAME test_nginx COMMAND ${CMAKE_SOURCE_DIR}/tests/nginx/test_nginx.pl)
