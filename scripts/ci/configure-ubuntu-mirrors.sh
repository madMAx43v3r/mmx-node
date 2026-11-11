#!/bin/sh
set -eu

# Handle both Ubuntu's source formats and the GitHub runner's mirror list.
script_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
cp "$script_dir/ubuntu-mirrors.txt" /etc/apt/apt-mirrors.txt
for source in /etc/apt/sources.list /etc/apt/sources.list.d/*.list /etc/apt/sources.list.d/*.sources; do
	[ -f "$source" ] || continue
	sed -E -i \
		-e 's#https?://([[:alnum:].-]+\.)?archive\.ubuntu\.com/ubuntu/?#mirror+file:/etc/apt/apt-mirrors.txt#g' \
		-e 's#https?://security\.ubuntu\.com/ubuntu/?#mirror+file:/etc/apt/apt-mirrors.txt#g' \
		"$source"
done

# Stop on incomplete indexes rather than installing from stale package lists.
cat > /etc/apt/apt.conf.d/80mmx-ci-network <<'EOF'
Acquire::Retries "3";
Acquire::http::Timeout "30";
Acquire::https::Timeout "30";
APT::Update::Error-Mode "any";
EOF
