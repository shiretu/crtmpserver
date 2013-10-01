#!/bin/sh

set -e

echo -n "Initialize script variables... "
ROOT_DIR=`realpath ../../..`
OUTPUT_DIR=`mktemp -d -t gigi`
PORT_NAME=crtmpserver
PORT_SECTION=net
PORT_SVN_VERSION=`svnversion -n $ROOT_DIR`
PORT_VERSION="0.$PORT_SVN_VERSION"
PORT_VERSIONED_NAME=$PORT_NAME-$PORT_VERSION
PORT_TEMPLATE=`realpath port_template`
echo "Done"

echo "ROOT_DIR:            $ROOT_DIR"
echo "OUTPUT_DIR:          $OUTPUT_DIR"
echo "PORT_NAME:           $PORT_NAME"
echo "PORT_SECTION:        $PORT_SECTION"
echo "PORT_SVN_VERSION:    $PORT_SVN_VERSION"
echo "PORT_VERSION:        $PORT_VERSION"
echo "PORT_VERSIONED_NAME: $PORT_VERSIONED_NAME"
echo "PORT_TEMPLATE:       $PORT_TEMPLATE"
#exit 1

#create the crtmpserver-0.xxxx folder
echo -n "Create the $PORT_VERSIONED_NAME folder structure... "
mkdir -p $OUTPUT_DIR/$PORT_VERSIONED_NAME/builders/
cp -R $ROOT_DIR/CODE_NAME $OUTPUT_DIR/$PORT_VERSIONED_NAME/
cp -R $ROOT_DIR/RELEASE_NUMBER $OUTPUT_DIR/$PORT_VERSIONED_NAME/
cp -R $ROOT_DIR/cleanup.sh $OUTPUT_DIR/$PORT_VERSIONED_NAME/
cp -R $ROOT_DIR/sources $OUTPUT_DIR/$PORT_VERSIONED_NAME/
cp -R $ROOT_DIR/builders/cmake $OUTPUT_DIR/$PORT_VERSIONED_NAME/builders/
cp -R $ROOT_DIR/3rdparty $OUTPUT_DIR/$PORT_VERSIONED_NAME/
cp -R $ROOT_DIR/man $OUTPUT_DIR/$PORT_VERSIONED_NAME/
cp $ROOT_DIR/LICENSE $OUTPUT_DIR/$PORT_VERSIONED_NAME/
cp $ROOT_DIR/configs/flvplayback.lua $OUTPUT_DIR/$PORT_VERSIONED_NAME/builders/cmake/crtmpserver/crtmpserver.lua
cp $ROOT_DIR/configs/all.debug.lua $OUTPUT_DIR/$PORT_VERSIONED_NAME/builders/cmake/crtmpserver/all.debug.lua
find $OUTPUT_DIR/$PORT_VERSIONED_NAME -type d -name "\.svn" -exec rm -rf {} +
mkdir -p $OUTPUT_DIR/$PORT_VERSIONED_NAME/builders/cmake/freebsd
cp $PORT_TEMPLATE/freebsd_CMakeLists.txt $OUTPUT_DIR/$PORT_VERSIONED_NAME/builders/cmake/freebsd/CMakeLists.txt
cp $PORT_TEMPLATE/fixConfFile.sh $OUTPUT_DIR/$PORT_VERSIONED_NAME/
cp $ROOT_DIR/man/* $OUTPUT_DIR/$PORT_VERSIONED_NAME/
echo "ADD_SUBDIRECTORY(freebsd)" >>$OUTPUT_DIR/$PORT_VERSIONED_NAME/builders/cmake/CMakeLists.txt
cat $OUTPUT_DIR/$PORT_VERSIONED_NAME/builders/cmake/CMakeLists.txt|sed "s/^.* svnversion -n.*$/SET(TEMP_VAL \"$PORT_SVN_VERSION (ports dist)\")/" >$OUTPUT_DIR/CMakeLists.txt
mv $OUTPUT_DIR/CMakeLists.txt $OUTPUT_DIR/$PORT_VERSIONED_NAME/builders/cmake/CMakeLists.txt
echo "Done"

#build the project
echo -n "Build the project ..."
cd "$OUTPUT_DIR/$PORT_VERSIONED_NAME/builders/cmake"
sh cleanup.sh
cmake -DCRTMPSERVER_INSTALL_PREFIX=$OUTPUT_DIR/tmp .
make -j32
make install
sh cleanup.sh
echo "Done"

#create pkg-plist
echo -n "Create the pkg-plist... "
cd $OUTPUT_DIR/tmp;
cp $PORT_TEMPLATE/conf.pkg-plist $OUTPUT_DIR/pkg-plist
find . -type f|sed "s/^\.\/\(.*\)$/\1/"|grep -v "\.sample"|grep -v "/man" >> $OUTPUT_DIR/pkg-plist
find . -d -type d -exec echo @dirrmtry {} \; |sed "s/^@\(.*\) \.\/\(.*\)$/@\1 \2/"|grep -v "\.$"|grep -v "lib$"|grep -v "sbin$"|grep -v "etc$"|grep -v " man" >> $OUTPUT_DIR/pkg-plist
echo "@dirrmtry var/log/crtmpserver" >>$OUTPUT_DIR/pkg-plist
echo "@dirrmtry var/crtmpserver/media" >>$OUTPUT_DIR/pkg-plist
echo "@dirrmtry var/crtmpserver" >>$OUTPUT_DIR/pkg-plist
rm -rf $OUTPUT_DIR/tmp
echo "Done"

#zip and upload
echo -n "Create $PORT_VERSIONED_NAME.tar.gz and upload it... "
cd $OUTPUT_DIR
tar -czf $PORT_VERSIONED_NAME.tar.gz $PORT_VERSIONED_NAME/
scp $OUTPUT_DIR/$PORT_VERSIONED_NAME.tar.gz $1
sudo cp $OUTPUT_DIR/$PORT_VERSIONED_NAME.tar.gz /usr/ports/distfiles/
echo "Done"

#Create the Makefile
echo -n "Copy the FreeBSD port files and apply the substitutions... "
cat $PORT_TEMPLATE/Makefile \
	|sed "s/##_PORTNAME_##/$PORT_NAME/" \
	|sed "s/##_PORTVERSION_##/$PORT_VERSION/" \
	|sed "s/##_PORTCATEGORIES_##/$PORT_SECTION/" \
	> $OUTPUT_DIR/Makefile
cp $PORT_TEMPLATE/pkg-descr $OUTPUT_DIR/
cd $OUTPUT_DIR
sha256 $PORT_VERSIONED_NAME.tar.gz >distinfo
echo "SIZE ($PORT_VERSIONED_NAME.tar.gz) = `ls -ALln $OUTPUT_DIR/$PORT_VERSIONED_NAME.tar.gz | awk '{print $5}'`" >> $OUTPUT_DIR/distinfo
mkdir -p $OUTPUT_DIR/files
cp $PORT_TEMPLATE/rc.d.crtmpserver $OUTPUT_DIR/files/crtmpserver.in
echo "Done"

#remove the zip archive
echo -n "Final cleanup... "
rm -rf $OUTPUT_DIR/$PORT_VERSIONED_NAME*
echo "Done"

echo ""
echo "Package completed"
echo "You should do:"
echo "cp -R `realpath $OUTPUT_DIR`/* /usr/ports/$PORT_SECTION/crtmpserver/; cd /usr/ports/$PORT_SECTION/crtmpserver; "
