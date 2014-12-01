set -o errexit

# echo Unpacking source...
# tar xvfz source.tar.gz
# echo

cd $ROOTPATHSH
cd $EWBLIBS

echo Unpacking dependencies...
echo  openssl...
tar --atime-preserve -xzvf $OPENSSL.tar.gz > /dev/null
echo  berkeleydb...
tar --atime-preserve -xzvf $BERKELEYDB.tar.gz > /dev/null
echo  boost...
tar --atime-preserve -xzvf $BOOST.tar.gz > /dev/null
echo  miniupnpc...
mkdir $MINIUPNPC
tar -C $MINIUPNPC --atime-preserve -xzvf $MINIUPNPC.tar.gz > /dev/null
mv $MINIUPNPC/$MINIUPNPC $MINIUPNPC/miniupnpc

echo

cd ..