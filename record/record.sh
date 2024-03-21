#!/bin/bash
set -eEu
set -o pipefail
set -o errtrace

SCRIPTDIR=$(dirname $(readlink -f $0))
FP_DEBUG=0
CURL=/usr/bin/curl

function main
{
    # Get command line options
    local help=0
    local sensor=
    local profile=
    OPTERR=1
    while getopts "hvc:" opt; do
        case $opt in
            h)
                echo "$0 [-v] [-c /path/to/curl] <sensor> <profile>"
                echo
                echo "Where:"
                echo "    <sensor> is the sensor address (IP or hostname)"
                echo "    <profile> is the logging profile ("maximal", "medium", ...)"
                echo
                exit 0
                ;;
            v)
                FP_DEBUG=1
                ;;
            *)
                error "Illegal option '$opt'!" 1>&2
                exit 1
                ;;
        esac
    done
    if [ ${OPTIND} -gt 1 ]; then
        shift $(expr $OPTIND - 1)
    fi
    # Need exactly two remaining arguments
    if [ $# -eq 2 ]; then
        sensor=$1
        profile=$2
    fi

    # Check arguments
    if [ -z "${sensor}" -o -z "${profile}" ]; then
        exit_fail "Bad or missing argument(s)"
    fi

    # We should be good to go now..
    local res=0
    if ! do_record ${sensor} ${profile}; then
        res=1
    fi

    # Happy?
    debug "res=${res}"
    if [ ${res} -eq 0 ]; then
        exit 0
    else
        error "Ouch"
        exit 1
    fi
}

function do_record
{
    local sensor=$1
    local profile=$2

    notice
    notice "***** Recording ${profile} from ${sensor}, press CTRL-C to end logging *****"
    notice

    local lastfpl=$(ls -ltrh *.fpl | tail -n1)

    trap "notice \"Stopping...\"; ${CURL} http://${sensor}/api/v2/record/stop -X POST -d dummy >/dev/null; notice \"Please wait...\"; sleep 5;" INT

    (
        trap '' INT
        ${CURL} -OJ http://${sensor}:21100/start -X POST -H "Content-Type: application/json" \
            -d "{\"target\":\"download\",\"profile\":\"${profile}\"}"
    ) &

    wait

    trap '' INT

    local newfpl=$(ls -ltrh *.fpl | tail -n1)

    if [ "${newfpl}" != "${lastfpl}" ]; then
        notice "Done: ${newfpl}"
        return 0
    else
        return 1
    fi
}

function exit_fail
{
    error "$@"
    echo "Try '$0 -h' for help." 1>&2
    exit 1
}

function notice
{
    echo -e "\033[1;37;40m$@\033[m" 1>&2
}

function info
{
    echo -e "\033[0;40m$@\033[m" 1>&2
}

function warning
{
    echo -e "\033[1;33;40mWarning: $@\033[m" 1>&2
}

function error
{
    echo -e "\033[1;31;40mError: $@\033[m" 1>&2
}

function debug
{
    if [ ${FP_DEBUG} -gt 0 ]; then
        echo -e "\033[0;36;40mDebug: $@\033[m" 1>&2
    fi
}

function panic
{
    local res=$?
    echo -e "\033[1;35;40mPanic at ${BASH_SOURCE[1]}:${BASH_LINENO[0]}! ${BASH_COMMAND} (res=$res)\033[m" 1>&2
    exit $res
}

main "$@"
exit 99
