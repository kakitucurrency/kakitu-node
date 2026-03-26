#!/usr/bin/env sh

set -euf

usage() {
	printf "Usage:\n"
	printf "  $0 kakitu_node [daemon] [cli_options] [-l] [-v size]\n"
	printf "    daemon\n"
	printf "      start as daemon\n\n"
	printf "    cli_options\n"
	printf "      kakitu_node cli options <see kakitu_node --help>\n\n"
	printf "    -l\n"
	printf "      log to console <use docker logs {container}>\n\n"
	printf "    -v<size>\n"
	printf "      vacuum database if over size GB on startup\n\n"
	printf "  $0 sh [other]\n"
	printf "    other\n"
	printf "      sh pass through\n"
	printf "  $0 [*]\n"
	printf "    *\n"
	printf "      usage\n\n"
	printf "default:\n"
	printf "  $0 kakitu_node daemon -l\n"
}

OPTIND=1
command=""
passthrough=""
db_size=0
log_to_cerr=0

if [ $# -lt 2 ]; then
	usage
	exit 1
fi

if [ "$1" = 'kakitu_node' ]; then
	command="${command}kakitu_node"
	shift
	for i in $@; do
		case $i in
		"daemon")
			command="${command} --daemon"
			;;
		*)
			if [ ${#passthrough} -ge 0 ]; then
				passthrough="${passthrough} $i"
			else
				passthrough="$i"
			fi
			;;
		esac
	done
	for i in $passthrough; do
		case $i in
		*"-v"*)
			db_size=$(echo $i | tr -d -c 0-9)
			echo "Vacuum DB if over $db_size GB on startup"
			;;
		"-l")
			echo "log_to_cerr = true"
			command="${command} --config"
			command="${command} node.logging.log_to_cerr=true"
			;;
		*)
			command="${command} $i"
			;;
		esac
	done
elif [ "$1" = 'sh' ]; then
	shift
	command=""
	for i in $@; do
		if [ "$command" = "" ]; then
			command="$i"
		else
			command="${command} $i"
		fi
	done
	printf "EXECUTING: ${command}\n"
	exec $command
else
	usage
	exit 1
fi

network="$(cat /etc/kakitu-network)"
case "${network}" in
live | '')
	network='live'
	dirSuffix=''
	;;
beta)
	dirSuffix='Beta'
	;;
dev)
	dirSuffix='Dev'
	;;
test)
	dirSuffix='Test'
	;;
esac

# Use KAKITU_DATA env var if set, otherwise default to /home/kakitu (Railway volume mount point)
KAKITU_DATA="${KAKITU_DATA:-/home/kakitu}"
kakitudir="${KAKITU_DATA}/Kakitu${dirSuffix}"
dbFile="${kakitudir}/data.ldb"

mkdir -p "${kakitudir}"

if [ ! -f "${kakitudir}/config-node.toml" ] && [ ! -f "${kakitudir}/config.json" ]; then
	echo "Config file not found, adding default."
	cp "/usr/share/kakitu/config/config-node.toml" "${kakitudir}/config-node.toml"
	cp "/usr/share/kakitu/config/config-rpc.toml" "${kakitudir}/config-rpc.toml"
fi

case $command in
*"--daemon"*)
	command="${command} --data_path ${kakitudir}"
	if [ $db_size -ne 0 ]; then
		if [ -f "${dbFile}" ]; then
			dbFileSize="$(stat -c %s "${dbFile}" 2>/dev/null)"
			if [ "${dbFileSize}" -gt $((1024 * 1024 * 1024 * db_size)) ]; then
				echo "ERROR: Database size grew above ${db_size}GB (size = ${dbFileSize})" >&2
				kakitu_node --vacuum --data_path "${kakitudir}"
			fi
		fi
	fi
	;;
esac

printf "EXECUTING: ${command}\n"

# Start kakitu_rpc alongside the node (it connects over IPC to serve HTTP on the RPC port)
case $command in
*"--daemon"*)
	kakitu_rpc --daemon --data_path "${kakitudir}" &
	;;
esac

exec $command
