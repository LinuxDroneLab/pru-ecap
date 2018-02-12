#Eseguire dalla BBB dove si dispone degli eseguibili: pru-ecap.out e pru-ecap-arm 
#La BBB deve aver caricato il dts PRU-ECAP-00A0.dts
#echo 'stop' > /sys/class/remoteproc/remoteproc1/state
cp pru-ecap.out /lib/firmware/pru-ecap-fw
echo "pru-ecap-fw" > /sys/class/remoteproc/remoteproc1/firmware
echo 'start' > /sys/class/remoteproc/remoteproc1/state
./pru-ecap-arm
