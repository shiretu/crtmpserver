#!/bin/sh
ROOT_FOLDER=../..
RELEASE_NUMBER=`cat $ROOT_FOLDER/RELEASE_NUMBER`
DIST_NAME="crtmpserver-$RELEASE_NUMBER-$OS_ARCH-${OS_NAME}_${OS_VERSION}"

rm -rf /tmp/$DIST_NAME
mkdir -p /tmp/$DIST_NAME/media
mkdir -p /tmp/$DIST_NAME/configs
mkdir -p /tmp/$DIST_NAME/logs
mkdir -p /tmp/$DIST_NAME/ssl
cp crtmpserver/crtmpserver /tmp/$DIST_NAME/
cp tests/tests /tmp/$DIST_NAME/
strip -x crtmpserver/crtmpserver /tmp/$DIST_NAME/crtmpserver
strip -x /tmp/$DIST_NAME/tests
cp ../../media/README.txt /tmp/$DIST_NAME/media/
cp ../../configs/*.lua /tmp/$DIST_NAME/configs/
cp ../../LICENSE /tmp/$DIST_NAME/
cp ../../configs/*.sh /tmp/$DIST_NAME/
cp applications/appselector/server.* /tmp/$DIST_NAME/ssl/
(cd /tmp && tar czfv $DIST_NAME.tar.gz $DIST_NAME)
mv /tmp/$DIST_NAME.tar.gz ./


