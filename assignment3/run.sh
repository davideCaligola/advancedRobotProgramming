#! /usr/bin/bash

address=127.0.0.1
port=45678
client=0

function show_usage (){
    echo "Available options"
    echo "-h shows this help"
    echo "-c starts application as client"
    echo "-a for specifying the address to connect to (i.e. 127.0.0.1)"
    echo "-p for specifying the port (i.e 45678)"
    
    return 0
}

while getopts a:p:ch OPT
do
    case "${OPT}"
    in
        a)
            address=${OPTARG};;
        p)
            port=${OPTARG};;
        c)
            client=1
            ;;
        h)
            show_usage
            exit 0;;
        *)
            show_usage
            exit 1;;
    esac
done

if [ $client -eq 1 ]; then
    ./bin/master -a $address -p $port -c
else
    ./bin/master -a $address -p $port
fi

