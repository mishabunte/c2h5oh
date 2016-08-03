#!/bin/bash

#
# echo check args ...
#

[[ -n $1 ]] || { echo "$0 <root dir> <name> <version>" && exit 1; }
[[ -n $2 ]] || { echo "$0 <root dir> <name> <version>" && exit 1; }
[[ -n $3 ]] || { echo "$0 <root dir> <name> <version>" && exit 1; }

#
# echo init variables  ...
#
DIR=$1
NAME=$2
VERSION=$3
ARCH=$(dpkg --print-architecture)

#
# echo check project dir ...
#
[ -d $DIR ] || { echo "$DIR not exists" && exit 1; }

#
# echo make debian source  dir ...
#
DIR_BUILD=$DIR/.build
DIR_DEB=$DIR_BUILD/deb_c2h5oh
DIR_DEBIAN=$DIR_DEB/DEBIAN
DIR_DEPLPOY=$DIR/deploy

DEPLOYER=$DIR_BUILD/deploy_c2h5oh.sh
INSTALLER=$DIR_DEPLPOY/${NAME}_${VERSION}_${ARCH}.sh

rm -rfd $DIR_DEB
mkdir -p $DIR_DEBIAN || { echo "$DIR_DEBIAN can't create" && exit 1; }

#
# set package dir names
#
C2H5OH_BIN=/sbin/c2h5oh_nginx
C2H5OH_ETC=/etc/c2h5oh
C2H5OH_LOG=/var/log/c2h5oh

#
# echo set debian control names
#
DEB_NAME=${NAME}_${VERSION}_${ARCH}.deb
DEB_CONTROL=$DIR_DEBIAN/control
DEB_CONFFILES=$DIR_DEBIAN/conffiles
DEB_POSTINST=$DIR_DEBIAN/postinst
DEB_PRERM=$DIR_DEBIAN/prerm
DEB_POSTRM=$DIR_DEBIAN/postrm

#
# make debian/control
#
echo "Package: ${NAME}
Version: $VERSION
Architecture: $ARCH
Depends: libc6, zlibc, libpq5, libpcre3
Maintainer: misha@genosse.org
Description: nginx with c2h5oh module, a lightweight web framework without drugs
" > $DEB_CONTROL  || exit 1

#
# make debian/conffiles
#
echo "/etc/logrotate.d/c2h5oh_nginx
/etc/c2h5oh/c2h5oh_nginx.conf
/etc/init.d/c2h5oh_nginx" > $DEB_CONFFILES || exit 1


#
# make debian/postinst
#
echo "#!/bin/sh
id -u www-data &> /dev/null || adduser --system --group --quiet --no-create-home www-data

chown -fR www-data:www-data /var/lib/c2h5oh
chown -fR www-data:www-data /var/log/c2h5oh

update-rc.d c2h5oh_nginx defaults
invoke-rc.d c2h5oh_nginx restart
exit 0
"  > $DEB_POSTINST  || exit 1
chmod +x $DEB_POSTINST

#
# make debian/prerm
#
echo "#!/bin/sh
[ -f /etc/init.d/README ] && [ -f /etc/init.d/c2h5oh_nginx ] && invoke-rc.d c2h5oh_nginx stop
exit 0
"  > $DEB_PRERM  || exit 1
chmod +x $DEB_PRERM

#
# make debian/postrm
#
echo "#!/bin/sh
[ -f /etc/init.d/c2h5oh_nginx ] && update-rc.d c2h5oh_nginx remove
[ -f /etc/init.d/c2h5oh_nginx ] && unlink /etc/init.d/c2h5oh_nginx
exit 0
"  > $DEB_POSTRM  || exit 1
chmod +x $DEB_POSTRM

#
# make deploy script
#

echo "#!/bin/bash

echo ... create deb dirtree ...

mkdir -p $DIR_DEB/etc/logrotate.d       || exit 1
mkdir -p $DIR_DEB/etc/init.d            || exit 1
mkdir -p $DIR_DEB/sbin                  || exit 1
mkdir -p $DIR_DEB/var/lib/c2h5oh/uploads|| exit 1
mkdir -p $DIR_DEB/var/lib/c2h5oh/nginx_body|| exit 1
mkdir -p $DIR_DEB/var/lib/c2h5oh/nginx_fastcgi|| exit 1
mkdir -p $DIR_DEB/var/lib/c2h5oh/nginx_scgi|| exit 1
mkdir -p $DIR_DEB/var/lib/c2h5oh/nginx_uwsgi|| exit 1
mkdir -p $DIR_DEB/$C2H5OH_ETC           || exit 1
mkdir -p $DIR_DEB/$C2H5OH_LOG           || exit 1

echo ... copy files ...

cp $DIR/bin/c2h5oh_nginx               $DIR_DEB/$C2H5OH_BIN       || exit 1

cp $DIR/config/init.c2h5oh_nginx       $DIR_DEB/etc/init.d/c2h5oh_nginx || exit 1

cp $DIR/config/logrotate.c2h5oh_nginx  $DIR_DEB/etc/logrotate.d/c2h5oh_nginx || exit 1

cp $DIR/config/c2h5oh_nginx.conf       $DIR_DEB/$C2H5OH_ETC       || exit 1

echo ... compile deb  ...

mkdir -p $DIR/deploy
rm -f $DIR/deploy/${DEB_NAME}
dpkg -b $DIR_DEB $DIR/deploy/${DEB_NAME}

echo ... make installer ...

rm -f $INSTALLER

echo \"#!/bin/bash
dir_base=\\\`pwd\\\`
dir_deb=\\\$(cd \\\$(dirname \\\$0); pwd)
cd \\\$dir_base
dpkg -i  \\\$dir_deb/${DEB_NAME}
apt-get -f install
\" > $INSTALLER || exit 1
chmod +x $INSTALLER

" > $DEPLOYER || exit 1

chmod +x $DEPLOYER

exit 0

