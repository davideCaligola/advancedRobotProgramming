#! /usr/bin/bash

processTimeout=10
processSamplingTime=1000
processNoise=0.4

function help (){
    echo "Available options"
    echo "-h shows this help"
    echo "-t for watchdog timeout in seconds (minimum 2s, default 10s)"
    echo "-s for application sampling time in milliseconds (minimum 30ms, default 1000ms)"
    echo "-n for the amplitude of the noise around 0 (default 0.4)"
    
    return 0
}

while getopts t:s:n:h OPT
do
    case "${OPT}"
    in
        t)
            processTimeout=${OPTARG};;
        s)
            processSamplingTime=${OPTARG};;
        n)
            processNoise=${OPTARG};;
        h)
            help
            exit 0;;
        *)
            show_usage
            exit 1;;
    esac
done

./bin/master -t $processTimeout -s $processSamplingTime -n $processNoise