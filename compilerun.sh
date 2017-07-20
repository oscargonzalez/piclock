#ª/bin/bash
echo "Compiling..."
clear
make 
if [ $? -ne 0 ]
then
    echo " ****************************"
    echo " ******* make failed ********"
    echo " ****************************"
    exit 1
fi
echo "Run!"
chmod 777 piclock
sudo ./piclock


