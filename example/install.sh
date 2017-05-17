#!/bin/bash
echo "Install required packages"
#uncomment next line to install cmake 3+ on Ubuntu 14.04 
#sudo apt-add-repository ppa:george-edison55/cmake-3.x -y
sudo apt-add-repository ppa:myriadrf/drivers -y
sudo apt-get update

sudo apt-get --yes --force-yes install build-essential git cmake python-mako libsqlite3-dev libboost-all-dev libmnl-dev libssl-dev libpcap-dev libi2c-dev libusb-1.0-0-dev libsoapysdr-dev autoconf libtool libpcsclite-dev libortp-dev libdbi-dev libdbd-sqlite3

echo "Install LimeSuite"
rm -rf LimeSuite
git clone https://github.com/myriadrf/LimeSuite
cd LimeSuite/build/
#git checkout 990ebebe5e9a2e8d91962e9a223ab59a221be527
cmake ..
make -j8
sudo make install
sudo ldconfig
cd ../../

echo "Install UHD drivers"
rm -rf uhd
git clone https://github.com/EttusResearch/uhd
cd uhd/host/
git checkout maint
#git checkout 6f87cca4e8d98b5544a89d25d9eddce5ba85d393
mkdir build
cd build
cmake ..
make -j8
sudo make install
sudo ldconfig
cd ../../../

echo "Install SoapyUHD"
rm -rf SoapyUHD
git clone https://github.com/pothosware/SoapyUHD
cd SoapyUHD
#git checkout efeb46b5cc39ddf3a78192410ae23a47ce281bb6
mkdir build
cd build
cmake ..
make -j8
sudo make install
sudo ldconfig
cd ../../

echo "Install OsmoBTS"
rm -rf osmo-combo
git clone https://github.com/fairwaves/osmo-combo
cd osmo-combo/
#git checkout 66af97ce0760c0c015b2bc5c8c3290345571b122
git submodule init
git submodule update
./apply_patches.sh static-build
sudo make -j8
sudo ldconfig
cd ../

echo "Install osmo-trx"
git clone https://github.com/myriadrf/osmo-trx
cd osmo-trx/
git checkout myriadrf/limesdr
#git checkout 6e7de0e9fff9cfac525f601f2a22e325c417d91a
autoreconf -i
./configure
make -j8
sudo make install
sudo ldconfig
cd ../
exit




